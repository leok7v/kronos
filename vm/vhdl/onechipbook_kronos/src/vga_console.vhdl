library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

-- VGA text console for the OneChipBook: renders the OS console byte stream
-- (the DL11 TX bytes) as 128x54 characters of 8x14 white-on-black text at
-- 1024x768@60, the board's and the monitor's native resolution.
--
-- WHY 1024x768: the OS's terminal driver hardcodes an 80x50 terminal
-- (sys/drvvm/TT2ANSI.m: lines:=50, columns:=80) and cannot be told otherwise
-- without rebuilding it. 8x14 cells give 128 columns (80 fit with room to
-- spare) and the row count is set to exactly 50 so the OS's bottom line IS our
-- bottom line: it scrolls by addressing row 50 and sending FF, which only lines
-- up if both agree where the bottom is. 50*14 = 700 of the 768 lines are used. (An earlier 512x480 mode driven straight from the 21.5 MHz system
-- clock gave only 64x30: every padded line wrapped and the prompt was clamped
-- onto the bottom line, effectively invisible.)
--
-- WHY a separate pixel clock: 1024x768@60 needs ~65 MHz, so the console runs on
-- a PLL output while the rest of the design stays on the 21.47727 MHz
-- oscillator. The PLL synthesises only the VIDEO clock -- the system and SDRAM
-- clocks are untouched, which matters because the SDRAM read-capture window on
-- this board was tuned by hand and is narrow.
--
-- The console byte stream therefore crosses clock domains. It is crossed with a
-- toggle plus a two-flop synchroniser: the sender flips ch_tog once per byte,
-- and ch is stable long before the toggle is observed (bytes arrive ~174 us
-- apart at 57600 baud, thousands of cycles), so no FIFO is needed.
--
-- Timing (1024x768@60 at the 64.43 MHz the Cyclone I PLL can actually make --
-- see video_pll.vhdl; blanking is trimmed to keep the scan rates on spec):
--   H: 1024 visible +  8 front + 136 sync + 160 back = 1328  -> 48.52 kHz
--                                                              (spec 48.36)
-- The BACK porch is held at the spec 160 and the front porch shortened
-- instead: a monitor starts drawing a fixed time after the sync edge, so a
-- short back porch slides the whole image left and the first columns fall off
-- the panel. Blanking totals 304 rather than the spec's 320 because H_TOT is
-- set by the refresh arithmetic at 64.43 MHz.
--   V:  756 visible + 15 front +   6 sync +  31 back =  808  -> 60.05 Hz
-- Both syncs are NEGATIVE. The horizontal counter starts 3 pixels before the
-- visible area so the fetch pipeline (text RAM -> font ROM, one registered
-- cycle each) is primed, and because the glyph byte is REGISTERED into the
-- shifter the first pixel of column k only emerges at fx = 8k+3.

entity vga_console is
    generic (
        COLS : integer := 128;      -- 128*8 = 1024
        ROWS : integer := 50);      -- match the OS's 50 lines exactly
    port (
        pclk   : in  std_logic;                     -- 65 MHz pixel clock
        reset  : in  std_logic;
        ch     : in  std_logic_vector(7 downto 0);  -- console byte (other domain)
        ch_tog : in  std_logic;                     -- toggles once per byte
        hsync  : out std_logic;
        vsync  : out std_logic;
        r      : out std_logic_vector(5 downto 0);
        g      : out std_logic_vector(5 downto 0);
        b      : out std_logic_vector(5 downto 0));
end vga_console;

architecture Behaviour of vga_console is

    constant CELL_H  : integer := 14;
    constant STRIDE  : integer := 128;              -- text RAM row pitch = COLS

    constant H_VIS_S : integer := 3;
    constant H_VIS_E : integer := 3 + 1024;         -- 1027
    constant H_SYN_S : integer := 1035;
    constant H_SYN_E : integer := 1171;
    constant H_TOT   : integer := 1328;
    constant V_VIS_E : integer := ROWS * CELL_H;    -- 756
    constant V_SYN_S : integer := 771;
    constant V_SYN_E : integer := 777;
    constant V_TOT   : integer := 808;

    signal fx      : std_logic_vector(10 downto 0) := (others => '0');  -- 0..1343
    signal vy      : std_logic_vector(10 downto 0) := (others => '0');  -- 0..805
    signal cell_y  : integer range 0 to CELL_H - 1 := 0;
    signal row_s   : integer range 0 to 63 := 0;    -- screen row
    signal frame   : std_logic_vector(5 downto 0) := (others => '0');

    -- 9 bits per cell: bit 8 is the REVERSE-VIDEO attribute, bits 7..0 the
    -- character. An M4K has a native x9 mode, so the attribute costs no extra
    -- memory blocks at all.
    type text_ram_t is array(0 to ROWS * STRIDE - 1) of std_logic_vector(8 downto 0);
    signal tram    : text_ram_t;
    signal char_q  : std_logic_vector(8 downto 0);
    signal rd_addr : integer range 0 to ROWS * STRIDE - 1 := 0;
    signal wr_addr : integer range 0 to ROWS * STRIDE - 1 := 0;
    signal wr_data : std_logic_vector(8 downto 0);
    signal rev     : std_logic := '0';   -- current attribute, set by ESC[1r
    signal rev_d   : std_logic := '0';   -- attribute of the cell being shown
    signal wr_en   : std_logic;

    signal font_addr : std_logic_vector(11 downto 0);
    signal font_q    : std_logic_vector(7 downto 0);
    signal shifter   : std_logic_vector(7 downto 0) := (others => '0');
    signal col_d     : integer range 0 to COLS - 1 := 0;   -- column being SHOWN

    -- circular scroll window: screen row s shows physical row
    -- (top_row + s) mod ROWS, so a scroll is one increment plus blanking the
    -- row that rotates in -- never a full-screen copy.
    -- ONE modulus, ROWS, everywhere: cursor addressing, display and scroll must
    -- agree or written text lands on rows that are not the ones displayed.
    -- (The OS's driver is built for exactly our height: TT2ANSI sets lines:=50.)
    signal top_row   : integer range 0 to ROWS - 1 := 0;
    signal cur_row   : integer range 0 to ROWS - 1 := 0;   -- PHYSICAL row
    signal cur_col   : integer range 0 to COLS - 1 := 0;
    signal phys_row  : integer range 0 to ROWS - 1;
    signal cur_scr   : integer range 0 to ROWS - 1;

    -- ANSI/CSI parser: the OS drives the console with escape sequences, which
    -- would otherwise print as literal "[50H" litter.
    type estate_t is (E_NONE, E_ESC, E_CSI);
    signal estate    : estate_t := E_NONE;
    signal p0, p1    : integer range 0 to 255 := 0;
    signal pn        : integer range 0 to 1 := 0;

    -- SHIFT implements ESC[P (delete char) and ESC[@ (insert char), which the
    -- OS uses for ALL in-place line editing -- backspace at any prompt is
    -- ESC[D followed by ESC[P. Without them the OS erased its own buffer while
    -- the screen kept the characters, so a mistyped username stayed on screen
    -- and could not be corrected.
    --
    -- Both need a read-modify-write across a row, and tram has ONE read port,
    -- busy with video scanout every pixel clock. A second port would infer a
    -- duplicated RAM and M4K is at 52/52, so the read port is TIME-SHARED: the
    -- shift steals it for two cycles per column. 128 columns is ~4us at
    -- 64.43MHz -- a fraction of one frame, invisible.
    type wstate_t is (INIT, IDLE, CLEAR, ERASE, ESCR, SHIFT);
    signal wstate    : wstate_t := INIT;
    signal clr_addr  : integer range 0 to ROWS * STRIDE - 1 := 0;
    signal clr_col   : integer range 0 to 255 := 0;
    signal clr_row   : integer range 0 to ROWS - 1 := 0;
    signal erase_end : integer range 0 to 255 := 0;
    -- ESC J walks LOGICAL rows, so it must map through the scroll window too
    signal escr_phys : integer range 0 to ROWS - 1;

    -- character shift engine (ESC[P / ESC[@)
    signal sh_col    : integer range 0 to 255 := 0;   -- destination column
    signal sh_n      : integer range 1 to 255 := 1;   -- how many to shift by
    signal sh_left   : std_logic := '0';              -- '1' = delete, '0' = insert
    signal sh_phase  : std_logic := '0';              -- 0 = address, 1 = write
    signal shifting  : std_logic;
    -- UNRANGED on purpose. This is a concurrent assignment, so it is evaluated
    -- every cycle whether or not a shift is running, and with the idle values
    -- (sh_col=0, sh_n=1, insert direction) it computes -1. Constrained to
    -- 0..ROWS*STRIDE-1 that is a bound-check failure in simulation and, far
    -- worse, a SILENT WRAP in synthesis -- an out-of-range address quietly
    -- corrupting a cell. The guard on rd_addr below is what keeps it legal.
    signal sh_src    : integer := 0;

    -- byte-stream CDC + a 1-deep holding slot for a byte that lands while a row
    -- is being blanked (that takes COLS cycles, versus ~174 us between bytes)
    signal tog_s     : std_logic_vector(2 downto 0) := (others => '0');
    signal ch_stb    : std_logic;
    signal pend      : std_logic_vector(7 downto 0) := (others => '0');
    signal pend_v    : std_logic := '0';
    signal ch_in     : std_logic_vector(7 downto 0);
    signal ch_go     : std_logic;

    signal h_active, v_active, active : std_logic;
    signal pix, cursor_on : std_logic;

begin

    -- ---------------------------------------------------------------- timing
    process (pclk)
    begin
        if rising_edge(pclk) then
            if reset = '1' then
                fx <= (others => '0'); vy <= (others => '0');
                cell_y <= 0; row_s <= 0;
            elsif fx = H_TOT - 1 then
                fx <= (others => '0');
                if vy = V_TOT - 1 then
                    vy <= (others => '0');
                    cell_y <= 0; row_s <= 0;
                    frame <= frame + 1;
                else
                    vy <= vy + 1;
                    if cell_y = CELL_H - 1 then
                        cell_y <= 0;
                        if row_s < 63 then row_s <= row_s + 1; end if;
                    else
                        cell_y <= cell_y + 1;
                    end if;
                end if;
            else
                fx <= fx + 1;
            end if;
        end if;
    end process;

    hsync <= '0' when (fx >= H_SYN_S and fx < H_SYN_E) else '1';   -- negative
    vsync <= '0' when (vy >= V_SYN_S and vy < V_SYN_E) else '1';   -- negative

    h_active <= '1' when (fx >= H_VIS_S and fx < H_VIS_E) else '0';
    v_active <= '1' when vy < V_VIS_E else '0';
    active   <= h_active and v_active;

    -- ------------------------------------------------------ fetch pipeline
    -- inside the OS's window the rows rotate; below it they map straight
    -- through (those lines are outside the scroll region and stay put)
    -- row_s keeps counting through vertical blanking (past ROWS), so clamp
    -- first: that address is never displayed, but it still indexes the RAM.
    phys_row <= 0                            when row_s >= ROWS  else
                row_s + top_row              when (row_s + top_row) < ROWS else
                (row_s + top_row) - ROWS;

    -- fx(9 downto 3) is the column being FETCHED. Taking only 7 bits makes it
    -- wrap past column 127, which keeps the address inside the row's slots;
    -- those pixels are blanked anyway. The DISPLAYED column is registered
    -- separately at load time, so the cursor never re-derives it from fx --
    -- doing that previously dropped a high bit and drew a second cursor at the
    -- opposite edge of the screen.
    -- The shift borrows the read port. Video shows one stale cell for the few
    -- microseconds this runs, which is well under a frame and never visible.
    shifting <= '1' when wstate = SHIFT else '0';
    sh_src   <= cur_row * STRIDE + sh_col + sh_n when sh_left = '1'
                else cur_row * STRIDE + sh_col - sh_n;
    rd_addr  <= sh_src when shifting = '1' and sh_src >= 0
                                          and sh_src < ROWS * STRIDE
                else phys_row * STRIDE + conv_integer(fx(9 downto 3));

    -- fold anything outside 0x20..0x7E to a space so the ROM is never indexed
    -- outside its 95 glyphs
    -- The ROM pads every glyph to 16 rows, so this is a CONCATENATION, not
    -- (char-32)*14 + row. That multiply sat between the text RAM and the font
    -- ROM and cost ~0.9 ns of setup at 64.43 MHz -- the only path that failed
    -- timing when the console moved to the pixel clock.
    -- The ROM now covers 0x20..0xFF, because the OS draws its window frames
    -- with a custom HIGH-RANGE pseudographic set (TT2ANSI.m: HBAR 0xA4, VBAR
    -- 0xB3 and a 3x3 BARS array of corners and tees). Folding those to a space
    -- is what rendered the file manager's panels as blank space.
    -- Only 0x00..0x1F (control codes) fold to a space now.
    font_addr <= (char_q(7 downto 0) - 32) & conv_std_logic_vector(cell_y, 4)
                 when char_q(7 downto 0) >= x"20"
                 else x"00" & conv_std_logic_vector(cell_y, 4);

    process (pclk)
    begin
        if rising_edge(pclk) then
            if wr_en = '1' then
                tram(wr_addr) <= wr_data;
            end if;
            -- read-during-write on one address can only mis-colour a single
            -- character for a single frame
            char_q <= tram(rd_addr);
        end if;
    end process;

    font : entity work.font_rom
        port map (clk => pclk, addr => font_addr, data => font_q);

    process (pclk)
    begin
        if rising_edge(pclk) then
            if fx(2 downto 0) = "010" then
                shifter <= font_q;
                rev_d   <= char_q(8);
                col_d   <= conv_integer(fx(9 downto 3));
            else
                shifter <= shifter(6 downto 0) & '0';
            end if;
        end if;
    end process;

    cursor_on <= '1' when frame(5) = '1' and phys_row = cur_row and
                          col_d = cur_col and cell_y >= CELL_H - 2
                 else '0';

    pix <= ((shifter(7) xor rev_d) or cursor_on) and active;

    r <= (others => pix);
    g <= (others => pix);
    b <= (others => pix);

    -- ------------------------------------------------------- console writer
    -- CDC: one edge of ch_tog per byte, synchronised into the pixel domain
    process (pclk)
    begin
        if rising_edge(pclk) then
            tog_s <= tog_s(1 downto 0) & ch_tog;
        end if;
    end process;
    ch_stb <= tog_s(2) xor tog_s(1);

    ch_in <= pend when pend_v = '1' else ch;
    ch_go <= '1' when (wstate = IDLE and (ch_stb = '1' or pend_v = '1')) else '0';

    cur_scr <= cur_row - top_row when cur_row >= top_row
               else cur_row - top_row + ROWS;

    escr_phys <= clr_row + top_row when (clr_row + top_row) < ROWS
                 else (clr_row + top_row) - ROWS;

    process (pclk)
        variable nrow : integer range 0 to ROWS - 1;
        variable nl   : boolean;
        variable scr  : integer range 0 to 255;
        variable col  : integer range 0 to 255;
    begin
        if rising_edge(pclk) then
            if reset = '1' then
                wstate <= INIT; clr_addr <= 0; rev <= '0';
                top_row <= 0; cur_row <= 0; cur_col <= 0;
                pend_v <= '0'; wr_en <= '0'; estate <= E_NONE;
            else
                wr_en <= '0';
                if ch_stb = '1' and wstate /= IDLE then
                    pend <= ch; pend_v <= '1';
                end if;

                case wstate is
                    when INIT =>
                        wr_en <= '1'; wr_addr <= clr_addr; wr_data <= '0' & x"20";
                        if clr_addr = ROWS * STRIDE - 1 then
                            wstate <= IDLE;
                        else
                            clr_addr <= clr_addr + 1;
                        end if;

                    when ESCR =>
                        -- cursor row from cur_col, then whole rows below it, in
                        -- LOGICAL order (escr_phys maps through the window)
                        wr_en <= '1';
                        wr_addr <= escr_phys * STRIDE + clr_col;
                        wr_data <= '0' & x"20";
                        if clr_col = COLS - 1 then
                            clr_col <= 0;
                            if clr_row = ROWS - 1 then
                                wstate <= IDLE;
                            else
                                clr_row <= clr_row + 1;
                            end if;
                        else
                            clr_col <= clr_col + 1;
                        end if;

                    when CLEAR =>
                        wr_en <= '1';
                        wr_addr <= clr_row * STRIDE + clr_col;
                        wr_data <= '0' & x"20";
                        if clr_col = COLS - 1 then
                            wstate <= IDLE;
                        else
                            clr_col <= clr_col + 1;
                        end if;

                    when SHIFT =>
                        -- phase 0 presents sh_src (combinational, above) and
                        -- waits one cycle for tram's registered output; phase 1
                        -- writes what came back to the destination column.
                        if sh_phase = '0' then
                            sh_phase <= '1';
                        else
                            sh_phase <= '0';
                            wr_en   <= '1';
                            wr_addr <= cur_row * STRIDE + sh_col;
                            wr_data <= char_q;
                            if sh_left = '1' then
                                -- delete: pull left, then blank the tail
                                if sh_col + sh_n >= COLS - 1 then
                                    clr_col   <= COLS - sh_n;
                                    erase_end <= COLS;
                                    wstate    <= ERASE;
                                else
                                    sh_col <= sh_col + 1;
                                end if;
                            else
                                -- insert: push right (must run right-to-left,
                                -- or each cell would overwrite its own source),
                                -- then blank the gap at the cursor
                                if sh_col <= cur_col + sh_n then
                                    clr_col   <= cur_col;
                                    erase_end <= cur_col + sh_n;
                                    wstate    <= ERASE;
                                else
                                    sh_col <= sh_col - 1;
                                end if;
                            end if;
                        end if;

                    when ERASE =>
                        wr_en <= '1';
                        wr_addr <= cur_row * STRIDE + clr_col;
                        wr_data <= '0' & x"20";
                        if clr_col + 1 >= erase_end or clr_col = COLS - 1 then
                            wstate <= IDLE;
                        else
                            clr_col <= clr_col + 1;
                        end if;

                    when IDLE =>
                        if ch_go = '1' then
                            pend_v <= '0';
                            nl := false;
                            if estate = E_ESC then
                                if ch_in = x"5B" then              -- '['
                                    estate <= E_CSI; p0 <= 0; p1 <= 0; pn <= 0;
                                elsif ch_in = x"4A" then           -- ESC J erase
                                    -- VT52 ESC J erases from the CURSOR to the
                                    -- end of the screen, and the OS means it:
                                    -- defTerminal.d documents _erase 0 as
                                    -- "к концу", and every full-clear site homes
                                    -- first (tty.home; tty.erase(0)). Treating it
                                    -- as a full wipe blanked the editor on every
                                    -- page down -- ex's rollup() clears its info
                                    -- line with set_pos(bottom+1,0)+erase(0) once
                                    -- per scrolled line, which took the text with
                                    -- it. The cursor must NOT move.
                                    estate <= E_NONE;
                                    clr_row <= cur_scr; clr_col <= cur_col;
                                    wstate <= ESCR;
                                else
                                    estate <= E_NONE;              -- ESC=, ...
                                end if;
                            elsif estate = E_CSI then
                                if ch_in >= x"30" and ch_in <= x"39" then
                                    if pn = 0 then
                                        p0 <= (p0 * 10 + conv_integer(ch_in(3 downto 0))) mod 256;
                                    else
                                        p1 <= (p1 * 10 + conv_integer(ch_in(3 downto 0))) mod 256;
                                    end if;
                                elsif ch_in = x"3B" then           -- ';'
                                    pn <= 1;
                                else
                                    estate <= E_NONE;
                                    case ch_in is
                                        when x"48" =>              -- 'H' position (1-based)
                                            scr := 0;
                                            if p0 > 0 then scr := p0 - 1; end if;
                                            if scr > ROWS - 1 then scr := ROWS - 1; end if;
                                            col := 0;
                                            if p1 > 0 then col := p1 - 1; end if;
                                            if col > COLS - 1 then col := COLS - 1; end if;
                                            if scr + top_row < ROWS then
                                                cur_row <= scr + top_row;
                                            else
                                                cur_row <= scr + top_row - ROWS;
                                            end if;
                                            cur_col <= col;
                                        when x"41" =>              -- 'A' up
                                            if cur_scr > 0 then
                                                if cur_row = 0 then cur_row <= ROWS - 1;
                                                else cur_row <= cur_row - 1; end if;
                                            end if;
                                        when x"42" =>              -- 'B' down
                                            if cur_scr < ROWS - 1 then
                                                if cur_row = ROWS - 1 then cur_row <= 0;
                                                else cur_row <= cur_row + 1; end if;
                                            end if;
                                        when x"43" =>              -- 'C' right
                                            if cur_col < COLS - 1 then cur_col <= cur_col + 1; end if;
                                        when x"44" =>              -- 'D' left
                                            if cur_col > 0 then cur_col <= cur_col - 1; end if;
                                        when x"50" =>              -- 'P' delete n chars
                                            -- ESC[D ESC[P is how the OS does a
                                            -- BACKSPACE. Ignoring it was why
                                            -- deleted characters stayed on
                                            -- screen while the OS believed its
                                            -- buffer was clear.
                                            if p0 = 0 then sh_n <= 1;
                                            else sh_n <= p0; end if;
                                            sh_left  <= '1';
                                            sh_col   <= cur_col;
                                            sh_phase <= '0';
                                            wstate   <= SHIFT;
                                        when x"40" =>              -- '@' insert n chars
                                            -- right-to-left, so start at the
                                            -- last column and walk back
                                            if p0 = 0 then sh_n <= 1;
                                            else sh_n <= p0; end if;
                                            sh_left  <= '0';
                                            sh_col   <= COLS - 1;
                                            sh_phase <= '0';
                                            wstate   <= SHIFT;
                                        when x"4B" =>              -- 'K' erase to EOL
                                            clr_col <= cur_col;
                                            erase_end <= COLS;
                                            wstate <= ERASE;
                                        when x"58" =>              -- 'X' erase n chars
                                            clr_col <= cur_col;
                                            if p0 = 0 then erase_end <= cur_col + 1;
                                            else erase_end <= cur_col + p0; end if;
                                            wstate <= ERASE;
                                        when x"72" =>              -- 'r' reverse video
                                            -- ESC[1r on, ESC[0r off. The file
                                            -- manager marks its selected line
                                            -- this way; ignoring it left the
                                            -- cursor position invisible.
                                            if p0 = 0 then rev <= '0';
                                            else rev <= '1'; end if;
                                        when x"54" =>              -- 'T' roll down
                                            -- The mirror of roll-up, and ex's
                                            -- PageUp depends on it: rolldw()
                                            -- tries _scroll_down (the driver
                                            -- has it commented out -> inv_op)
                                            -- and falls back to _roll_down =
                                            -- ESC[1T. Ignoring it meant the
                                            -- text never moved while the editor
                                            -- believed it had. Rotate the
                                            -- window back and blank the row
                                            -- that turns up at the top.
                                            if top_row = 0 then
                                                top_row <= ROWS - 1;
                                                clr_row <= ROWS - 1;
                                            else
                                                top_row <= top_row - 1;
                                                clr_row <= top_row - 1;
                                            end if;
                                            clr_col <= 0;
                                            wstate  <= CLEAR;
                                        when others =>
                                            null;                  -- @ P L M u
                                    end case;
                                end if;
                            elsif ch_in = x"1B" then               -- ESC
                                estate <= E_ESC;
                            elsif ch_in = x"0D" then               -- CR
                                cur_col <= 0;
                            elsif ch_in = x"0A" then               -- LF
                                nl := true;
                            elsif ch_in = x"08" then               -- BS
                                if cur_col > 0 then cur_col <= cur_col - 1; end if;
                            elsif ch_in = x"0C" then               -- FF = roll up
                                -- The OS scrolls ONLY like this: TT2ANSI's
                                -- _roll_up emits ESC[50;1H then FF. Ignoring FF
                                -- meant nothing ever scrolled and the OS simply
                                -- overwrote the same lines. Advancing the
                                -- circular window rotates the old top row to the
                                -- bottom, so blank it and land the cursor there.
                                if top_row + 1 >= ROWS then top_row <= 0;
                                else top_row <= top_row + 1; end if;
                                cur_row <= top_row;
                                cur_col <= 0;
                                clr_row <= top_row;
                                clr_col <= 0;
                                wstate  <= CLEAR;
                            elsif ch_in >= x"20" then   -- printable + pseudographics
                                wr_en   <= '1';
                                wr_addr <= cur_row * STRIDE + cur_col;
                                wr_data <= rev & ch_in;
                                if cur_col = COLS - 1 then
                                    cur_col <= 0; nl := true;
                                else
                                    cur_col <= cur_col + 1;
                                end if;
                            end if;

                            if nl then
                                if cur_row + 1 >= ROWS then nrow := 0;
                                else nrow := cur_row + 1; end if;
                                cur_row <= nrow;
                                cur_col <= 0;
                                if nrow = top_row then      -- wrapped onto the top line
                                    if top_row + 1 >= ROWS then top_row <= 0;
                                    else top_row <= top_row + 1; end if;
                                    clr_row <= nrow;
                                    clr_col <= 0;
                                    wstate  <= CLEAR;
                                end if;
                            end if;
                        end if;
                end case;
            end if;
        end if;
    end process;

end architecture Behaviour;

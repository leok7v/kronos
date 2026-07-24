library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
use std.textio.all;

-- Testbench for vga_console. Two independent checks:
--   1. TIMING -- measures the real line/frame periods and sync widths off the
--      pins and compares them against the VGA 640x480@60 spec. A monitor locks
--      on sync width + line rate, so those are what must be right.
--   2. RENDERING -- reconstructs the pixel stream back into ASCII art and
--      prints it, so the actual glyphs can be READ in the log. This is the only
--      way to be sure the text RAM, the circular scroll window, the font ROM
--      and the 2-stage fetch pipeline all line up; a timing-only check would
--      happily pass with a blank or one-pixel-skewed screen.
-- The tb derives its position from the sync pins alone (counting clocks since
-- hsync, lines since vsync), so vga_console needs no debug ports.

entity tb_vga_console is
end tb_vga_console;

architecture sim of tb_vga_console is
    constant PERIOD : time := 15521 ps;      -- 64.43 MHz pixel clock

    signal clk    : std_logic := '0';
    signal reset  : std_logic := '1';
    signal ch     : std_logic_vector(7 downto 0) := (others => '0');
    signal ch_tog : std_logic := '0';
    signal hsync, vsync : std_logic;
    signal r, g, b : std_logic_vector(5 downto 0);
    signal done   : boolean := false;
begin
    clk <= not clk after PERIOD / 2 when not done else '0';

    dut : entity work.vga_console
        port map (pclk => clk, reset => reset, ch => ch, ch_tog => ch_tog,
                  hsync => hsync, vsync => vsync, r => r, g => g, b => b);

    -- feed a console string, including a newline and a wrap-inducing run
    stim : process
        procedure put(s : string) is
        begin
            for i in s'range loop
                wait until rising_edge(clk);
                ch <= conv_std_logic_vector(character'pos(s(i)), 8);
                wait until rising_edge(clk);
                ch_tog <= not ch_tog;          -- one toggle per byte
                -- A toggle CDC needs the level held long enough for the
                -- receiving domain's synchroniser to sample it; back-to-back
                -- toggles are simply missed. Real console bytes are ~174 us
                -- apart at 57600 baud (thousands of cycles), so pace the
                -- testbench rather than the hardware.
                for k in 1 to 24 loop wait until rising_edge(clk); end loop;
            end loop;
        end procedure;
    begin
        wait for PERIOD * 20;
        reset <= '0';
        wait for PERIOD * 8000;          -- INIT now blanks ROWS*STRIDE = 6912 cells
        -- EX PAGE-DOWN TEST -- replays the exact byte stream the editor emits.
        -- ex scrolls a page one line at a time through exScreen.rollup(), and
        -- EVERY rollup starts by clearing its status line:
        --     pushandclearinfo:  set_pos(bottom+1,0)  then  erase(0) -> ESC J
        -- ESC J is "erase from the cursor to the END OF SCREEN" (defTerminal.d
        -- documents _erase 0 as "к концу"), so from the status line it must
        -- touch that line ONLY. Implementing it as a full-screen wipe blanked
        -- the whole text area on every scrolled line -- a blank page.
        put(string'(1 => character'val(27))); put("[1;1H"); put("AAA");
        put(string'(1 => character'val(27))); put("[2;1H"); put("BBB");
        put(string'(1 => character'val(27))); put("[3;1H"); put("CCC");
        put(string'(1 => character'val(27))); put("[5;1H"); put("ZZZ");
        -- ---- one ex rollup(), byte for byte
        put(string'(1 => character'val(27))); put("[50;1H");  -- to the info line
        put(string'(1 => character'val(27))); put("J");       -- Clear: info line ONLY
        put(string'(1 => character'val(27))); put("[50;1H");  -- roll_up, part 1
        put(string'(1 => character'val(10)));                 -- roll_up, part 2: scroll
        put(string'(1 => character'val(27))); put("[49;1H");  -- pos(bottom,0)
        put("NEW");                                           -- the line rolled in
        -- ---- now one ex rolldw(), i.e. PAGE UP. rolldw() asks for
        -- _scroll_down (commented out in the driver -> inv_op) and falls back
        -- to _roll_down = ESC[1T. Ignoring that left the text standing still
        -- while the editor believed it had scrolled.
        put(string'(1 => character'val(27))); put("[1T");
        wait;
    end process;

    -- ---------------------------------------------------------------- checks
    check : process
        variable t_hs, t_hs_prev, t_vs, t_vs_prev : time := 0 ns;
        variable hs_lo : time := 0 ns;
        variable line_p, frame_p, hs_w : time := 0 ns;
        variable c    : integer := 0;    -- clocks since hsync fell
        variable L    : integer := 0;    -- lines since vsync fell
        variable vy, fx : integer;
        variable art  : string(1 to 520);
        variable ln   : line;
        variable measured : boolean := false;
        type litv_t is array(0 to 4) of boolean;
        variable lit_r : litv_t := (others => false);   -- which rows hold text
    begin
        -- ---- measure timing over a couple of frames
        wait until falling_edge(hsync);
        t_hs_prev := now;
        wait until rising_edge(hsync);
        hs_w := now - t_hs_prev;
        wait until falling_edge(hsync);
        line_p := now - t_hs_prev;

        wait until falling_edge(vsync);
        t_vs_prev := now;
        wait until falling_edge(vsync);
        frame_p := now - t_vs_prev;

        report "VGA TIMING  line=" & time'image(line_p) &
               "  hsync_width=" & time'image(hs_w) &
               "  frame=" & time'image(frame_p) severity note;
        -- NB: compute vfreq from microseconds -- 1e12 overflows VHDL's 32-bit integer
        report "VGA RATES   hfreq=" & integer'image(1000000000 / (line_p / 1 ns)) &
               " Hz (spec 48518)   vfreq_mHz=" &
               integer'image(1000000000 / (frame_p / 1 us)) &
               " (spec 59940)" severity note;

        -- VESA 1024x768@60: 20.68 us line, 2.09 us hsync, 16.67 ms frame
        assert line_p > 20300 ns and line_p < 20900 ns
            report "FAIL: line period out of 1024x768 range" severity failure;
        assert hs_w > 2000 ns and hs_w < 2300 ns
            report "FAIL: hsync width out of 1024x768 range" severity failure;
        assert frame_p > 16400 us and frame_p < 16950 us
            report "FAIL: frame period out of 1024x768 range" severity failure;

        -- ---- reconstruct the first text row (16 scanlines) as ASCII art.
        -- hsync falls at fx=1051 of 1344 and vsync at vy=771 of 806, so
        -- counting from those edges gives absolute position:
        --   fx = (1035 + c) mod 1328  ->  fx = c - 293 across the visible area
        --   vy = (771 + L) mod 808    ->  vy = L - 37
        wait until falling_edge(vsync);
        L := 0;
        loop
            wait until falling_edge(hsync);
            L := L + 1;
            vy := L - 37;   -- vsync falls at vy=771 of 808
            exit when vy >= 70;
            if vy >= 0 then
                art := (others => ' ');
                c := 0;
                -- Sample MID-pixel, on the falling edge: hsync falls just after
                -- the edge where fx became 530, so the first falling edge after
                -- it sits inside pixel 530, and pixel n is fx = 530+n-682.
                -- Sampling on the rising edge instead reads the value from the
                -- pixel BEFORE the edge and reports the image a pixel skewed.
                for k in 0 to 700 loop
                    wait until falling_edge(clk);
                    fx := c - 293;
                    if fx >= 0 and fx < 520 then
                        if r(0) = '1' then art(fx + 1) := '#'; else art(fx + 1) := '.'; end if;
                    end if;
                    c := c + 1;
                    exit when fx >= 520;
                end loop;
                for k in 1 to 60 loop
                    if art(k) = '#' then lit_r(vy / 14) := true; end if;
                end loop;
                write(ln, string'("ROW") & integer'image(vy / 14) &
                          string'("|") & art(1 to 60));
                writeline(std.textio.output, ln);
            end if;
        end loop;

        -- AAA BBB CCC on rows 0,1,2 and ZZZ on row 4; rollup moved them up one
        -- (AAA off the top), then rolldw moved them back down one. So:
        --   row 0 blank (rolled in), 1 BBB, 2 CCC, 3 blank, 4 ZZZ
        assert lit_r(1) or lit_r(2) or lit_r(4)
            report "FAIL: text area is BLANK -- ESC J erased the whole screen "
                 & "instead of cursor-to-end. This is the editor's blank page "
                 & "on page-down."
            severity failure;
        assert lit_r(1) and lit_r(2) and lit_r(4)
            report "FAIL: rows lost across the rollup/rolldw pair"
            severity failure;
        assert not lit_r(0) and not lit_r(3)
            report "FAIL: ESC[1T did not roll the window down -- ex's PageUp "
                 & "would leave the text standing still"
            severity failure;

        report "*** PASS: VGA timing within spec; text SURVIVED an ex page-down "
             & "rollup and came back to the right rows after a page-up ***"
             severity note;
        done <= true;
        wait;
    end process;
end architecture sim;

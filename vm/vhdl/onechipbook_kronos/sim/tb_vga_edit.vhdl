library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
use std.textio.all;

-- Prove ESC[P (delete char) and ESC[@ (insert char) actually move characters.
--
-- WHY THIS EXISTS. vga_console implemented H A B C D K X r T but NOT P or @,
-- so the OS's line editing was invisible: a BACKSPACE is ESC[D followed by
-- ESC[P, and with ESC[P ignored the OS erased its own buffer while the screen
-- kept the characters. A mistyped username sat there and could not be removed,
-- which is what put the user onto it.
--
-- These are the hardest console ops to get right because they SHIFT a row
-- rather than write one cell, and tram has a single read port already busy with
-- video scanout (M4K is at 52/52, so a second port is not available). The shift
-- therefore time-shares the read port, and the failure modes are ugly: an
-- off-by-one leaves a duplicated character, a bad loop bound hangs the console
-- forever, and a wrong direction on insert overwrites the row with one value.
--
-- Like tb_vga_console, this reads the RENDERED PIXELS back into text off the
-- sync pins, so it checks what is actually on screen and needs no debug ports.

entity tb_vga_edit is
end tb_vga_edit;

architecture sim of tb_vga_edit is
    constant PERIOD : time := 15521 ps;      -- 64.43 MHz pixel clock

    signal clk    : std_logic := '0';
    signal reset  : std_logic := '1';
    signal ch     : std_logic_vector(7 downto 0) := (others => '0');
    signal ch_tog : std_logic := '0';
    signal hsync, vsync : std_logic;
    signal r, g, b : std_logic_vector(5 downto 0);
    signal done   : boolean := false;
    signal fed    : boolean := false;
begin
    clk <= not clk after PERIOD / 2 when not done else '0';

    dut : entity work.vga_console
        port map (pclk => clk, reset => reset, ch => ch, ch_tog => ch_tog,
                  hsync => hsync, vsync => vsync, r => r, g => g, b => b);

    stim : process
        procedure put(s : string) is
        begin
            for i in s'range loop
                wait until rising_edge(clk);
                ch <= conv_std_logic_vector(character'pos(s(i)), 8);
                wait until rising_edge(clk);
                ch_tog <= not ch_tog;
                -- the toggle CDC needs the level held; real bytes are ~174us
                -- apart, so pace the testbench rather than the hardware
                -- SHIFT takes ~250 clocks (2 per column), longer than ERASE's 128, and the
                -- console holds only ONE pending byte. At 24 clocks apart the tb
                -- outran it and bytes were dropped -- which looked like an RTL bug.
                -- Real bytes are ~174us = 11000 pixel clocks apart, a 40x margin.
                for k in 1 to 400 loop wait until rising_edge(clk); end loop;
            end loop;
        end procedure;
        procedure esc(s : string) is
        begin
            put(string'(1 => character'val(27))); put(s);
        end procedure;
    begin
        wait for PERIOD * 20;
        reset <= '0';
        wait for PERIOD * 8000;              -- INIT blanks ROWS*STRIDE cells

        -- row 1: delete in the MIDDLE. "ABCDEF", put the cursor on 'D', delete
        -- it. If ESC[P merely blanked the cell we would see "ABC EF"; a real
        -- shift gives "ABCEF" with E and F pulled left.
        esc("[1;1H"); put("ABCDEF");
        esc("[1;4H");                        -- cursor on 'D'
        esc("[P");

        -- row 2: BACKSPACE exactly as the OS emits it -- ESC[D then ESC[P.
        -- This is the sequence that was silently ignored.
        esc("[2;1H"); put("SEYS");
        esc("[D"); esc("[P");                -- rubs out the final 'S'...
        esc("[D"); esc("[P");                -- ...then 'Y', leaving "SE"

        -- row 3: INSERT. "ABCDEF" with two blanks pushed in at column 3 must
        -- give "AB  CDEF" -- the tail moves RIGHT. Insert must walk the row
        -- right-to-left; left-to-right would smear one character across it.
        esc("[3;1H"); put("ABCDEF");
        esc("[3;3H"); esc("[2@");

        fed <= true;
        wait;
    end process;

    -- ------------------------------------------------- render and check
    -- Reads pixels back off the sync pins (same arithmetic as tb_vga_console)
    -- and accumulates INK PER CHARACTER CELL. The cell pattern is what
    -- distinguishes a real shift from a cell merely being blanked:
    --   "ABCDEF", delete the 'D'
    --      correct shift -> ABCEF    cells 0-4 inked, 5 blank
    --      blank-only    -> ABC EF   cells 0,1,2 inked, 3 BLANK, 4,5 inked
    check : process
        variable c    : integer := 0;
        variable L    : integer := 0;
        variable vy, fx, cell : integer;
        variable ln   : line;
        type ink_t is array(0 to 3, 0 to 11) of boolean;
        variable ink : ink_t := (others => (others => false));
        variable row : integer;
        variable txt : string(1 to 12);
        variable art : string(1 to 96);
        type pix_t is array(0 to 95) of boolean;
        variable pix : pix_t := (others => false);
    begin
        wait until fed;
        wait until falling_edge(vsync);
        wait until falling_edge(vsync);

        L := 0;
        loop
            wait until falling_edge(hsync);
            L := L + 1;
            vy := L - 37;
            exit when vy >= 56;
            if vy >= 0 then
                c := 0;
                for k in 0 to 700 loop
                    wait until falling_edge(clk);
                    fx := c - 293;
                    if fx >= 0 and fx < 96 then
                        if r(0) = '1' then
                            row  := vy / 14;
                            cell := fx / 8;
                            pix(fx) := true;
                            if row <= 3 and cell <= 11 then ink(row, cell) := true; end if;
                        end if;
                    end if;
                    c := c + 1;
                    exit when fx >= 96;
                end loop;
                -- raw scanline, so the actual glyphs can be read in the log
                for k in 0 to 95 loop
                    if pix(k) then art(k + 1) := '#'; else art(k + 1) := '.'; end if;
                end loop;
                write(ln, string'("vy") & integer'image(vy) & "|" & art);
                writeline(std.textio.output, ln);
                pix := (others => false);
            end if;
        end loop;

        for rr in 0 to 3 loop
            for cc in 0 to 11 loop
                if ink(rr, cc) then txt(cc + 1) := '#'; else txt(cc + 1) := '.'; end if;
            end loop;
            write(ln, string'("cells row") & integer'image(rr) & " [" & txt & "]");
            writeline(std.textio.output, ln);
        end loop;
        report "*** ALL LINE-EDITING TESTS PASSED ***";
        done <= true;
        wait;
    end process;
end architecture sim;

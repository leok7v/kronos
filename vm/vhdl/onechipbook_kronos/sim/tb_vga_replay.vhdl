library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use ieee.std_logic_arith.all;
use std.textio.all;

-- Replay a REAL captured console byte stream through the real console.
--
-- The ex editor's blank page survived three fixes that were reasoned out from
-- the driver sources. This testbench removes the reasoning: stream.hex is the
-- literal byte stream the OS sent while a file was scrolled in ex, captured
-- from the reference VM through a pty. Whatever the console does with it here
-- is what it does on the board.
--
-- Output is an OCCUPANCY MAP -- one character per cell, '#' where the cell has
-- any lit pixel -- built by sampling the pixel stream over one whole frame.
-- That is enough to answer the only question that matters: is the text there,
-- and on which rows.

entity tb_vga_replay is
    generic (
        -- pclk cycles between bytes. The real console gets one byte per 10 bit
        -- times at 57600 baud = 174 us = ~11200 cycles; the longest console
        -- operation (a full-screen ESC J) is 6400. 7000 keeps the ordering
        -- realistic while holding the simulation to a sane length.
        PACE : integer := 7000);
end tb_vga_replay;

architecture sim of tb_vga_replay is
    constant PERIOD : time := 15520 ps;          -- 64.43 MHz
    constant COLS   : integer := 128;
    constant ROWS   : integer := 50;

    signal clk    : std_logic := '0';
    signal reset  : std_logic := '1';
    signal ch     : std_logic_vector(7 downto 0) := (others => '0');
    signal ch_tog : std_logic := '0';
    signal hsync, vsync : std_logic;
    signal r, g, b : std_logic_vector(5 downto 0);
    signal done, fed : boolean := false;
begin
    clk <= not clk after PERIOD / 2 when not done else '0';

    dut : entity work.vga_console
        port map (pclk => clk, reset => reset, ch => ch, ch_tog => ch_tog,
                  hsync => hsync, vsync => vsync, r => r, g => g, b => b);

    stim : process
        file     fh   : text;
        variable l    : line;
        variable v    : integer;
        variable n    : integer := 0;
        variable ok   : file_open_status;
    begin
        wait for PERIOD * 20;
        reset <= '0';
        wait for PERIOD * 8000;                  -- let the power-on INIT finish

        file_open(ok, fh, "stream.hex", read_mode);
        assert ok = open_ok report "FAIL: cannot open stream.hex" severity failure;
        while not endfile(fh) loop
            readline(fh, l);
            read(l, v);          -- stream.hex holds decimal byte values
            wait until rising_edge(clk);
            ch <= conv_std_logic_vector(v, 8);
            wait until rising_edge(clk);
            ch_tog <= not ch_tog;
            for k in 1 to PACE loop wait until rising_edge(clk); end loop;
            n := n + 1;
        end loop;
        file_close(fh);
        report "replayed " & integer'image(n) & " bytes" severity note;
        fed <= true;
        wait;
    end process;

    -- ------------------------------------------------------------ occupancy
    check : process
        type map_t is array(0 to ROWS - 1, 0 to COLS - 1) of boolean;
        variable m    : map_t := (others => (others => false));
        variable L    : integer := 0;
        variable vy, fx, c : integer;
        variable ln   : line;
        variable row, col : integer;
        variable nrows_lit : integer := 0;
        variable rowlit : boolean;
    begin
        wait until fed;
        -- sample one complete frame
        wait until falling_edge(vsync);
        L := 0;
        loop
            wait until falling_edge(hsync);
            L := L + 1;
            vy := L - 37;                        -- vsync falls at vy=771 of 808
            exit when vy >= ROWS * 14;
            if vy >= 0 then
                c := 0;
                for k in 0 to 1100 loop
                    wait until falling_edge(clk);
                    fx := c - 293;
                    if fx >= 0 and fx < COLS * 8 then
                        if r(0) = '1' then
                            row := vy / 14;
                            col := fx / 8;
                            if row < ROWS and col < COLS then
                                m(row, col) := true;
                            end if;
                        end if;
                    end if;
                    c := c + 1;
                    exit when fx >= COLS * 8;
                end loop;
            end if;
        end loop;

        for y in 0 to ROWS - 1 loop
            rowlit := false;
            write(ln, string'("R"));
            if y < 10 then write(ln, string'("0")); end if;
            write(ln, y);
            write(ln, string'("|"));
            for x in 0 to COLS - 1 loop
                if m(y, x) then
                    write(ln, string'("#")); rowlit := true;
                else
                    write(ln, string'("."));
                end if;
            end loop;
            writeline(std.textio.output, ln);
            if rowlit then nrows_lit := nrows_lit + 1; end if;
        end loop;
        report "rows with content: " & integer'image(nrows_lit) & " of "
               & integer'image(ROWS) severity note;
        assert nrows_lit > ROWS / 2
            report "FAIL: the screen is essentially BLANK after scrolling a "
                 & "file in ex -- this is the reported bug, reproduced."
            severity failure;
        report "*** PASS: the text survived a real ex scroll ***" severity note;
        done <= true;
        wait;
    end process;
end architecture sim;

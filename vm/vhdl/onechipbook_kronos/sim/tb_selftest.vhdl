library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use std.textio.all;

-- Prove the on-hardware console self-test before it is trusted on hardware.
-- Runs console_selftest into the real console in BOTH variants and prints the
-- occupancy map, so the picture the board is expected to show is known here
-- first. Otherwise a blank board would be ambiguous: broken console, or broken
-- self-test?

entity tb_selftest is
    generic (ESCJ : std_logic := '1');
end tb_selftest;

architecture sim of tb_selftest is
    constant PCLK_P : time := 15520 ps;      -- 64.43 MHz video
    constant SCLK_P : time := 46561 ps;      -- 21.477 MHz system
    constant COLS   : integer := 128;
    constant ROWS   : integer := 50;

    signal pclk, sclk : std_logic := '0';
    signal reset : std_logic := '1';
    signal done  : boolean := false;

    signal tbyte : std_logic_vector(7 downto 0);
    signal tstb  : std_logic;
    signal cdc_byte : std_logic_vector(7 downto 0) := (others => '0');
    signal cdc_tog  : std_logic := '0';
    signal hsync, vsync : std_logic;
    signal r, g, b : std_logic_vector(5 downto 0);
begin
    pclk <= not pclk after PCLK_P / 2 when not done else '0';
    sclk <= not sclk after SCLK_P / 2 when not done else '0';

    -- a much shorter GAP than the real 3730: the console's longest operation is
    -- a full-screen ESC J at 6400 pixel-clock cycles ~= 2150 system cycles, so
    -- 2500 keeps the ordering honest while the simulation stays short
    gen : entity work.console_selftest
        generic map (GAP => 2200)
        port map (clk => sclk, reset => reset, enable => '1', escj => ESCJ,
                  byte => tbyte, stb => tstb);

    -- the same toggle CDC the board uses
    process (sclk)
    begin
        if rising_edge(sclk) then
            if reset = '1' then cdc_tog <= '0';
            elsif tstb = '1' then
                cdc_byte <= tbyte; cdc_tog <= not cdc_tog;
            end if;
        end if;
    end process;

    dut : entity work.vga_console
        port map (pclk => pclk, reset => reset, ch => cdc_byte, ch_tog => cdc_tog,
                  hsync => hsync, vsync => vsync, r => r, g => g, b => b);

    stim : process
    begin
        wait for PCLK_P * 20;
        reset <= '0';
        wait;
    end process;

    check : process
        type map_t is array(0 to ROWS - 1, 0 to COLS - 1) of boolean;
        variable m : map_t := (others => (others => false));
        variable L, vy, fx, c, row, col, lit : integer := 0;
        variable ln : line;
        variable rowlit : boolean;
    begin
        -- let the whole sequence play out
        -- 5 + 49*(7+25) + 4*30 = ~1693 bytes; at GAP=2200 system clocks that
        -- is ~173 ms, so give it margin before sampling the screen
        wait for 190 ms;
        wait until falling_edge(vsync);
        L := 0;
        loop
            wait until falling_edge(hsync);
            L := L + 1;
            vy := L - 37;
            exit when vy >= ROWS * 14;
            if vy >= 0 then
                c := 0;
                for k in 0 to 1100 loop
                    wait until falling_edge(pclk);
                    fx := c - 293;
                    if fx >= 0 and fx < COLS * 8 then
                        if r(0) = '1' then
                            row := vy / 14; col := fx / 8;
                            if row < ROWS and col < COLS then m(row, col) := true; end if;
                        end if;
                    end if;
                    c := c + 1;
                    exit when fx >= COLS * 8;
                end loop;
            end if;
        end loop;

        lit := 0;
        for y in 0 to ROWS - 1 loop
            rowlit := false;
            write(ln, string'("R"));
            if y < 10 then write(ln, string'("0")); end if;
            write(ln, y); write(ln, string'("|"));
            for x in 0 to 47 loop
                if m(y, x) then write(ln, string'("#")); rowlit := true;
                else write(ln, string'(".")); end if;
            end loop;
            writeline(std.textio.output, ln);
            if rowlit then lit := lit + 1; end if;
        end loop;
        report "ESCJ=" & std_logic'image(ESCJ) & "  rows with content: "
               & integer'image(lit) & " of " & integer'image(ROWS) severity note;
        assert lit > ROWS / 2
            report "FAIL: self-test itself produces a blank screen -- fix it "
                 & "before trusting it on hardware"
            severity failure;
        report "*** self-test renders ***" severity note;
        done <= true;
        wait;
    end process;
end architecture sim;

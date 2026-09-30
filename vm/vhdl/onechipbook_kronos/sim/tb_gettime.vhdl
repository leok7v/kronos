library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Check io2 gettime (op 6) returns what XD.m actually reads.
--
-- XD.doio runs this on EVERY disk operation and does
--     time := tim.pack(y,m,d,hr,mn,sc);  tim.set_time(time)
-- so the six words ARE the machine's clock. Getting them wrong does not fail
-- quietly: pack() returns -1 for an out-of-range year, set_time(-1) lands the
-- OS at the 1-Jan-1986 epoch, and it happens again on the next disk access.
-- That was the real bug -- the clock kept resetting with no reboot, and kc
-- could never save a KC.SETUP newer than its own binary.
--
-- Word order, matching the reference VM's buffer fill:
--     wYear, wMonth, wDay, wHour, wMinute, wSecond
-- The YEAR MUST BE A FULL YEAR and <= 2017, because the shipped Time.pack
-- refuses anything later (measured on hardware: 2017 ok, 2020 and 2026 not).

entity tb_gettime is
end tb_gettime;

architecture sim of tb_gettime is
    type int6 is array(0 to 5) of integer;   -- integer_vector is VHDL-2008
    constant PERIOD : time := 46561 ps;          -- 21.47727 MHz
    -- a deliberately tiny "second" so the tb can watch the clock advance
    constant FAKE_HZ : integer := 1000;

    signal clk   : std_logic := '0';
    signal reset : std_logic := '1';
    signal done  : boolean := false;

    signal reg_adr   : std_logic_vector(2 downto 0) := (others => '0');
    signal reg_dat_i : std_logic_vector(31 downto 0) := (others => '0');
    signal reg_dat_o : std_logic_vector(31 downto 0);
    signal reg_stb, reg_we : std_logic := '0';
    signal dma_adr : std_logic_vector(18 downto 0);
    signal dma_dat : std_logic_vector(31 downto 0);
    signal dma_stb, dma_we, boot_done, boot_fail, busy_o : std_logic;
    signal sclk, scs, smosi, smiso : std_logic;
begin
    clk <= not clk after PERIOD / 2 when not done else '0';

    dut : entity work.sd_disk_controller
        generic map (CLK_DIV => 16, BOOT_SECTORS => 0, WDOG_CYCLES => 2000000,
                     CLK_HZ => FAKE_HZ,
                     RTC_YEAR => 2017, RTC_MONTH => 7, RTC_DAY => 19)
        port map (clk => clk, reset => reset,
                  reg_adr => reg_adr, reg_dat_i => reg_dat_i, reg_dat_o => reg_dat_o,
                  reg_stb => reg_stb, reg_we => reg_we,
                  dma_adr => dma_adr, dma_dat => dma_dat, dma_stb => dma_stb,
                  dma_we => dma_we, dma_ack => '0',
                  boot_done => boot_done, boot_fail => boot_fail, busy_o => busy_o,
                  sd_clk => sclk, sd_cs => scs, sd_mosi => smosi, sd_miso => smiso);

    -- without a card model the controller never finishes init and never
    -- serves a command, so gettime cannot even be issued
    card : entity work.sd_model_xd0
        generic map (FNAME => "xd0.dec", SECTORS => 1580)
        port map (sclk => sclk, cs => scs, mosi => smosi, miso => smiso,
                  dbg_wsec => open, dbg_wxor => open, dbg_wdone => open);

    stim : process
        variable w : integer;
        variable got : int6;
        variable t2  : int6;

        procedure wreg(a : integer; d : integer) is
        begin
            wait until rising_edge(clk);
            reg_adr <= std_logic_vector(to_unsigned(a, 3));
            reg_dat_i <= std_logic_vector(to_unsigned(d, 32));
            reg_we <= '1'; reg_stb <= '1';
            wait until rising_edge(clk);
            reg_stb <= '0'; reg_we <= '0';
        end procedure;

        procedure rreg(a : integer; v : out integer) is
        begin
            wait until rising_edge(clk);
            reg_adr <= std_logic_vector(to_unsigned(a, 3));
            reg_stb <= '1'; reg_we <= '0';
            wait until rising_edge(clk);
            reg_stb <= '0';
            v := to_integer(unsigned(reg_dat_o));
        end procedure;

        -- one full gettime: issue op 6, then pull six words through DATA
        procedure gettime(t : out int6) is
            variable f, v : integer;
            variable guard : integer := 0;
        begin
            wreg(6, 0);                       -- DSK
            wreg(3, 6);                       -- CMD = gettime
            for i in 0 to 5 loop
                loop                          -- wait for "data available"
                    rreg(5, f);
                    exit when (f mod 2) = 1;
                    guard := guard + 1;
                    assert guard < 100000 report "FAIL: gettime stalled" severity failure;
                end loop;
                rreg(7, v);                   -- read DATA word
                t(i) := v;
            end loop;
        end procedure;
    begin
        wait for PERIOD * 20;
        reset <= '0';
        wait until boot_done = '1' for 50 ms;
        assert boot_done = '1' report "FAIL: card never became ready" severity failure;

        gettime(got);
        report "gettime -> y=" & integer'image(got(0)) & " m=" & integer'image(got(1))
             & " d=" & integer'image(got(2)) & " hr=" & integer'image(got(3))
             & " mn=" & integer'image(got(4)) & " sc=" & integer'image(got(5))
             severity note;

        -- the year must be one Time.pack will ACCEPT, or set_time gets -1
        assert got(0) >= 1986 and got(0) <= 2017
            report "FAIL: year " & integer'image(got(0))
                 & " is outside what the shipped Time.pack accepts (1986..2017) "
                 & "-- pack would return -1 and reset the OS clock to the epoch"
            severity failure;
        assert got(1) >= 1 and got(1) <= 12
            report "FAIL: month out of range" severity failure;
        assert got(2) >= 1 and got(2) <= 31
            report "FAIL: day-of-month out of range (a WEEKDAY here was the old bug)"
            severity failure;
        assert got(3) <= 23 and got(4) <= 59 and got(5) <= 59
            report "FAIL: time of day out of range" severity failure;

        -- and it must RUN: a frozen clock would freeze the whole machine's time
        wait for PERIOD * FAKE_HZ * 3;
        gettime(t2);
        report "3 fake-seconds later -> sc=" & integer'image(t2(5)) severity note;
        assert t2(5) /= got(5)
            report "FAIL: the time of day never advanced -- gettime is frozen"
            severity failure;

        report "*** PASS: gettime returns a full year, a real day-of-month, "
             & "and a running clock ***" severity note;
        done <= true;
        wait;
    end process;
end architecture sim;

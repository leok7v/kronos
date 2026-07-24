library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Unit test for suspend_ctrl with the runtime, seconds-based timer. A sec_tick
-- stand-in pulses every 10 clocks (a fast "second"); timeout_sec=5 -> sleep
-- after ~5 ticks. A behavioural stand-in models the SDRAM suspend/suspended
-- handshake. Checks: sleeps on inactivity, stays asleep, a key wakes it (halt
-- held until SDRAM resumes), activity keeps it awake, and timeout_sec=0 disables.

entity tb_suspend_ctrl is
end tb_suspend_ctrl;

architecture sim of tb_suspend_ctrl is
    signal clk   : std_logic := '0';
    signal reset : std_logic := '1';
    signal done  : boolean := false;
    signal cpu_idle, activity, sleepnow, sec_tick, sdc_suspended : std_logic := '0';
    signal cpu_halt, sdc_suspend, sleeping : std_logic;
    signal dbg_ramp : std_logic_vector(2 downto 0);
    signal timeout_sec : std_logic_vector(15 downto 0) := std_logic_vector(to_unsigned(5, 16));
    signal errors : integer := 0;
begin
    clk <= not clk after 5 ns when not done else '0';

    dut : entity work.suspend_ctrl
        generic map (IDLE_GATED => false)
        port map (clk=>clk, reset=>reset, cpu_idle=>cpu_idle, activity=>activity,
                  sleep_now=>sleepnow, sec_tick=>sec_tick, timeout_sec=>timeout_sec, sdc_suspended=>sdc_suspended,
                  cpu_halt=>cpu_halt, sdc_suspend=>sdc_suspend, sleeping=>sleeping, dbg_ramp=>dbg_ramp);

    -- SDRAM controller stand-in: suspended tracks suspend with 3-cycle lag.
    model : process
    begin
        wait until rising_edge(clk);
        if reset = '1' then sdc_suspended <= '0';
        elsif sdc_suspend = '1' and sdc_suspended = '0' then
            for i in 0 to 2 loop wait until rising_edge(clk); end loop; sdc_suspended <= '1';
        elsif sdc_suspend = '0' and sdc_suspended = '1' then
            for i in 0 to 2 loop wait until rising_edge(clk); end loop; sdc_suspended <= '0';
        end if;
    end process;

    -- 1 Hz stand-in: a one-cycle sec_tick every 10 clocks.
    sec : process
    begin
        wait until rising_edge(clk);
        sec_tick <= '0';
        for i in 0 to 8 loop wait until rising_edge(clk); end loop;
        sec_tick <= '1';
    end process;

    stim : process
        procedure step is begin wait until rising_edge(clk); end procedure;
        variable n : integer;
    begin
        reset <= '1'; activity <= '0'; cpu_idle <= '0';
        for i in 0 to 4 loop step; end loop;
        reset <= '0'; step;

        -- 1) sleeps after ~timeout seconds of inactivity
        n := 0;
        while sdc_suspend = '0' loop step; n := n + 1;
            assert n < 400 report "FAIL: never slept on inactivity" severity failure;
        end loop;
        assert cpu_halt = '1' report "FAIL: CPU not halted on suspend" severity warning;
        report "*** ok: slept after inactivity timeout ***" severity note;
        n := 0; while sleeping = '0' loop step; n := n + 1;
            assert n < 20 report "FAIL: sleeping never asserted" severity failure; end loop;
        for i in 0 to 60 loop step;
            if sleeping /= '1' then report "FAIL: woke with no key" severity warning; errors <= errors + 1; end if;
        end loop;
        report "*** ok: stayed asleep ***" severity note;

        -- 2) keypress wakes; halt held until SDRAM resumes
        activity <= '1'; step; activity <= '0';
        while sdc_suspended = '1' loop
            assert cpu_halt = '1' report "FAIL: CPU released before SDRAM resumed" severity warning;
            step;
        end loop;
        n := 0; while cpu_halt = '1' loop step; n := n + 1;
            assert n < 10 report "FAIL: CPU never released after wake" severity failure; end loop;
        assert sleeping = '0' report "FAIL: still sleeping after wake" severity warning;
        report "*** ok: keypress woke it, CPU released after resume ***" severity note;

        -- 3) periodic activity keeps it awake despite the sec_ticks
        for i in 0 to 300 loop
            step;
            if (i mod 7) = 0 then activity <= '1'; else activity <= '0'; end if;
            if sdc_suspend /= '0' then
                report "FAIL: slept despite activity" severity warning; errors <= errors + 1; end if;
        end loop;
        activity <= '0';
        report "*** ok: activity kept it awake ***" severity note;

        -- 4) timeout_sec = 0 disables auto-sleep entirely
        timeout_sec <= (others => '0');
        for i in 0 to 300 loop
            step;
            if sdc_suspend /= '0' then
                report "FAIL: slept with timeout=0" severity warning; errors <= errors + 1; end if;
        end loop;
        report "*** ok: timeout=0 disables sleep ***" severity note;

        -- 5) `sleep` command: a sleepnow pulse sleeps immediately, even with
        --    auto-sleep disabled (timeout_sec still 0 from case 4).
        sleepnow <= '1'; step; sleepnow <= '0';
        n := 0;
        while sdc_suspend = '0' loop step; n := n + 1;
            assert n < 10 report "FAIL: sleepnow did not sleep immediately" severity failure;
        end loop;
        report "*** ok: sleepnow (sleep command) slept immediately ***" severity note;
        -- and a key still wakes it (wait until fully parked first, so the wake
        -- key is not consumed during the brief S_ENTER window)
        n := 0; while sdc_suspended = '0' loop step; n := n + 1;
            assert n < 20 report "FAIL: never fully parked after sleepnow" severity failure; end loop;
        activity <= '1'; step; activity <= '0';
        n := 0; while cpu_halt = '1' loop step; n := n + 1;
            assert n < 20 report "FAIL: no wake after sleepnow-sleep" severity failure; end loop;
        report "*** ok: woke from forced sleep ***" severity note;

        if errors = 0 then
            report "*** PASS: suspend_ctrl (configurable) behaves correctly ***" severity note;
        else
            report "*** FAIL: " & integer'image(errors) & " errors ***" severity failure;
        end if;
        done <= true; wait;
    end process;
end sim;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Measure sdram_controller's COST PER ACCESS, separately for same-row and
-- different-row traffic, and count the ACTIVE commands it issues.
--
-- WHY: tb_sdram proves the controller is correct; it says nothing about whether
-- the open-row policy is actually engaged. A controller that took the row-miss
-- path every single time would pass tb_sdram perfectly while delivering none of
-- the speedup -- and the only place that would show up is a hardware flash,
-- which costs ~5 minutes and a person standing at the board.
--
-- So this bench measures the thing the optimisation claims to improve:
--
--   same-row    256 sequential words inside one row  (row = word address >> 8)
--   diff-row    256 words each 256 apart, so every access opens a new row
--
-- and reports cycles/access for each, plus the ACTIVE count -- which is the
-- direct evidence: with open-row, same-row traffic must issue FAR fewer ACTIVEs
-- than accesses. If ACTIVEs == accesses, the policy is not working regardless
-- of what the cycle numbers say.
--
-- CROSS-CHECK AGAINST HARDWARE. The board's perf counters measured 12.2-12.4
-- cycles per SDRAM transaction on the old auto-precharge controller during a
-- real `dry` run. The diff-row number here is the closest analogue to that, so
-- if this bench reports something wildly different from ~13 for the OLD
-- controller, the bench is wrong and its predictions should not be trusted.
--
-- T_REFI is deliberately LARGE here (refresh every 1000 cycles rather than
-- tb_sdram's 40). Refresh closes the open row, so a short interval turns
-- same-row hits into misses and would understate the policy's value. Real
-- refresh is ~7.8us = ~168 cycles at 21.5MHz, far enough apart not to dominate.

entity tb_sdram_perf is
end tb_sdram_perf;

architecture sim of tb_sdram_perf is
    constant ADDR_BITS : integer := 19;
    constant N         : integer := 256;      -- accesses per pattern

    signal clk   : std_logic := '0';
    signal reset : std_logic := '1';

    signal wb_adr : std_logic_vector(ADDR_BITS-1 downto 0) := (others => '0');
    signal wb_di  : std_logic_vector(31 downto 0) := (others => '0');
    signal wb_do  : std_logic_vector(31 downto 0);
    signal wb_we, wb_stb, wb_cyc, wb_ack, rdy : std_logic := '0';

    signal sd_a   : std_logic_vector(12 downto 0);
    signal sd_ba  : std_logic_vector(1 downto 0);
    signal sd_dq  : std_logic_vector(15 downto 0);
    signal sd_cke, sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n, sd_dqml, sd_dqmh : std_logic;

    signal cyc_cnt : integer := 0;            -- free-running cycle counter
    signal act_cnt : integer := 0;            -- ACTIVE commands seen on the pins
begin
    clk <= not clk after 18.5 ns;   -- ~27 MHz

    dut : entity work.sdram_controller
        generic map (ADDR_BITS => ADDR_BITS, T_INIT => 20, T_REFI => 1000)
        port map (clk=>clk, reset=>reset,
                  wb_adr=>wb_adr, wb_dat_i=>wb_di, wb_dat_o=>wb_do,
                  wb_we=>wb_we, wb_stb=>wb_stb, wb_cyc=>wb_cyc, wb_ack=>wb_ack, ready=>rdy,
                  sd_a=>sd_a, sd_ba=>sd_ba, sd_dq=>sd_dq, sd_cke=>sd_cke,
                  sd_cs_n=>sd_cs_n, sd_ras_n=>sd_ras_n, sd_cas_n=>sd_cas_n, sd_we_n=>sd_we_n,
                  sd_dqml=>sd_dqml, sd_dqmh=>sd_dqmh);

    model : entity work.sdram_model
        port map (clk=>clk, a=>sd_a, ba=>sd_ba, dq=>sd_dq, cke=>sd_cke,
                  cs_n=>sd_cs_n, ras_n=>sd_ras_n, cas_n=>sd_cas_n, we_n=>sd_we_n,
                  dqml=>sd_dqml, dqmh=>sd_dqmh);

    -- cycle counter, and an ACTIVE-command sniffer on the SDRAM pins
    tick : process (clk)
    begin
        if rising_edge(clk) then
            cyc_cnt <= cyc_cnt + 1;
            if sd_cs_n = '0' and sd_ras_n = '0' and sd_cas_n = '1' and sd_we_n = '1' then
                act_cnt <= act_cnt + 1;       -- ACTIVE
            end if;
        end if;
    end process;

    stim : process
        variable t0, a0 : integer;
        variable cyc_same, cyc_diff : integer;
        variable act_same, act_diff : integer;

        procedure wb_write(addr : integer; data : std_logic_vector(31 downto 0)) is
        begin
            wb_adr <= std_logic_vector(to_unsigned(addr, ADDR_BITS));
            wb_di  <= data; wb_we <= '1'; wb_stb <= '1'; wb_cyc <= '1';
            loop wait until rising_edge(clk); exit when wb_ack = '1'; end loop;
            wb_stb <= '0'; wb_cyc <= '0'; wb_we <= '0';
            wait until rising_edge(clk);
        end procedure;

        procedure wb_read(addr : integer) is
        begin
            wb_adr <= std_logic_vector(to_unsigned(addr, ADDR_BITS));
            wb_we <= '0'; wb_stb <= '1'; wb_cyc <= '1';
            loop wait until rising_edge(clk); exit when wb_ack = '1'; end loop;
            wb_stb <= '0'; wb_cyc <= '0';
            wait until rising_edge(clk);
        end procedure;

        -- cycles*10/n, so one decimal place without real arithmetic
        function tenths(total : integer; n : integer) return string is
        begin
            return integer'image((total * 10) / n / 10) & "." &
                   integer'image((total * 10) / n mod 10);
        end function;
    begin
        reset <= '1'; wait for 1 us; reset <= '0';
        wait until rdy = '1' for 200 us;
        assert rdy = '1' report "FAIL: controller never became ready (init)" severity failure;

        -- ---- same row: 256 consecutive words, row = addr>>8, so all row 0 ----
        t0 := cyc_cnt; a0 := act_cnt;
        for i in 0 to N-1 loop wb_read(i); end loop;
        cyc_same := cyc_cnt - t0; act_same := act_cnt - a0;

        -- ---- different row every time: stride 256 words = one row apart ----
        t0 := cyc_cnt; a0 := act_cnt;
        for i in 0 to N-1 loop wb_read(i * 256); end loop;
        cyc_diff := cyc_cnt - t0; act_diff := act_cnt - a0;

        report "";
        report "=== sdram_controller cost per access (" & integer'image(N) & " reads each) ===";
        report "same-row : " & tenths(cyc_same, N) & " cycles/access, " &
               integer'image(act_same) & " ACTIVE cmds";
        report "diff-row : " & tenths(cyc_diff, N) & " cycles/access, " &
               integer'image(act_diff) & " ACTIVE cmds";
        report "blended at the 56% same-row rate measured on hardware: " &
               tenths(cyc_same * 56 + cyc_diff * 44, N * 100) & " cycles/access";

        -- The optimisation is only real if same-row traffic stops re-opening
        -- the row. Without open-row this is one ACTIVE per access.
        assert act_same < N / 4
            report "OPEN-ROW NOT ENGAGED: " & integer'image(act_same) &
                   " ACTIVE commands for " & integer'image(N) &
                   " same-row accesses -- the row is being closed between them"
            severity failure;
        report "*** open-row engaged: same-row traffic issued " & integer'image(act_same) &
               " ACTIVEs for " & integer'image(N) & " accesses ***";
        wait;
    end process;
end sim;

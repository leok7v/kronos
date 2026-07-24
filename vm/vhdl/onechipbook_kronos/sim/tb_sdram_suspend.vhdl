library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Verify the suspend-to-RAM (self-refresh) path of sdram_controller.
--
-- What this proves, against the protocol-checking sdram_model:
--   1. `suspend` parks the controller: `suspended` rises and CKE goes LOW.
--   2. It STAYS parked -- CKE is held low for far longer than a refresh
--      interval (T_REFI=40 here), so the controller is NOT sneaking in normal
--      AUTO_REFRESH commands; the device is refreshing itself. A CKE that rose
--      during the window would mean the controller left self-refresh on its own.
--   3. A bus request issued WHILE parked is not acked (it stalls), and is then
--      served correctly the moment `suspend` drops -- the "CPU freezes on the
--      bus, then continues" contract the top-level relies on.
--   4. Every word written before the suspend reads back byte-for-byte after
--      resume. (The model's array never decays, so retention is a given; what is
--      under test is that entry/exit issue no corrupting command and the resume
--      sequence puts the device back in a servable state.)
--   5. The model's severity-FAILURE protocol assertions never fire across
--      entry, the long hold, or exit -- i.e. the entry/exit command sequence is
--      legal (no ACTIVE-while-open, no refresh-with-a-row-open, tXSR honoured).
--   6. All of the above survive TWO back-to-back suspend/resume cycles.
--
-- NOTE ON THE MODEL. sdram_model does not simulate the device's internal
-- self-refresh explicitly -- it decodes commands only while CKE is high, so the
-- CKE-low entry/hold simply pass as "device busy internally", and its storage
-- (a plain array) never decays. That is faithful in effect: no external command
-- is accepted while parked, and the data is still there on exit, which is
-- exactly the observable contract of self-refresh. The hardware-only part it
-- cannot model -- the actual low-power internal refresh -- is what CKE-low
-- physically triggers on the real W9825G6-class part.

entity tb_sdram_suspend is
end tb_sdram_suspend;

architecture sim of tb_sdram_suspend is
    constant ADDR_BITS : integer := 23;
    signal clk   : std_logic := '0';
    signal reset : std_logic := '1';
    signal done  : boolean := false;

    signal wb_adr : std_logic_vector(ADDR_BITS-1 downto 0) := (others => '0');
    signal wb_di  : std_logic_vector(31 downto 0) := (others => '0');
    signal wb_do  : std_logic_vector(31 downto 0);
    signal wb_we, wb_stb, wb_cyc, wb_ack, rdy : std_logic := '0';
    signal suspend   : std_logic := '0';
    signal suspended : std_logic;

    signal sd_a   : std_logic_vector(12 downto 0);
    signal sd_ba  : std_logic_vector(1 downto 0);
    signal sd_dq  : std_logic_vector(15 downto 0);
    signal sd_cke, sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n, sd_dqml, sd_dqmh : std_logic;

    signal errors : integer := 0;

    -- test vectors: (word address, data). Addresses are spread across many rows
    -- of bank 0 (row = adr(20:8)); MEM_HW below is sized so none of them alias.
    type vec_t is record a : integer; d : std_logic_vector(31 downto 0); end record;
    type vlist_t is array (natural range <>) of vec_t;
    constant V : vlist_t := (
        (0,      x"DEADBEEF"), (1,      x"00000090"), (5,      x"07F00012"),
        (255,    x"12345678"), (256,    x"000001B1"), (1000,   x"CAFEF00D"),
        (4095,   x"A5A5A5A5"), (65535,  x"0000FFFF"), (262144, x"11223344"),
        (500000, x"FFFF0000"));
begin
    clk <= not clk after 18.5 ns when not done else '0';   -- ~27 MHz

    dut : entity work.sdram_controller
        generic map (ADDR_BITS => ADDR_BITS, T_INIT => 20, T_REFI => 40)
        port map (clk=>clk, reset=>reset,
                  wb_adr=>wb_adr, wb_dat_i=>wb_di, wb_dat_o=>wb_do,
                  wb_we=>wb_we, wb_stb=>wb_stb, wb_cyc=>wb_cyc, wb_ack=>wb_ack, ready=>rdy,
                  suspend=>suspend, suspended=>suspended,
                  sd_a=>sd_a, sd_ba=>sd_ba, sd_dq=>sd_dq, sd_cke=>sd_cke,
                  sd_cs_n=>sd_cs_n, sd_ras_n=>sd_ras_n, sd_cas_n=>sd_cas_n, sd_we_n=>sd_we_n,
                  sd_dqml=>sd_dqml, sd_dqmh=>sd_dqmh);

    model : entity work.sdram_model
        generic map (MEM_HW => 2097152)   -- 2M half-words: no aliasing for V above
        port map (clk=>clk, a=>sd_a, ba=>sd_ba, dq=>sd_dq, cke=>sd_cke,
                  cs_n=>sd_cs_n, ras_n=>sd_ras_n, cas_n=>sd_cas_n, we_n=>sd_we_n,
                  dqml=>sd_dqml, dqmh=>sd_dqmh);

    stim : process
        procedure wb_write(addr : integer; data : std_logic_vector(31 downto 0)) is
        begin
            wb_adr <= std_logic_vector(to_unsigned(addr, ADDR_BITS));
            wb_di  <= data; wb_we <= '1'; wb_stb <= '1'; wb_cyc <= '1';
            loop wait until rising_edge(clk); exit when wb_ack = '1'; end loop;
            wb_stb <= '0'; wb_cyc <= '0'; wb_we <= '0';
            wait until rising_edge(clk);
        end procedure;

        procedure wb_read(addr : integer; expect : std_logic_vector(31 downto 0)) is
        begin
            wb_adr <= std_logic_vector(to_unsigned(addr, ADDR_BITS));
            wb_we <= '0'; wb_stb <= '1'; wb_cyc <= '1';
            loop wait until rising_edge(clk); exit when wb_ack = '1'; end loop;
            if wb_do /= expect then
                report "MISMATCH @word " & integer'image(addr) &
                       " got " & to_hstring(wb_do) & " expected " & to_hstring(expect) severity warning;
                errors <= errors + 1;
            end if;
            wb_stb <= '0'; wb_cyc <= '0';
            wait until rising_edge(clk);
        end procedure;

        -- Park the controller, hold for `hold` cycles asserting CKE stays low,
        -- then resume. Checks suspended/CKE assert on entry and clear on exit.
        procedure suspend_window(hold : integer) is
        begin
            suspend <= '1';
            -- suspended must rise, and with it CKE must fall
            loop wait until rising_edge(clk); exit when suspended = '1'; end loop;
            wait until rising_edge(clk);   -- let CKE settle to its parked level
            if sd_cke /= '0' then
                report "FAIL: CKE not low while suspended" severity warning;
                errors <= errors + 1;
            end if;
            -- Held far longer than T_REFI (=40): CKE must never rise on its own.
            for i in 0 to hold loop
                wait until rising_edge(clk);
                if sd_cke /= '0' then
                    report "FAIL: CKE rose during self-refresh hold (@cycle " &
                           integer'image(i) & ")" severity warning;
                    errors <= errors + 1;
                end if;
            end loop;
            -- Resume and wait for full return to service.
            suspend <= '0';
            loop wait until rising_edge(clk); exit when suspended = '0'; end loop;
            if sd_cke /= '1' then
                report "FAIL: CKE not high after resume" severity warning;
                errors <= errors + 1;
            end if;
        end procedure;

        variable stall : integer;
    begin
        reset <= '1'; wait for 1 us; reset <= '0';
        wait until rdy = '1' for 200 us;
        assert rdy = '1' report "FAIL: controller never became ready (init)" severity failure;
        report "*** SDRAM init complete ***" severity note;

        -- 1) load the pattern
        for i in V'range loop wb_write(V(i).a, V(i).d); end loop;

        -- 2) first suspend window: park for 2000 cycles (>> 40-cycle T_REFI)
        report "*** entering self-refresh (window 1) ***" severity note;
        suspend_window(2000);
        report "*** resumed from self-refresh (window 1) ***" severity note;

        -- 3) everything must still be there
        for i in V'range loop wb_read(V(i).a, V(i).d); end loop;

        -- 4) the stall-then-serve contract: issue a READ while parked, confirm
        --    it is NOT acked for a good while, then resume and confirm the SAME
        --    held request completes with the right data.
        report "*** testing request-stall-during-suspend ***" severity note;
        suspend <= '1';
        loop wait until rising_edge(clk); exit when suspended = '1'; end loop;
        wb_adr <= std_logic_vector(to_unsigned(V(4).a, ADDR_BITS));  -- word 256
        wb_we <= '0'; wb_stb <= '1'; wb_cyc <= '1';
        stall := 0;
        for i in 0 to 199 loop
            wait until rising_edge(clk);
            if wb_ack = '1' then stall := stall + 1; end if;   -- must stay 0
        end loop;
        if stall /= 0 then
            report "FAIL: request was acked while suspended (" &
                   integer'image(stall) & " acks)" severity warning;
            errors <= errors + 1;
        end if;
        -- resume; the still-asserted read now completes
        suspend <= '0';
        loop wait until rising_edge(clk); exit when wb_ack = '1'; end loop;
        if wb_do /= V(4).d then
            report "FAIL: held read returned " & to_hstring(wb_do) &
                   " expected " & to_hstring(V(4).d) severity warning;
            errors <= errors + 1;
        end if;
        wb_stb <= '0'; wb_cyc <= '0';
        wait until rising_edge(clk);

        -- 5) a NORMAL access must work again right after (write + read a new word)
        wb_write(123456, x"5150524E");   -- "SPRN"
        wb_read (123456, x"5150524E");

        -- 6) do it all again to prove the entry/exit is repeatable
        report "*** entering self-refresh (window 2) ***" severity note;
        suspend_window(500);
        report "*** resumed from self-refresh (window 2) ***" severity note;
        for i in V'range loop wb_read(V(i).a, V(i).d); end loop;
        wb_read(123456, x"5150524E");

        if errors = 0 then
            report "*** PASS: all " & integer'image(V'length) &
                   " words survived self-refresh; suspend/resume clean ***" severity note;
        else
            report "*** FAIL: " & integer'image(errors) & " errors ***" severity failure;
        end if;
        done <= true; wait;
    end process;
end sim;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- SDR SDRAM controller for the OneChipBook (256 Mbit, 16-bit, 13-bit row /
-- 9-bit col / 2 banks -- e.g. W9825G6 / IS42S16160 class).
--
-- Presents a simple Wishbone-style slave to the Kronos bus. The Kronos word is
-- 32-bit and the SDRAM is 16-bit, so each word access is a burst-of-2 (BL=2)
-- with auto-precharge: one ACTIVE + one READ/WRITE per word. CAS latency 2.
--
-- The controller and the SDRAM run on the same clock; at the top level an altpll
-- drives the SDRAM's own clock pin (pin 38 = PLL1_OUTp) phase-shifted so the
-- device captures commands/data correctly. Read latency is variable -- the CPU
-- waits on `ack`, so no fixed-latency assumptions (unlike the M4K path).
--
-- Timing generics are in clock cycles; defaults are safe for <=27 MHz where one
-- cycle (>=37 ns) already covers most SDR timings. The testbench shrinks T_INIT
-- and T_REFI so simulation is quick.

entity sdram_controller is
    generic (
        ADDR_BITS : integer := 19;      -- Kronos word-address width
        T_INIT    : integer := 5000;    -- power-up NOP cycles (>=100 us); sim shrinks
        T_REFI    : integer := 150;     -- auto-refresh interval in cycles (~7.8 us)
        CAS_LAT   : integer := 2;
        T_RCD     : integer := 2;        -- ACTIVE -> READ/WRITE
        T_RP      : integer := 2;        -- PRECHARGE period
        T_RFC     : integer := 4;        -- AUTO_REFRESH period
        T_MRD     : integer := 2;        -- LOAD_MODE period
        T_WR      : integer := 2;        -- write recovery before precharge
        T_XSR     : integer := 8         -- SELF-REFRESH exit -> first command (tXSR).
                                         -- Datasheet tXSR is ~70 ns; 8 cycles is
                                         -- safe up to well past the board's clock.
    );
    port (
        clk      : in  std_logic;
        reset    : in  std_logic;
        -- Wishbone-style slave (32-bit word)
        wb_adr   : in  std_logic_vector(ADDR_BITS-1 downto 0);
        wb_dat_i : in  std_logic_vector(31 downto 0);
        wb_dat_o : out std_logic_vector(31 downto 0);
        wb_we    : in  std_logic;
        wb_stb   : in  std_logic;
        wb_cyc   : in  std_logic;
        wb_ack   : out std_logic;
        ready    : out std_logic;        -- '1' once init done
        read_lat : in  std_logic_vector(3 downto 0) := "0100";  -- READ cmd -> first captured
                                         -- half-word, in cycles (CAS_LAT + register skew).
                                         -- Runtime-adjustable to tune SDRAM read capture on HW.
        -- Suspend-to-RAM (self-refresh). Assert `suspend` to park the SDRAM in
        -- self-refresh: the controller finishes any access already in flight,
        -- issues an AUTO_REFRESH with CKE dropping LOW (the JEDEC self-refresh
        -- entry), and then holds CKE low issuing nothing. The device retains all
        -- data on its own internal refresh at microamp-class current -- this is
        -- the "RAM" half of suspend-to-RAM. Deassert `suspend` to resume: CKE
        -- goes high, the controller waits tXSR, issues one refresh and returns
        -- to normal service. While parked it does NOT ack bus requests, so a
        -- master that touches RAM simply STALLS until resume (this is what lets
        -- the CPU be frozen by the bus rather than needing an explicit clock
        -- gate). `suspended` reads '1' from entry until service is fully
        -- restored, so a top-level sequencer can wait on it in both directions.
        suspend   : in  std_logic := '0';
        suspended : out std_logic;
        -- SDRAM device
        sd_a     : out std_logic_vector(12 downto 0);
        sd_ba    : out std_logic_vector(1 downto 0);
        sd_dq    : inout std_logic_vector(15 downto 0);
        sd_cke   : out std_logic;
        sd_cs_n  : out std_logic;
        sd_ras_n : out std_logic;
        sd_cas_n : out std_logic;
        sd_we_n  : out std_logic;
        sd_dqml  : out std_logic;
        sd_dqmh  : out std_logic
    );
end sdram_controller;

architecture rtl of sdram_controller is

    -- command = (cs_n, ras_n, cas_n, we_n)
    constant CMD_NOP    : std_logic_vector(3 downto 0) := "0111";
    constant CMD_ACTIVE : std_logic_vector(3 downto 0) := "0011";
    constant CMD_READ   : std_logic_vector(3 downto 0) := "0101";
    constant CMD_WRITE  : std_logic_vector(3 downto 0) := "0100";
    constant CMD_PRE    : std_logic_vector(3 downto 0) := "0010";
    constant CMD_REF    : std_logic_vector(3 downto 0) := "0001";
    constant CMD_MRS    : std_logic_vector(3 downto 0) := "0000";
    constant CMD_DESL   : std_logic_vector(3 downto 0) := "1111";

    -- mode register: BL=2 (A2:0=001), sequential (A3=0), CAS=2 (A6:4=010)
    constant MODE_REG : std_logic_vector(12 downto 0) := "0000000100001";

    type state_t is (S_RESET, S_INIT_WAIT, S_INIT_PRE, S_INIT_REF, S_INIT_MRS,
                     S_IDLE, S_REFRESH, S_ACTIVE, S_READ_DATA, S_WRITE_DATA, S_RECOVER, S_ACK, S_GAP,
                     S_SELF_ENTRY, S_SELF_HOLD, S_SELF_EXIT);   -- suspend-to-RAM
    signal state : state_t := S_RESET;

    signal cmd      : std_logic_vector(3 downto 0) := CMD_NOP;
    signal delay    : integer range 0 to T_INIT := 0;
    signal ref_cnt  : integer range 0 to T_REFI := 0;
    signal ref_due  : std_logic := '0';
    signal init_ref : integer range 0 to 8 := 0;

    -- latched request
    signal req_we   : std_logic := '0';
    signal req_dat  : std_logic_vector(31 downto 0) := (others => '0');
    signal hw_addr  : unsigned(23 downto 0);          -- half-word address = word*2 (full 24-bit)
    signal cap_cnt  : integer range 0 to 15 := 0;

    signal dq_out   : std_logic_vector(15 downto 0) := (others => '0');
    signal dq_oe    : std_logic := '0';
    signal dqm      : std_logic_vector(1 downto 0) := "11";
    signal rd_lo    : std_logic_vector(15 downto 0) := (others => '0');

    -- SDRAM clock-enable: high in normal operation, LOW only while parked in
    -- self-refresh. Registered so the entry command and CKE fall on one edge.
    signal sr_cke   : std_logic := '1';

begin
    -- SDRAM command / bidir data
    sd_cs_n  <= cmd(3);
    sd_ras_n <= cmd(2);
    sd_cas_n <= cmd(1);
    sd_we_n  <= cmd(0);
    sd_cke   <= sr_cke;
    -- '1' from self-refresh entry, through the parked hold, until service is
    -- fully restored (back in S_IDLE). A sequencer waits high before cutting
    -- power / gating clocks, and waits low again before releasing the CPU.
    suspended <= '1' when (state = S_SELF_ENTRY or state = S_SELF_HOLD
                           or state = S_SELF_EXIT) else '0';
    sd_dqml  <= dqm(0);
    sd_dqmh  <= dqm(1);
    sd_dq    <= dq_out when dq_oe = '1' else (others => 'Z');

    hw_addr <= resize(unsigned(wb_adr) & '0', 24); -- word*2 (low half-word address)

    process (clk)
        variable col : std_logic_vector(8 downto 0);
        variable row : std_logic_vector(12 downto 0);
        variable bnk : std_logic_vector(1 downto 0);
    begin
        if rising_edge(clk) then
            -- address fields of the current request's half-word base
            col := std_logic_vector(hw_addr(8 downto 0));
            row := std_logic_vector(hw_addr(21 downto 9));
            bnk := std_logic_vector(hw_addr(23 downto 22));

            cmd    <= CMD_NOP;
            wb_ack <= '0';

            -- refresh interval counter (runs once initialised). Frozen while
            -- parked in self-refresh: the device is refreshing itself, so the
            -- controller must not queue a normal AUTO_REFRESH -- and clearing
            -- ref_due here means it does not fire one the instant it resumes.
            if state = S_RESET or state = S_INIT_WAIT
               or state = S_SELF_ENTRY or state = S_SELF_HOLD or state = S_SELF_EXIT then
                ref_cnt <= 0; ref_due <= '0';
            elsif ref_cnt >= T_REFI then
                ref_due <= '1';
            else
                ref_cnt <= ref_cnt + 1;
            end if;

            if reset = '1' then
                state <= S_RESET; ready <= '0'; dq_oe <= '0'; dqm <= "11";
                delay <= 0; init_ref <= 0; ref_cnt <= 0; ref_due <= '0';
                sr_cke <= '1';
            else
                case state is

                    -- ---- initialisation ----------------------------------
                    when S_RESET =>
                        ready <= '0'; dq_oe <= '0'; dqm <= "11";
                        delay <= T_INIT; state <= S_INIT_WAIT;

                    when S_INIT_WAIT =>
                        if delay = 0 then
                            cmd <= CMD_PRE; sd_a <= (10 => '1', others => '0');  -- precharge all
                            delay <= T_RP; state <= S_INIT_PRE;
                        else
                            delay <= delay - 1;
                        end if;

                    when S_INIT_PRE =>
                        if delay = 0 then
                            cmd <= CMD_REF; delay <= T_RFC; init_ref <= 1; state <= S_INIT_REF;
                        else
                            delay <= delay - 1;
                        end if;

                    when S_INIT_REF =>
                        if delay = 0 then
                            if init_ref >= 8 then
                                cmd <= CMD_MRS; sd_a <= MODE_REG; sd_ba <= "00";
                                delay <= T_MRD; state <= S_INIT_MRS;
                            else
                                cmd <= CMD_REF; delay <= T_RFC; init_ref <= init_ref + 1;
                            end if;
                        else
                            delay <= delay - 1;
                        end if;

                    when S_INIT_MRS =>
                        if delay = 0 then ready <= '1'; state <= S_IDLE;
                        else delay <= delay - 1; end if;

                    -- ---- normal operation --------------------------------
                    when S_IDLE =>
                        ready <= '1'; dq_oe <= '0'; dqm <= "11";
                        -- Suspend takes priority: we only reach S_IDLE with every
                        -- bank precharged (auto-precharge closes the row after
                        -- each access), which is exactly the precondition for a
                        -- clean self-refresh entry. Any access already accepted
                        -- is in S_ACTIVE/READ/WRITE and finishes normally first,
                        -- because those states never look at `suspend`.
                        if suspend = '1' then
                            -- self-refresh ENTRY: AUTO_REFRESH command with CKE
                            -- dropping low on the same edge (both registered here).
                            cmd <= CMD_REF; sr_cke <= '0'; state <= S_SELF_ENTRY;
                        elsif ref_due = '1' then
                            cmd <= CMD_REF; delay <= T_RFC; ref_due <= '0'; ref_cnt <= 0;
                            state <= S_REFRESH;
                        elsif wb_cyc = '1' and wb_stb = '1' then
                            req_we <= wb_we; req_dat <= wb_dat_i;
                            cmd <= CMD_ACTIVE; sd_a <= row; sd_ba <= bnk;
                            delay <= T_RCD; state <= S_ACTIVE;
                        end if;

                    when S_REFRESH =>
                        if delay = 0 then state <= S_IDLE;
                        else delay <= delay - 1; end if;

                    when S_ACTIVE =>
                        if delay = 0 then
                            -- issue READ/WRITE with auto-precharge (A10=1)
                            sd_a <= "00" & '1' & '0' & col; sd_ba <= bnk;
                            if req_we = '1' then
                                cmd <= CMD_WRITE;
                                dq_out <= req_dat(15 downto 0); dq_oe <= '1'; dqm <= "00";
                                state <= S_WRITE_DATA;
                            else
                                cmd <= CMD_READ; dqm <= "00"; dq_oe <= '0';
                                cap_cnt <= to_integer(unsigned(read_lat)); state <= S_READ_DATA;
                            end if;
                        else
                            delay <= delay - 1;
                        end if;

                    when S_WRITE_DATA =>
                        -- second half-word on the cycle after the WRITE command
                        dq_out <= req_dat(31 downto 16); dq_oe <= '1'; dqm <= "00";
                        delay <= T_WR + T_RP; state <= S_RECOVER;

                    when S_READ_DATA =>
                        -- wait READ_LAT, then capture 2 half-words on consecutive cycles
                        if cap_cnt > 1 then
                            cap_cnt <= cap_cnt - 1;
                        elsif cap_cnt = 1 then
                            rd_lo <= sd_dq; cap_cnt <= 0;
                        else
                            wb_dat_o <= sd_dq & rd_lo;      -- high & low (held until read)
                            delay <= T_RP; state <= S_RECOVER;
                        end if;

                    when S_RECOVER =>
                        -- precharge / write-recovery time before acking
                        dq_oe <= '0'; dqm <= "11";
                        if delay = 0 then state <= S_ACK;
                        else delay <= delay - 1; end if;

                    when S_ACK =>
                        -- Ack for EXACTLY ONE cycle, then a mandatory ack-low
                        -- gap (S_GAP) before returning to IDLE. The data cache
                        -- issues back-to-back Wishbone cycles with stb HELD (a
                        -- dirty instruction-fetch miss FLUSHes the old line then
                        -- READs the refill without ever dropping stb). The old
                        -- "hold ack until stb drops" made the refill READ
                        -- complete against the FLUSH write's stale ack/data ->
                        -- the code word read back as 0 and the OS boot derailed
                        -- in memory_top. The S_GAP cycle gives the master time
                        -- to either drop stb (single request done) or present
                        -- the next request's address, so IDLE never re-runs a
                        -- still-asserted request.
                        wb_ack <= '1';
                        state  <= S_GAP;

                    when S_GAP =>
                        state <= S_IDLE;

                    -- ---- suspend to RAM (self-refresh) -------------------
                    when S_SELF_ENTRY =>
                        -- The AUTO_REFRESH was issued on the way in and CKE is
                        -- now low, so the device has entered self-refresh. cmd
                        -- defaults to NOP here; CKE stays low because sr_cke is
                        -- only ever reassigned on the way out.
                        state <= S_SELF_HOLD;

                    when S_SELF_HOLD =>
                        -- Parked: CKE low, no commands, no acks. The SDRAM holds
                        -- its contents by itself. Wait for the resume request.
                        if suspend = '0' then
                            sr_cke <= '1';       -- CKE high -> exit self-refresh
                            delay  <= T_XSR;      -- tXSR before any command
                            state  <= S_SELF_EXIT;
                        end if;

                    when S_SELF_EXIT =>
                        -- CKE is high again; NOP for tXSR, then one AUTO_REFRESH
                        -- (recommended after self-refresh) and back to service.
                        if delay = 0 then
                            cmd <= CMD_REF; delay <= T_RFC; state <= S_REFRESH;
                        else
                            delay <= delay - 1;
                        end if;

                    when others =>
                        state <= S_RESET;
                end case;
            end if;
        end if;
    end process;
end rtl;

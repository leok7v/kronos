library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Suspend-to-RAM sequencer for the OneChipBook Kronos.
--
-- Decides WHEN to sleep and drives the handshake that parks the SDRAM in
-- self-refresh (sdram_controller suspend/suspended) and freezes the CPU.
--
-- TIMER. Inactivity is counted in SECONDS, incremented by the `sec_tick` 1 Hz
-- pulse and reset by `activity`. It sleeps once `inact` reaches the RUNTIME
-- threshold `timeout_sec` (written by the OS via an I/O register). Seconds, not
-- clocks, because a 20-minute timeout is ~31 billion clocks and will not fit a
-- 32-bit counter -- and because seconds are the natural unit for the OS to set.
-- timeout_sec = 0 DISABLES auto-sleep.
--
-- WHY NOT gate on cpu_idle: this OS busy-polls the console and never executes
-- the IDLE instruction, so cpu_idle never asserts (confirmed on hardware). The
-- IDLE_GATED generic is kept (default false) for a CPU/OS that does idle.
--
-- HANDSHAKE. ENTER asserts suspend+halt and waits for `sdc_suspended`. On a
-- keypress we deassert suspend but KEEP halt through EXIT until `sdc_suspended`
-- clears (CKE high, tXSR elapsed, refreshed); only then is it safe to run again.

entity suspend_ctrl is
    generic (
        -- Require cpu_idle as well as inactivity before sleeping? False here --
        -- see the header note.
        IDLE_GATED : boolean := false
    );
    port (
        clk           : in  std_logic;
        reset         : in  std_logic;
        cpu_idle      : in  std_logic;                       -- CPU parked in IDLE
        activity      : in  std_logic;                       -- key or console output
        sleep_now     : in  std_logic;                       -- sleep NOW (the `sleep` command)
        sec_tick      : in  std_logic;                       -- 1 Hz inactivity increment
        timeout_sec   : in  std_logic_vector(15 downto 0);   -- sleep threshold (s); 0=off
        sdc_suspended : in  std_logic;                       -- SDRAM parked/servicing
        cpu_halt      : out std_logic;                       -- freeze the CPU while asleep
        sdc_suspend   : out std_logic;                       -- request SDRAM self-refresh
        sleeping      : out std_logic;                       -- asleep: mask timer / status
        -- diagnostic: inactivity past 1/4, 1/2, 3/4 of the (runtime) threshold.
        dbg_ramp      : out std_logic_vector(2 downto 0)
    );
end suspend_ctrl;

architecture rtl of suspend_ctrl is
    type st_t is (S_RUN, S_ENTER, S_SLEEP, S_EXIT, S_WAKE);
    signal state : st_t := S_RUN;
    signal inact : unsigned(15 downto 0) := (others => '0');   -- seconds of inactivity
    signal tmo   : unsigned(15 downto 0);
begin
    tmo <= unsigned(timeout_sec);
    sleeping    <= '1' when (state = S_ENTER or state = S_SLEEP or state = S_EXIT) else '0';
    dbg_ramp(0) <= '1' when inact >= shift_right(tmo, 2) else '0';                    -- >= 1/4
    dbg_ramp(1) <= '1' when inact >= shift_right(tmo, 1) else '0';                    -- >= 1/2
    dbg_ramp(2) <= '1' when inact >= (shift_right(tmo,1) + shift_right(tmo,2)) else '0'; -- >= 3/4

    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                state <= S_RUN; inact <= (others => '0');
                cpu_halt <= '0'; sdc_suspend <= '0';
            else
                case state is
                    when S_RUN =>
                        cpu_halt <= '0'; sdc_suspend <= '0';
                        if sleep_now = '1' then
                            state <= S_ENTER;                   -- `sleep` command: sleep now
                        elsif activity = '1' then
                            inact <= (others => '0');           -- user/OS active; stay awake
                        else
                            if sec_tick = '1' and inact /= x"FFFF" then
                                inact <= inact + 1;
                            end if;
                            -- sleep once inactive long enough (timeout 0 disables),
                            -- and if IDLE_GATED also require the CPU parked now.
                            if tmo /= 0 and inact >= tmo
                               and (cpu_idle = '1' or not IDLE_GATED) then
                                state <= S_ENTER;
                            end if;
                        end if;

                    when S_ENTER =>
                        sdc_suspend <= '1'; cpu_halt <= '1';
                        if sdc_suspended = '1' then state <= S_SLEEP; end if;

                    when S_SLEEP =>
                        sdc_suspend <= '1'; cpu_halt <= '1';
                        if activity = '1' then state <= S_EXIT; end if;   -- a key wakes it

                    when S_EXIT =>
                        sdc_suspend <= '0'; cpu_halt <= '1';
                        if sdc_suspended = '0' then state <= S_WAKE; end if;

                    when S_WAKE =>
                        sdc_suspend <= '0'; cpu_halt <= '0';
                        inact <= (others => '0');
                        state <= S_RUN;
                end case;
            end if;
        end if;
    end process;
end rtl;

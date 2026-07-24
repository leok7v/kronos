library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

-- Print the performance counters to the serial console, periodically.
--
-- WHY NOT THE LEDs. The counters were first exposed on the DIP/LED readout and
-- it was the wrong instrument three times over:
--   * the metrics re-latch every ~2ms, so the low bits are a visible blur --
--     you cannot read a number that is still moving;
--   * reading 8 LEDs as binary, three times, is tedious and error-prone;
--   * the index needed switches SW1/SW2, which belong to the board's VGA
--     line-scan generator and cannot be moved without corrupting the display.
-- Printing the numbers as text removes all three problems at once, and lands
-- them straight in a host-side UART capture where they can be diffed run to run.
--
-- Output, once every PERIOD clocks:
--
--     \r\nPERF SR=xx CYC=xx RT=xx\r\n
--
-- in HEX, because hex is a shift-and-lookup in hardware while decimal needs
-- division. Scaling (all deliberately shift-derived, see OneChipBook):
--   SR  same-row fraction   00..FF = 0..100%
--   CYC average cycles per miss, in 1/16ths -- divide by 16 (A0 = 10.0)
--   RT  misses per 2^20 clocks, divided by 256
--
-- Transmission is strictly best-effort and NEVER blocks the CPU: a byte is
-- offered only when the transmitter is idle AND the CPU is not writing the
-- console this cycle, so the OS's own output always wins. The worst case is a
-- mangled report line, never a stalled machine or a lost OS character.

entity perf_report is
    generic (
        -- ~2s at 21.48MHz. Long enough not to intrude on the console, often
        -- enough to watch a metric move during a benchmark run.
        PERIOD : integer := 43000000);
    port (
        clk      : in  std_logic;
        reset    : in  std_logic;
        enable   : in  std_logic := '1';
        samerow  : in  std_logic_vector(7 downto 0);
        avgcyc   : in  std_logic_vector(7 downto 0);
        txnrate  : in  std_logic_vector(7 downto 0);
        tx_ready : in  std_logic;    -- transmitter idle
        tx_ack   : in  std_logic;    -- OUR byte was loaded (not the CPU's)
        cpu_busy : in  std_logic;    -- CPU is writing the console this cycle
        tx_byte  : out std_logic_vector(7 downto 0);
        tx_stb   : out std_logic);
end perf_report;

architecture rtl of perf_report is
    constant LAST : integer := 25;
    signal idx    : integer range 0 to LAST + 1 := LAST + 1;   -- idle
    signal tick   : integer range 0 to PERIOD - 1 := 0;
    -- snapshot at the start of a line, so one report is internally consistent
    -- even though the counters keep updating while it is being sent
    signal s_sr, s_cy, s_rt : std_logic_vector(7 downto 0) := (others => '0');
    -- HANDSHAKE. Two failure modes were hit on hardware getting this right, and
    -- they are opposites:
    --
    --   1. Advancing on tx_ready alone DROPPED EVERY SECOND CHARACTER. tx_stb is
    --      registered, so tx_ready is still high the cycle after a byte is
    --      offered -- the transmitter has not seen the strobe yet -- and the
    --      next byte was issued against that stale value, hit a busy
    --      transmitter and vanished. "PERF SR=41" printed as "PR R4".
    --
    --   2. Adding a `pending` flag cleared only by tx_ready going low DEADLOCKED
    --      the reporter completely (zero output). dl11 accepts injection in an
    --      ELSIF *after* the CPU register access, so if con_stb is high in the
    --      cycle the strobe lands -- one cycle after cpu_busy was sampled -- the
    --      byte is REFUSED. tx_ready then never falls, pending never clears, and
    --      it waits forever. The OS writes the console constantly at boot, so
    --      this happened within milliseconds.
    --
    -- And a third trap, which the hardware showed by only producing output WHILE
    -- KEYS WERE BEING TYPED: tx_ready falling is NOT proof our byte was taken --
    -- it also falls when the CPU transmits. Inferring acceptance from it makes a
    -- refused byte look accepted, so it is silently lost, and the reporter only
    -- limps forward on the OS's console traffic.
    --
    -- So: OFFER the current byte every cycle the transmitter looks free, and
    -- advance only on tx_ack, which dl11 pulses when it loads an INJECTED byte
    -- specifically. Unambiguous: a refusal just retries next cycle, nothing is
    -- dropped, and nothing can hang.

    function hex(v : std_logic_vector(3 downto 0)) return std_logic_vector is
    begin
        if conv_integer(unsigned(v)) < 10 then
            return conv_std_logic_vector(48 + conv_integer(unsigned(v)), 8);
        else
            return conv_std_logic_vector(55 + conv_integer(unsigned(v)), 8);
        end if;
    end function;

    function ch(c : character) return std_logic_vector is
    begin
        return conv_std_logic_vector(character'pos(c), 8);
    end function;
begin

    process (clk)
        variable b : std_logic_vector(7 downto 0);
    begin
        if rising_edge(clk) then
            tx_stb <= '0';

            if reset = '1' then
                idx <= LAST + 1; tick <= 0;
            else
                if idx > LAST then                    -- idle: wait for the period
                    if enable = '0' then
                        -- Disabled: hold the counter reset and never start a
                        -- line. This is a clean gate, not just a suppressed
                        -- fire -- letting tick free-run would step past its
                        -- `0 to PERIOD-1` range (a harmless wrap on silicon, but
                        -- a range error the instant a strict sim runs disabled).
                        tick <= 0;
                    elsif tick = PERIOD - 1 then
                        tick  <= 0;
                        s_sr  <= samerow;             -- snapshot
                        s_cy  <= avgcyc;
                        s_rt  <= txnrate;
                        idx   <= 0;
                    else
                        tick <= tick + 1;
                    end if;
                else
                    -- advance only on PROOF that OUR byte was loaded
                    if tx_ack = '1' then
                        idx <= idx + 1;
                    end if;

                    case idx is
                        when  0 => b := ch(CR);
                        when  1 => b := ch(LF);
                        when  2 => b := ch('P');
                        when  3 => b := ch('E');
                        when  4 => b := ch('R');
                        when  5 => b := ch('F');
                        when  6 => b := ch(' ');
                        when  7 => b := ch('S');
                        when  8 => b := ch('R');
                        when  9 => b := ch('=');
                        when 10 => b := hex(s_sr(7 downto 4));
                        when 11 => b := hex(s_sr(3 downto 0));
                        when 12 => b := ch(' ');
                        when 13 => b := ch('C');
                        when 14 => b := ch('Y');
                        when 15 => b := ch('=');
                        when 16 => b := hex(s_cy(7 downto 4));
                        when 17 => b := hex(s_cy(3 downto 0));
                        when 18 => b := ch(' ');
                        when 19 => b := ch('R');
                        when 20 => b := ch('T');
                        when 21 => b := ch('=');
                        when 22 => b := hex(s_rt(7 downto 4));
                        when 23 => b := hex(s_rt(3 downto 0));
                        when 24 => b := ch(CR);
                        when others => b := ch(LF);
                    end case;
                    -- offer it; a refusal just means we try again next cycle
                    tx_byte <= b;
                    if tx_ready = '1' and cpu_busy = '0' then
                        tx_stb <= '1';
                    end if;
                end if;
            end if;
        end if;
    end process;
end architecture rtl;

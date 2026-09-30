library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;

-- Prove the DL11 receiver IGNORES crosstalk, and still accepts real frames.
--
-- THE BUG THIS EXISTS FOR (found on hardware 2026-07-20). uart_txd is PIN_3 and
-- uart_rxd is PIN_2 -- adjacent on the DB9. With nothing plugged in, the ~25k
-- weak pull-up cannot hold the line against capacitive coupling from TX
-- switching next to it, and the receiver used to begin a frame on a SINGLE low
-- sample: a 46ns glitch. Enough of those assemble into well-formed frames, and
-- because every phantom character makes the OS print, printing produces more:
--
--     OS prints -> TX toggles -> phantom char -> OS prints -> louder
--
-- On the board that is an endless stream of shell prompts which no keystroke
-- can stop. It disappears the instant a USB-TTL is plugged in, because the
-- adapter drives the line from a low impedance -- which is why it never once
-- reproduced while a capture cable was attached, and why it was mistaken in
-- turn for a cache bug, an OS bug, and a PS/2 bug.
--
-- The fix mirrors the PS/2 receiver: require 8 stable samples before believing
-- a level, and re-check the start bit at the half-bit point. A bit is
-- BAUD_DIV = 373 clocks, so the filter costs 2% of a bit.
--
-- The negative control matters as much as the positive one: a filter that
-- rejected EVERYTHING would pass a glitch test trivially, so this also sends
-- real frames and requires them through.

entity tb_dl11_glitch is
end tb_dl11_glitch;

architecture sim of tb_dl11_glitch is
    constant PERIOD   : time := 46561 ps;          -- 21.47727 MHz
    constant BAUD_DIV : integer := 373;
    constant BIT_T    : time := PERIOD * BAUD_DIV; -- one 57600 bit

    signal clk   : std_logic := '0';
    signal reset : std_logic := '1';
    signal done  : boolean := false;

    signal reg_adr   : std_logic_vector(1 downto 0) := "00";
    signal reg_dat_i : std_logic_vector(31 downto 0) := (others => '0');
    signal reg_dat_o : std_logic_vector(31 downto 0);
    signal reg_stb, reg_we : std_logic := '0';
    signal rxd : std_logic := '1';                 -- idle high
    signal rx_irq, tx_irq, txd : std_logic;

    signal errors : integer := 0;
begin
    clk <= not clk after PERIOD / 2 when not done else '0';

    dut : entity work.dl11_console
        generic map (BAUD_DIV => BAUD_DIV)
        port map (clk => clk, reset => reset,
                  reg_adr => reg_adr, reg_dat_i => reg_dat_i,
                  reg_dat_o => reg_dat_o, reg_stb => reg_stb, reg_we => reg_we,
                  rx_irq => rx_irq, tx_irq => tx_irq,
                  uart_txd => txd, uart_rxd => rxd,
                  rx_inj => (others => '0'), rx_inj_stb => '0');

    stim : process
        procedure send(b : std_logic_vector(7 downto 0)) is
        begin
            rxd <= '0'; wait for BIT_T;
            for i in 0 to 7 loop
                rxd <= b(i); wait for BIT_T;
            end loop;
            rxd <= '1'; wait for BIT_T * 2;
        end procedure;

        -- a capacitive glitch: the line dips for n clocks, then recovers
        procedure glitch(n : integer) is
        begin
            rxd <= '0'; wait for PERIOD * n; rxd <= '1';
        end procedure;

        procedure rd(adr : std_logic_vector(1 downto 0);
                     v : out std_logic_vector(31 downto 0)) is
        begin
            reg_adr <= adr; reg_stb <= '1'; reg_we <= '0';
            wait until rising_edge(clk);
            v := reg_dat_o;
            reg_stb <= '0';
            wait until rising_edge(clk);
        end procedure;

        procedure check(ok : boolean; msg : string) is
        begin
            if ok then report "PASS: " & msg;
            else report "FAIL: " & msg severity error; errors <= errors + 1; end if;
        end procedure;

        variable v : std_logic_vector(31 downto 0);
    begin
        wait for PERIOD * 10; reset <= '0'; wait for PERIOD * 20;

        -- 1 -------------------------------------------- single-clock glitch
        -- This is the one that used to start a frame outright.
        glitch(1); wait for BIT_T * 12;
        rd("00", v);
        check(v(7) = '0', "a 1-clock glitch does not start a frame");

        -- 2 ------------------------------------- glitch burst, crosstalk-like
        -- TX switching next door produces a train of short dips, not one.
        for i in 0 to 20 loop
            glitch(2); wait for PERIOD * 30;
        end loop;
        wait for BIT_T * 12;
        rd("00", v);
        check(v(7) = '0', "a burst of 2-clock glitches invents no character");

        -- 3 ---------------------------------- glitch shorter than the filter
        glitch(7); wait for BIT_T * 12;
        rd("00", v);
        check(v(7) = '0', "a 7-clock dip (under the 8-sample filter) is rejected");

        -- 4 ------------------------------------------------ false start bit
        -- Long enough to pass the filter, but gone before the half-bit re-check.
        rxd <= '0'; wait for BIT_T / 4; rxd <= '1';
        wait for BIT_T * 12;
        rd("00", v);
        check(v(7) = '0', "a start bit that vanishes before mid-bit is rejected");

        -- 5 ------------------------ NEGATIVE CONTROL: real frames still work
        -- Without this, a receiver that ignored everything would 'pass' above.
        send(x"41");
        rd("00", v);
        check(v(7) = '1', "a REAL frame still sets rx_avail");
        rd("01", v);
        check(v(7 downto 0) = x"41", "and delivers the right byte ('A')");
        rd("00", v);
        check(v(7) = '0', "reading RBUF clears rx_avail");

        send(x"7A");
        rd("01", v);
        check(v(7 downto 0) = x"7A", "a second real frame arrives ('z')");

        -- 6 ------------------------- a real frame immediately after a glitch
        glitch(3); wait for PERIOD * 40;
        send(x"35");
        rd("01", v);
        check(v(7 downto 0) = x"35", "a real frame right after a glitch is intact");

        if errors = 0 then
            report "*** PASS: crosstalk glitches rejected, real frames unaffected ***";
        else
            report "*** FAIL: DL11 glitch rejection is broken ***" severity failure;
        end if;
        done <= true;
        wait;
    end process;
end architecture sim;

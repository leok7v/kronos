library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

-- Prove perf_report actually transmits a COMPLETE line through dl11_console.
--
-- The first hardware attempt printed "PR R4 YD TD" instead of
-- "PERF SR=41 CY=A0 RT=27" -- every SECOND character was dropped. tx_stb is
-- registered, so tx_ready is still high the cycle after a byte is offered (the
-- transmitter has not seen the strobe yet); the next byte was then issued
-- against that stale tx_ready, hit a busy transmitter and vanished. A one-flash
-- round trip to discover a handshake race that a testbench catches in seconds.
--
-- This decodes the real serial line rather than peeking at internal signals, so
-- it exercises the same path the host does.

entity tb_perf_report is
end tb_perf_report;

architecture sim of tb_perf_report is
    constant PERIOD   : time := 46561 ps;
    constant BAUD_DIV : integer := 373;
    constant BIT_T    : time := PERIOD * BAUD_DIV;

    signal clk   : std_logic := '0';
    signal reset : std_logic := '1';
    signal done  : boolean := false;

    signal txd, tx_idle, perf_stb, perf_ack : std_logic;
    signal perf_ch : std_logic_vector(7 downto 0);
    signal rx_irq, tx_irq : std_logic;
    signal reg_dat_o : std_logic_vector(31 downto 0);

    -- the counter values under test
    constant SR : std_logic_vector(7 downto 0) := X"41";
    constant CY : std_logic_vector(7 downto 0) := X"A0";
    constant RT : std_logic_vector(7 downto 0) := X"27";

    signal got : string(1 to 64) := (others => ' ');
    signal ngot : integer := 0;

    -- CONTENTION. The first version of this testbench tied cpu_busy low, so it
    -- never exercised a REFUSED injection -- and refusal is exactly what
    -- deadlocked the reporter on hardware (dl11 accepts injection only in an
    -- ELSIF after the CPU register access). A testbench that cannot reproduce
    -- the contention cannot catch the bug, and this one duly passed a design
    -- that produced zero output on the board. It now asserts cpu_busy in a
    -- lumpy, irregular pattern so refusals genuinely happen.
    signal busy_ctr : integer := 0;
    signal cpu_busy : std_logic := '0';
begin

    harass : process (clk)
    begin
        if rising_edge(clk) then
            busy_ctr <= (busy_ctr + 1) mod 37;
            -- high roughly 1 cycle in 3, in bursts, at a period coprime with
            -- anything in the reporter so the two cannot fall into lockstep
            if busy_ctr < 13 then cpu_busy <= '1'; else cpu_busy <= '0'; end if;
        end if;
    end process;

    clk <= not clk after PERIOD / 2 when not done else '0';

    -- report every 20000 clocks so the test runs quickly
    rep : entity work.perf_report
        generic map (PERIOD => 20000)
        port map (clk => clk, reset => reset,
                  samerow => SR, avgcyc => CY, txnrate => RT,
                  tx_ready => tx_idle, tx_ack => perf_ack, cpu_busy => cpu_busy,
                  tx_byte => perf_ch, tx_stb => perf_stb);

    con : entity work.dl11_console
        generic map (BAUD_DIV => BAUD_DIV)
        port map (clk => clk, reset => reset,
                  reg_adr => "00", reg_dat_i => (others => '0'),
                  reg_dat_o => reg_dat_o, reg_stb => '0', reg_we => '0',
                  rx_irq => rx_irq, tx_irq => tx_irq,
                  uart_txd => txd, uart_rxd => '1',
                  tx_inj => perf_ch, tx_inj_stb => perf_stb, tx_idle => tx_idle,
                  tx_inj_ack => perf_ack);

    -- decode the serial line exactly as the host would
    rxproc : process
        variable b : std_logic_vector(7 downto 0);
    begin
        wait until falling_edge(txd);          -- start bit
        wait for BIT_T * 3 / 2;                -- middle of bit 0
        for i in 0 to 7 loop
            b(i) := txd;
            wait for BIT_T;
        end loop;
        if ngot < 64 then
            ngot <= ngot + 1;
            got(ngot + 1) <= character'val(conv_integer(unsigned(b)));
        end if;
    end process;

    stim : process
        variable want : string(1 to 22) := "PERF SR=41 CY=A0 RT=27";
        variable ok : boolean;
    begin
        wait for PERIOD * 10; reset <= '0';
        -- one report is 26 chars at ~174us each
        wait for BIT_T * 12 * 30;
        report "received: [" & got(1 to 30) & "]";
        ok := false;
        for s in 1 to 40 loop
            if got(s to s + 21) = want then ok := true; end if;
        end loop;
        if ok then
            report "*** PASS: the full line was transmitted intact ***";
        else
            report "*** FAIL: line mangled -- characters are being dropped ***"
                severity failure;
        end if;
        done <= true;
        wait;
    end process;
end architecture sim;

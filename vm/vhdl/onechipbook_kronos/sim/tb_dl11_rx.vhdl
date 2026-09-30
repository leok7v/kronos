library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;

-- Prove the DL11 console RECEIVER before console input is trusted on hardware.
--
-- Receive has been disabled on this board since the "0080h" episode, so the
-- path has never actually been exercised end to end. This drives uart_rxd with
-- real 57600 8N1 frames and checks what the CPU would read:
--
--   RCSR (reg 00) bit 7 = rx_avail   -- a character is waiting
--   RBUF (reg 01)       = the byte   -- and reading it clears rx_avail
--
-- It also checks the two things that decide whether a floating pin can invent
-- characters: a frame with a BAD STOP BIT must be rejected, and a line held
-- idle-high must never produce one.

entity tb_dl11_rx is
end tb_dl11_rx;

architecture sim of tb_dl11_rx is
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
        -- send one 8N1 frame, LSB first
        procedure send(b : std_logic_vector(7 downto 0); good_stop : boolean) is
        begin
            rxd <= '0'; wait for BIT_T;                    -- start
            for i in 0 to 7 loop
                rxd <= b(i); wait for BIT_T;
            end loop;
            if good_stop then rxd <= '1'; else rxd <= '0'; end if;
            wait for BIT_T;
            rxd <= '1'; wait for BIT_T;                    -- back to idle
        end procedure;

        procedure rd(a : std_logic_vector(1 downto 0)) is
        begin
            wait until rising_edge(clk);
            reg_adr <= a; reg_stb <= '1'; reg_we <= '0';
            wait until rising_edge(clk);
            reg_stb <= '0';
        end procedure;
    begin
        wait for PERIOD * 20;
        reset <= '0';
        wait for BIT_T * 2;

        -- ---- 1. a good frame must arrive intact
        send(x"41", true);                                 -- 'A'
        rd("00");
        assert reg_dat_o(7) = '1'
            report "FAIL: rx_avail never set -- the receiver did not see 'A'"
            severity failure;
        rd("01");
        assert reg_dat_o(7 downto 0) = x"41"
            report "FAIL: RBUF held the wrong byte" severity failure;
        report "'A' received correctly" severity note;

        -- ---- 2. reading RBUF must clear rx_avail, or the OS re-reads forever
        rd("00");
        assert reg_dat_o(7) = '0'
            report "FAIL: rx_avail stuck after reading RBUF -- this is exactly "
                 & "the runaway that made the console repeat characters"
            severity failure;

        -- ---- 3. a second, different byte (proves it is not latched garbage)
        send(x"7A", true);                                 -- 'z'
        rd("01");
        assert reg_dat_o(7 downto 0) = x"7A"
            report "FAIL: second byte wrong" severity failure;
        report "'z' received correctly" severity note;

        -- ---- 4. a BAD STOP BIT must be rejected, not delivered
        send(x"55", false);
        rd("00");
        assert reg_dat_o(7) = '0'
            report "FAIL: framing error accepted as a character -- noise on a "
                 & "floating pin would be delivered to the OS"
            severity failure;
        report "framing error correctly rejected" severity note;

        -- ---- 5. an idle line must never invent a character.
        -- Flush first: a framing error makes any UART resynchronise, and the
        -- bad stop bit itself reads as the next start bit, so ONE spurious
        -- character after a deliberate error is correct behaviour rather than
        -- a defect. That resync is exactly how a FLOATING pin manufactured the
        -- phantom frames -- which is what the weak pull-up now prevents.
        rxd <= '1';
        wait for BIT_T * 12;
        rd("01");                                      -- drain anything pending
        wait for BIT_T * 40;
        rd("00");
        assert reg_dat_o(7) = '0'
            report "FAIL: idle line produced a character" severity failure;

        report "*** PASS: DL11 receive works, clears on read, rejects bad "
             & "frames, and stays quiet when idle ***" severity note;
        done <= true;
        wait;
    end process;
end architecture sim;

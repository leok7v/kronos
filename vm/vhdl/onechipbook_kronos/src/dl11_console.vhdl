library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- DL11-style serial console for the Kronos VM I/O convention.
--
-- The Excelsior booter/OS drive the console through inp/out (opcodes 0x90/0x91)
-- at I/O registers 177560b..177566b DIV 2 = 0x7FB8..0x7FBB; the new microcode
-- masks the register number to 12 bits and adds the I/O page base, so this
-- block appears at word addresses 0x7FFB8..0x7FFBB. Register semantics match
-- the VM (vm/int/src/SIO.cpp cI::inp/out):
--   +0 RCSR (r/w) bit7 = rx char available, bit6 = rx interrupt enable (stored)
--   +1 RBUF (r)   received byte; reading clears rx-available
--   +2 XCSR (r/w) bit7 = tx ready,          bit6 = tx interrupt enable (stored)
--   +3 XBUF (w)   byte to transmit
--
-- Physical side: minimal 8N1 UART on the OneChipBook DB9 (57600 by default).
-- tx_byte/tx_strobe mirror every transmitted byte so a testbench (or a future
-- LCD/VGA console) can observe the output stream without decoding serial.

entity dl11_console is
    generic (
        BAUD_DIV : integer := 373            -- 21.47727 MHz / 57600
    );
    port (
        clk       : in  std_logic;
        reset     : in  std_logic;
        -- register interface (decoded strobe; adr = word offset 0..3)
        reg_adr   : in  std_logic_vector(1 downto 0);
        reg_dat_i : in  std_logic_vector(31 downto 0);
        reg_dat_o : out std_logic_vector(31 downto 0);
        reg_stb   : in  std_logic;           -- one pulse per access
        reg_we    : in  std_logic;
        -- interrupt request lines (level; enabled + pending)
        rx_irq    : out std_logic;
        tx_irq    : out std_logic;
        -- serial pins
        uart_txd  : out std_logic;
        uart_rxd  : in  std_logic;
        -- local keyboard injection (PS/2, M5): delivers a byte straight into
        -- the receiver as if it had arrived on the wire. Defaulted so the
        -- serial-only instantiations need no change.
        rx_inj    : in  std_logic_vector(7 downto 0) := (others => '0');
        rx_inj_stb : in std_logic := '0';
        -- HARDWARE transmit injection, mirroring rx_inj on the receive side:
        -- lets on-chip logic (perf_report) print to the console without the CPU
        -- or the OS being involved. Accepted only when the transmitter is idle,
        -- and a CPU write always takes priority, so the OS can never lose a
        -- character to it. Defaulted off for the serial-only instantiations.
        tx_inj    : in  std_logic_vector(7 downto 0) := (others => '0');
        tx_inj_stb : in std_logic := '0';
        tx_idle   : out std_logic;
        -- Pulses for exactly one cycle when an INJECTED byte was actually
        -- loaded. The injector must not infer this from tx_idle falling: that
        -- also falls when the CPU transmits, so a refused byte would look
        -- accepted and be silently lost.
        tx_inj_ack : out std_logic;
        -- debug taps
        tx_byte   : out std_logic_vector(7 downto 0);
        tx_strobe : out std_logic;
        -- diagnostics for the console-input question:
        --   dbg_rx_ien  = the OS has ENABLED the receive interrupt, i.e. it
        --                 wants interrupt-driven input rather than polling
        --   dbg_rbuf_rd = the OS has READ the receive buffer, i.e. it actually
        --                 consumed a character we injected
        dbg_rx_ien  : out std_logic;
        dbg_rbuf_rd : out std_logic
    );
end dl11_console;

architecture rtl of dl11_console is
    signal dbg_rbuf_rd_i : std_logic := '0';
    -- receive side
    signal rx_avail : std_logic := '0';
    signal rx_data  : std_logic_vector(7 downto 0) := (others => '0');
    signal rx_ien   : std_logic := '0';
    -- transmit side
    signal tx_ready : std_logic := '1';
    signal tx_ien   : std_logic := '0';

    -- UART TX engine
    signal txd_r     : std_logic := '1';
    signal tx_shift  : std_logic_vector(9 downto 0) := (others => '1');
    signal tx_bits   : integer range 0 to 10 := 0;
    signal tx_baud   : integer range 0 to BAUD_DIV-1 := 0;

    -- UART RX engine (mid-bit sampling)
    --
    -- GLITCH FILTER -- this is not belt-and-braces, it fixes a real failure.
    -- uart_txd is PIN_3 and uart_rxd is PIN_2: physically adjacent on the DB9.
    -- With nothing plugged in, the ~25k weak pull-up is no match for capacitive
    -- coupling from TX switching right next to it, and the receiver used to
    -- start a frame on a SINGLE low sample of rxd_s2 -- a 46ns glitch. Enough of
    -- those form a well-formed frame that the console feeds itself phantom
    -- characters, and since each one makes the OS print, printing makes more:
    --   OS prints -> TX toggles -> phantom char -> OS prints -> louder.
    -- On hardware that appears as an endless stream of shell prompts that no
    -- keystroke can stop, and it VANISHES the moment a USB-TTL is plugged in,
    -- because the adapter drives the line from a low impedance. That is exactly
    -- why it never reproduced while the capture cable was attached.
    --
    -- The PS/2 receiver has required 8 stable samples for this same reason
    -- ("a metre of keyboard cable picks up ringing"); the UART input never got
    -- the same treatment. A bit is BAUD_DIV = 373 clocks, so 8 samples costs
    -- 2% of a bit and rejects anything shorter than ~370ns.
    signal tx_inj_ack_i : std_logic := '0';
    signal rxd_s1, rxd_s2 : std_logic := '1';
    signal rxd_hist  : std_logic_vector(7 downto 0) := (others => '1');
    signal rxd_f     : std_logic := '1';   -- filtered line level
    signal rx_bits   : integer range 0 to 9 := 0;
    signal rx_baud   : integer range 0 to BAUD_DIV+BAUD_DIV/2 := 0;
    signal rx_shift  : std_logic_vector(7 downto 0) := (others => '0');
    signal rx_active : std_logic := '0';
begin
    uart_txd <= txd_r;
    rx_irq <= rx_avail and rx_ien;
    dbg_rx_ien  <= rx_ien;
    dbg_rbuf_rd <= dbg_rbuf_rd_i;
    tx_irq <= tx_ready and tx_ien;
    tx_idle <= tx_ready;
    tx_inj_ack <= tx_inj_ack_i;

    reg_dat_o <=
        (7 => rx_avail, 6 => rx_ien, others => '0') when reg_adr = "00" else
        X"000000" & rx_data                         when reg_adr = "01" else
        (7 => tx_ready, 6 => tx_ien, others => '0') when reg_adr = "10" else
        (others => '0');

    process (clk)
    begin
        if rising_edge(clk) then
            tx_strobe <= '0';
            tx_inj_ack_i <= '0';
            rxd_s1 <= uart_rxd; rxd_s2 <= rxd_s1;   -- 2FF synchronizer
            -- ...then require 8 stable samples before believing a level change
            rxd_hist <= rxd_hist(6 downto 0) & rxd_s2;
            if rxd_hist = x"FF" then
                rxd_f <= '1';
            elsif rxd_hist = x"00" then
                rxd_f <= '0';
            end if;

            if reset = '1' then
                rx_avail <= '0'; rx_ien <= '0'; tx_ien <= '0';
                tx_ready <= '1'; txd_r <= '1'; tx_bits <= 0;
                rx_active <= '0'; rx_bits <= 0;
            else
                -- ------------- register accesses
                if reg_stb = '1' then
                    if reg_we = '1' then
                        case reg_adr is
                            when "00" => rx_ien <= reg_dat_i(6);
                            when "10" => tx_ien <= reg_dat_i(6);
                            when "11" =>
                                if tx_ready = '1' then
                                    tx_shift <= '1' & reg_dat_i(7 downto 0) & '0'; -- stop,data,start
                                    tx_bits  <= 10; tx_baud <= 0;
                                    tx_ready <= '0';
                                    tx_byte   <= reg_dat_i(7 downto 0);
                                    tx_strobe <= '1';
                                end if;
                            when others => null;
                        end case;
                    else
                        if reg_adr = "01" then           -- reading RBUF consumes
                            rx_avail <= '0';
                            dbg_rbuf_rd_i <= '1';
                        end if;
                    end if;
                elsif tx_inj_stb = '1' and tx_ready = '1' then
                    -- Hardware-injected byte. Deliberately in the ELSIF: a CPU
                    -- register access this cycle wins outright, so on-chip
                    -- diagnostics can never displace an OS character.
                    tx_shift <= '1' & tx_inj & '0';      -- stop, data, start
                    tx_bits  <= 10; tx_baud <= 0;
                    tx_ready <= '0';
                    tx_byte   <= tx_inj;
                    tx_strobe <= '1';
                    tx_inj_ack_i <= '1';                 -- THIS byte was taken
                end if;

                -- ------------- UART TX
                if tx_bits /= 0 then
                    if tx_baud = BAUD_DIV-1 then
                        tx_baud  <= 0;
                        txd_r    <= tx_shift(0);
                        tx_shift <= '1' & tx_shift(9 downto 1);
                        if tx_bits = 1 then
                            tx_ready <= '1';
                        end if;
                        tx_bits <= tx_bits - 1;
                    else
                        tx_baud <= tx_baud + 1;
                    end if;
                end if;

                -- ------------- UART RX  (all decisions use the FILTERED level)
                if rx_active = '0' then
                    if rxd_f = '0' then                  -- start bit edge
                        rx_active <= '1';
                        rx_bits <= 0;
                        rx_baud <= 0;
                    end if;
                else
                    -- FALSE-START REJECTION. A real start bit is low for a whole
                    -- bit time, so re-check at the half-bit point and abandon the
                    -- frame if the line has already gone back high. Without this
                    -- the start bit is never validated at all: the first sample
                    -- lands at 1.5 bit times, which is the middle of DATA bit 0.
                    if rx_bits = 0 and rx_baud = BAUD_DIV/2 - 1 and rxd_f = '1' then
                        rx_active <= '0';
                        rx_baud   <= 0;
                    -- first sample point: 1.5 bit times after the start edge,
                    -- then one bit time between samples
                    elsif (rx_bits = 0 and rx_baud = BAUD_DIV+BAUD_DIV/2-1)
                       or (rx_bits /= 0 and rx_baud = BAUD_DIV-1) then
                        rx_baud <= 0;
                        if rx_bits = 8 then              -- stop bit position
                            rx_active <= '0';
                            if rxd_f = '1' then          -- valid frame
                                rx_data  <= rx_shift;
                                rx_avail <= '1';
                            end if;
                        else
                            rx_shift <= rxd_f & rx_shift(7 downto 1);  -- LSB first
                            rx_bits  <= rx_bits + 1;
                        end if;
                    else
                        rx_baud <= rx_baud + 1;
                    end if;
                end if;

                -- LAST, so it takes priority over the RBUF-read clear above:
                -- a keystroke landing in the same cycle the OS reads RBUF must
                -- not be swallowed. (An unread character is still overwritten,
                -- exactly as a real DL11 overruns.)
                if rx_inj_stb = '1' then
                    rx_data  <= rx_inj;
                    rx_avail <= '1';
                end if;
            end if;
        end if;
    end process;
end rtl;

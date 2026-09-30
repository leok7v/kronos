library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

-- Expansion-slot pin-identification beacon (step 0 of the Tang Nano 9K card,
-- see vm/vhdl/tang9k_card/DESIGN.md).
--
-- Every expansion-slot GPIO transmits ITS OWN FPGA pin number ("P122\r\n",
-- "P123\r\n", ...) forever at 2400 8N1. Touch a TTL-adapter RX probe to a slot
-- contact and the terminal names the FPGA pin -- a contact->pin map with no
-- continuity probing on the QFP240. Channel 42 sends "P003" on the console
-- DB9 TX (FPGA pin 3), the known-good path from the 2026-07-19 measurement:
-- read that first to prove the bitstream + decode scheme before probing.
-- LED1 = heartbeat. Flash with NO card in the slot; slot pins 48/50 carry
-- +/-12V from the supply -- keep the probe off those.
entity expansion_beacon is
    port (
        clk   : in  std_logic;                        -- PIN_28, 21.47727 MHz
        rst_n : in  std_logic;                        -- PIN_153
        led   : out std_logic_vector(8 downto 0);
        txp   : out std_logic_vector(43 downto 0)     -- 42 slot GPIOs + console TX
    );                                                -- + 121 (internal slot c4)
end expansion_beacon;

architecture rtl of expansion_beacon is
    constant BAUD_DIV : integer := 8949;              -- 21.47727 MHz / 2400
    constant NP       : integer := 44;

    type intarr is array (0 to NP-1) of integer;
    constant PINS : intarr := (
        122,123,124,125,126,127,128,131,132,133,134,135,136,137,138,
        139,140,141,143,144,156,158,159,160,161,162,163,164,165,166,
        167,168,169,170,173,174,175,176,177,178,179,180, 3, 121);

    type msg_t  is array (0 to 5) of std_logic_vector(7 downto 0);
    type msgs_t is array (0 to NP-1) of msg_t;

    function build_msgs return msgs_t is
        variable m : msgs_t;
        variable n : integer;
    begin
        for i in 0 to NP-1 loop
            n := PINS(i);
            m(i)(0) := x"50";                                          -- 'P'
            m(i)(1) := conv_std_logic_vector(48 + n/100, 8);
            m(i)(2) := conv_std_logic_vector(48 + (n/10) mod 10, 8);
            m(i)(3) := conv_std_logic_vector(48 + n mod 10, 8);
            m(i)(4) := x"0D";
            m(i)(5) := x"0A";
        end loop;
        return m;
    end;
    constant MSGS : msgs_t := build_msgs;

    type sh_arr is array (0 to NP-1) of std_logic_vector(9 downto 0);
    signal sh   : sh_arr := (others => (others => '1'));
    signal txr  : std_logic_vector(NP-1 downto 0) := (others => '1');
    signal hb   : std_logic_vector(24 downto 0) := (others => '0');
    signal bcnt : integer range 0 to BAUD_DIV-1 := 0;
    signal bidx : integer range 0 to 10 := 0;
    signal midx : integer range 0 to 5 := 0;
begin
    led(0) <= hb(23);
    led(8 downto 1) <= (others => '0');
    txp <= txr;

    -- One shared baud/bit/char sequencer; only the shift-register CONTENT
    -- differs per pin, so all 43 channels transmit in lockstep.
    process (clk) begin
        if rising_edge(clk) then
            hb <= hb + 1;
            if rst_n = '0' then
                bcnt <= 0; bidx <= 0; midx <= 0;
                txr <= (others => '1');
                sh  <= (others => (others => '1'));
            elsif bcnt = BAUD_DIV-1 then
                bcnt <= 0;
                if bidx = 0 then
                    for i in 0 to NP-1 loop
                        sh(i) <= '1' & MSGS(i)(midx) & '0';  -- stop|data|start
                        txr(i) <= '1';
                    end loop;
                    bidx <= 10;
                    if midx = 5 then midx <= 0; else midx <= midx + 1; end if;
                else
                    for i in 0 to NP-1 loop
                        txr(i) <= sh(i)(0);
                        sh(i)  <= '1' & sh(i)(9 downto 1);
                    end loop;
                    bidx <= bidx - 1;
                end if;
            else
                bcnt <= bcnt + 1;
            end if;
        end if;
    end process;
end rtl;

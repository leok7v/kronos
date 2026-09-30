library ieee;
use ieee.std_logic_1164.all;

-- Pixel-clock PLL for the 1024x768@60 text console: 21.47727 MHz in, 64.43 MHz
-- out (x3).
--
-- 1024x768@60 nominally wants 65 MHz, but a Cyclone I PLL's multiplier is
-- capped at 32 and its VCO must stay in 500..1000 MHz, which with this
-- oscillator leaves x3 (fvco 644 MHz / post 10) as the closest achievable
-- ratio -- Quartus rejects finer ones outright ("Can't implement clock
-- multiplication and clock division parameter values"). The console therefore
-- trims its blanking instead, giving 48.52 kHz line rate and 60.05 Hz refresh:
-- both within ~0.4% of spec, which any monitor locks to.
--
-- This synthesises the VIDEO clock ONLY. The system and SDRAM clocks keep
-- coming straight off the oscillator pin, untouched, which matters because the
-- SDRAM read-capture window on this board was tuned by hand (read_lat=3 was the
-- only value that ever worked). The oscillator pin drives both the global clock
-- network and this PLL's input, so nothing about the existing clocking changes.
--
-- USE_PLL=false substitutes the input clock for simulation: GHDL cannot
-- elaborate altpll. Video timing is then wrong (the console runs at 21.5 MHz,
-- so ~20 Hz refresh) but everything functional -- text, escapes, scrolling --
-- still behaves, and tb_vga_console drives the console with a real 65 MHz clock
-- directly to check the timing numbers.

entity video_pll is
    generic (
        USE_PLL : boolean := true);
    port (
        inclk  : in  std_logic;
        pclk   : out std_logic;
        locked : out std_logic);
end video_pll;

architecture rtl of video_pll is

    component altpll
        generic (
            bandwidth_type         : string  := "AUTO";
            clk0_divide_by         : natural;
            clk0_duty_cycle        : natural := 50;
            clk0_multiply_by       : natural;
            clk0_phase_shift       : string  := "0";
            compensate_clock       : string  := "CLK0";
            inclk0_input_frequency : natural;
            intended_device_family : string  := "Cyclone";
            lpm_type               : string  := "altpll";
            operation_mode         : string  := "NORMAL";
            port_clk0              : string  := "PORT_USED";
            port_inclk0            : string  := "PORT_USED";
            port_locked            : string  := "PORT_USED";
            width_clock            : natural := 6);
        port (
            inclk  : in  std_logic_vector(1 downto 0);
            clk    : out std_logic_vector(5 downto 0);
            locked : out std_logic;
            areset : in  std_logic);
    end component;

    signal clk_v : std_logic_vector(5 downto 0);

begin

    gen_pll : if USE_PLL generate
        u : altpll
            generic map (
                clk0_divide_by         => 1,
                clk0_multiply_by       => 3,        -- 21.47727 * 3 = 64.43 MHz
                inclk0_input_frequency => 46565,    -- ps period of 21.47727 MHz
                intended_device_family => "Cyclone",
                operation_mode         => "NORMAL")
            port map (
                inclk(0) => inclk, inclk(1) => '0',
                clk      => clk_v,
                locked   => locked,
                areset   => '0');
        pclk <= clk_v(0);
    end generate;

    gen_sim : if not USE_PLL generate
        pclk   <= inclk;
        locked <= '1';
    end generate;

end architecture rtl;

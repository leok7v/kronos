library ieee;
use ieee.std_logic_1164.all;

-- System clock PLL: multiplies the 21.47727 MHz board oscillator up to the CPU
-- clock. Modelled on video_pll, which already proves altpll works on this part.
--
-- WHY: measured on hardware, `dry` spends only ~22% of cycles stalled on SDRAM
-- and 78% executing. Every memory-side optimisation is therefore capped at
-- 1/(1-0.22) = 1.28x, while the clock scales the 78% as well -- it is the only
-- lever above that ceiling, and unlike a cache change it speeds up EVERY
-- workload, including the interactive ones that idle at ~68% stalled.
--
-- The gain is NOT the naive frequency ratio. SDRAM timings are in nanoseconds,
-- so a faster clock makes each miss cost proportionally MORE cycles and the
-- stall fraction grows: 0.78/1.25 + 0.22 = 0.844 -> about 1.19x for x5/4.
--
-- CAUTION -- the CPU logic is the easy part. The real hazard is SDRAM READ
-- CAPTURE. sd_clk is the same clock the controller runs on, and read_lat=3 was
-- historically "the only value that worked" on this board, so the capture
-- window is narrow. Shrinking the period shrinks it too. The likely failure is
-- therefore NOT a Quartus timing violation but a board that boots and then
-- reads garbage from SDRAM; the response is to retune SD_READ_LAT, or to add a
-- phase shift on the SDRAM clock output.
--
-- Deliberately NO phase shift here: sd_clk keeps the exact relationship it has
-- today, so frequency is the only variable changed from a known-good build.

entity system_pll is
    generic (
        MULT    : integer := 5;
        DIV     : integer := 4;
        -- GHDL cannot elaborate altpll, so simulation bypasses it. The sim then
        -- runs at the input frequency, which is correct for everything except
        -- absolute timing -- and the boot sim was never timing-accurate anyway.
        USE_PLL : boolean := true);
    port (
        inclk  : in  std_logic;
        outclk : out std_logic;
        locked : out std_logic);
end system_pll;

architecture rtl of system_pll is
    component altpll
        generic (
            clk0_divide_by         : natural;
            clk0_multiply_by       : natural;
            inclk0_input_frequency : natural;
            intended_device_family : string;
            operation_mode         : string);
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
                clk0_divide_by         => DIV,
                clk0_multiply_by       => MULT,
                inclk0_input_frequency => 46565,   -- ps period of 21.47727 MHz
                intended_device_family => "Cyclone",
                operation_mode         => "NORMAL")
            port map (
                inclk(0) => inclk, inclk(1) => '0',
                clk      => clk_v,
                locked   => locked,
                areset   => '0');
        outclk <= clk_v(0);
    end generate;

    gen_sim : if not USE_PLL generate
        outclk <= inclk;
        locked <= '1';
    end generate;

end architecture rtl;

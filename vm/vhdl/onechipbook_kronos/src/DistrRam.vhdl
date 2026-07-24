library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use ieee.std_logic_arith.all;

-- Inferred 16 x 1 dual-port distributed RAM (sync write, async dual-port read).
-- Replaces the Xilinx RAM16X1D hard-instantiation in common/DistrRam.vhdl.
-- Quartus infers this as LUT/MLAB RAM; GHDL simulates it directly -- one file
-- for both. DataStorage instantiates four of these (the Kronos evaluation stack).

entity DistrRam is
    port (
        D    : in std_logic;
        WE   : in std_logic;
        WCLK : in std_logic;
        A    : in std_logic_vector(3 downto 0);   -- write / port-A read address
        DPRA : in std_logic_vector(3 downto 0);   -- dual-port (async) read address
        DPO  : out std_logic
    );
end DistrRam;

architecture Behaviour of DistrRam is
    type ram_type is array (0 to 15) of std_logic;
    signal ram : ram_type := (others => '0');
begin
    process (WCLK)
    begin
        if rising_edge(WCLK) then
            if WE = '1' then
                ram(conv_integer(unsigned(A))) <= D;
            end if;
        end if;
    end process;

    DPO <= ram(conv_integer(unsigned(DPRA)));   -- asynchronous dual-port read
end architecture Behaviour;

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

-- Simulation BlockRam that is FAITHFUL TO CYCLONE M4K, including the parts that
-- make caching hard. GHDL cannot elaborate altsyncram, so this stands in for it
-- -- and until now it stood in DISHONESTLY: it returned clean read-first data,
-- so no simulation could ever reproduce the failures that killed the cache on
-- hardware. Every "verified in simulation" claim about this cache was hollow.
--
-- What the real M4K does (confirmed against Quartus's own report):
--
--   READ_DURING_WRITE_MODE_PORT_A/B      = NEW_DATA_NO_NBE_READ   (FORCED --
--       Quartus rejects OLD_DATA for the Cyclone family outright)
--   READ_DURING_WRITE_MODE_MIXED_PORTS   = DONT_CARE  (i.e. UNDEFINED)
--
-- So this model:
--   * same-port read-during-write  -> returns the NEW data (write-first)
--   * mixed-port read-during-write -> drives 'X' on the reading port
--   * a port with clocken low FREEZES: address register and output both hold
--
-- The 'X' is the whole point. Any design that depends on a mixed-port RDW
-- result now poisons its own datapath in simulation instead of quietly working
-- here and derailing on silicon.
--
-- The forwarding logic below mirrors src/BlockRam.vhdl exactly, so simulation
-- and synthesis agree: the mixed-port hazard is repaired identically in both,
-- and only the UNREPAIRED cases show up as 'X'.

-- Depth/width generics mirror src/BlockRam.vhdl so the 2-way cache can build
-- narrower, shallower arrays. Defaults are the original 512x32.
entity BlockRam is
    generic (
        ADDR_W : integer := 9;          -- depth = 2**ADDR_W
        DATA_W : integer := 32
    );
    port (
        clock  : in std_logic;
        reset  : in std_logic := '0';
        en0    : in std_logic;
        en1    : in std_logic;
        we0    : in std_logic;
        we1    : in std_logic;
        a0     : in std_logic_vector(ADDR_W-1 downto 0);
        a1     : in std_logic_vector(ADDR_W-1 downto 0);
        di0    : in std_logic_vector(DATA_W-1 downto 0);
        di1    : in std_logic_vector(DATA_W-1 downto 0);
        do0    : out std_logic_vector(DATA_W-1 downto 0);
        do1    : out std_logic_vector(DATA_W-1 downto 0)
    );
end BlockRam;

architecture behavioural of BlockRam is
    type ram_type is array (0 to 2**ADDR_W - 1) of std_logic_vector(DATA_W-1 downto 0);
    signal ram : ram_type := (others => (others => '0'));

    -- raw M4K outputs, before the fabric repair
    signal do0_ram, do1_ram : std_logic_vector(DATA_W-1 downto 0) := (others => '0');

    -- forwarding, mirroring src/BlockRam.vhdl
    signal fwd_to_0, fwd_to_1 : std_logic := '0';
    signal di_for0, di_for1   : std_logic_vector(DATA_W-1 downto 0) := (others => '0');
begin

    -- ---------------------------------------------------------------- raw M4K
    raw : process (clock)
        variable same : boolean;
    begin
        if rising_edge(clock) then
            same := (a0 = a1);

            -- writes
            if en0 = '1' and we0 = '1' then
                ram(conv_integer(unsigned(a0))) <= di0;
            end if;
            if en1 = '1' and we1 = '1' then
                ram(conv_integer(unsigned(a1))) <= di1;
            end if;

            -- port 0 read (only advances while enabled)
            if en0 = '1' then
                if we0 = '1' then
                    do0_ram <= di0;                       -- same-port: NEW data
                elsif same and en1 = '1' and we1 = '1' then
                    do0_ram <= (others => 'X');           -- mixed-port: UNDEFINED
                else
                    do0_ram <= ram(conv_integer(unsigned(a0)));
                end if;
            end if;

            -- port 1 read
            if en1 = '1' then
                if we1 = '1' then
                    do1_ram <= di1;                       -- same-port: NEW data
                elsif same and en0 = '1' and we0 = '1' then
                    do1_ram <= (others => 'X');           -- mixed-port: UNDEFINED
                else
                    do1_ram <= ram(conv_integer(unsigned(a1)));
                end if;
            end if;
        end if;
    end process;

    -- ------------------------------------------------- fabric repair (mirror)
    fwd : process (clock)
    begin
        if rising_edge(clock) then
            if en1 = '1' then
                di_for1 <= di0;
                if a0 = a1 and we0 = '1' and en0 = '1' then
                    fwd_to_1 <= '1';
                else
                    fwd_to_1 <= '0';
                end if;
            end if;
            if en0 = '1' then
                di_for0 <= di1;
                if a0 = a1 and we1 = '1' and we0 = '0' and en1 = '1' then
                    fwd_to_0 <= '1';
                else
                    fwd_to_0 <= '0';
                end if;
            end if;
        end if;
    end process;

    do0 <= di_for0 when fwd_to_0 = '1' else do0_ram;
    do1 <= di_for1 when fwd_to_1 = '1' else do1_ram;

end architecture behavioural;

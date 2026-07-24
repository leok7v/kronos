library ieee;
use ieee.std_logic_1164.all;
library altera_mf;
use altera_mf.altera_mf_components.all;

-- True dual-port block RAM (512 x 32) as an EXPLICIT Altera altsyncram, so it maps
-- to Cyclone M4K. DataCacheFM's cache_keys/cache_data write on BOTH ports (true
-- dual-port); Quartus CANNOT INFER that into an M4K from behavioural code -- it
-- builds ~16k registers per RAM instead (a 5x logic blow-up that won't fit the
-- EP1C12). The Xilinx port used RAMB16_S36_S36 and the Gowin port used a DPB
-- primitive for exactly this reason; on Altera the equivalent is altsyncram.
--
-- Dual-clock form (clock0/clock1 both = clock, clocken0/1 = en0/en1) so each port
-- keeps its independent clock-enable (the TLB instances gate reads with en0/en1).
-- Read latency is 1 cycle (outdata UNREGISTERED = address-registered only), matching
-- the original RAMB16 / behavioural model. Mixed-port read-during-write is
-- DONT_CARE (the only mode Cyclone M4K true dual-port supports); the cache accesses
-- its two ports at independent hash addresses, and the data cache is re-validated
-- on hardware regardless (its RDW behaviour was the Gowin failure -> bypass fallback).
--
-- GHDL can't elaborate altsyncram; the simulation build uses the behavioural
-- BlockRam in sim/ instead (see compile_onechip.sh).

-- Depth and width are generics so the 2-way cache can build narrower, shallower
-- arrays without a second copy of this file. Defaults are the original 512x32,
-- so every pre-existing instantiation is untouched.
entity BlockRam is
    generic (
        ADDR_W : integer := 9;          -- depth = 2**ADDR_W
        DATA_W : integer := 32
    );
    port (
        clock  : in std_logic;
        reset  : in std_logic := '0';   -- unused (kept for interface compatibility)
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

architecture altera of BlockRam is

    -- ---- mixed-port read-during-write FORWARDING ----
    -- Cyclone M4K true dual-port offers only DONT_CARE for mixed-port RDW: if
    -- one port writes address X while the other reads X, the reader's output is
    -- UNDEFINED BY THE SILICON. There is no altsyncram setting that fixes it.
    -- That is the defect that forced the cache bypass here and on Tang Nano:
    -- the data port and the instruction-fetch port collide on the cache hash
    -- constantly, and the garbage went into a fetch (and, via FLUSH, into SDRAM).
    --
    -- The standard fix is to detect the collision and supply the written data
    -- from fabric instead of trusting the RAM output. Costs a 9-bit comparator
    -- and a 32-bit mux per direction -- no extra M4K, which matters because
    -- 51 of 52 blocks are already used.
    --
    -- Read latency is one cycle (address-registered, output unregistered), so
    -- the decision belongs to the address presented in the PREVIOUS cycle:
    -- register the collision flag and the write data alongside it.
    signal do0_ram, do1_ram : std_logic_vector(DATA_W-1 downto 0);
    signal fwd_to_0, fwd_to_1 : std_logic := '0';
    signal di_for0, di_for1 : std_logic_vector(DATA_W-1 downto 0) := (others => '0');

begin

    -- CRITICAL: gate each direction by the READING port's clock enable.
    -- With clocken low, altsyncram FREEZES that port -- its address register
    -- holds and its output stays put. The forwarding state must freeze with
    -- it, or a stale flag fires against a held output and injects wrong data.
    -- The cache RAMs run with en0=en1='1', but tlb0/tlb1 gate on tlb_en and
    -- a1_stb, so an ungated version corrupts TLB reads -- which is a fetch
    -- derail, and is what the first attempt at this did on hardware.
    fwd : process (clock)
    begin
        if rising_edge(clock) then
            -- port 1's output advances only when port 1 is enabled
            if en1 = '1' then
                di_for1 <= di0;
                -- port 0 wins if both write: simultaneous writes to one address
                -- are a design error anyway, and fixed priority at least keeps
                -- the two outputs self-consistent
                if a0 = a1 and we0 = '1' and en0 = '1' then
                    fwd_to_1 <= '1';
                else
                    fwd_to_1 <= '0';
                end if;
            end if;
            -- port 0's output advances only when port 0 is enabled
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

    ram : altsyncram
        generic map (
            operation_mode                     => "BIDIR_DUAL_PORT",
            width_a       => DATA_W, widthad_a => ADDR_W, numwords_a => 2**ADDR_W,
            width_b       => DATA_W, widthad_b => ADDR_W, numwords_b => 2**ADDR_W,
            width_byteena_a => 1, width_byteena_b => 1,
            outdata_reg_a                      => "UNREGISTERED",
            outdata_reg_b                      => "UNREGISTERED",
            address_reg_b                      => "CLOCK1",
            indata_reg_b                       => "CLOCK1",
            wrcontrol_wraddress_reg_b          => "CLOCK1",
            read_during_write_mode_mixed_ports => "DONT_CARE",
            -- NOTE: same-port RDW is NEW_DATA_NO_NBE_READ and CANNOT be changed
            -- -- Quartus rejects OLD_DATA for the Cyclone family outright. The
            -- behavioural model in sim/ is read-first (OLD data), so hardware
            -- and simulation DISAGREE on same-port read-during-write, and no
            -- simulation of this cache can be trusted on that point.
            power_up_uninitialized             => "FALSE",
            intended_device_family             => "Cyclone",
            lpm_type                           => "altsyncram"
        )
        port map (
            clock0    => clock,   clock1    => clock,
            clocken0  => en0,     clocken1  => en1,
            address_a => a0,      address_b => a1,
            data_a    => di0,     data_b    => di1,
            wren_a    => we0,     wren_b    => we1,
            q_a       => do0_ram, q_b       => do1_ram
        );
end architecture altera;

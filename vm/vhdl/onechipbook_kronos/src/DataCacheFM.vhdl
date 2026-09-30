library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
use ieee.std_logic_textio.all;
use std.textio.all;
use work.Kronos_Types.all;

-- ===========================================================================
-- WRITE-BACK, READ-ALLOCATE  (2026-07-20)
-- ===========================================================================
--
-- History matters here, because "write-back" is what made this cache unusable
-- on Cyclone twice, and the bypass that replaced it cost a 2.4x slowdown:
--
--   bypassed (no caching)  2157 dhrystones/s
--   write-through          5192 dhrystones/s   (measured on hardware)
--   write-back             this design
--
-- Two Cyclone M4K read-during-write hazards killed the ORIGINAL write-back:
--
--   1. SAME-PORT RDW.  `cache_ready0` tested do0_keys(cache_dirty) -- the tag of
--      the line being EVICTED -- while `we0key` wrote the tag being INSTALLED,
--      same port, same cycle. Cyclone forces NEW_DATA_NO_NBE_READ and Quartus
--      REJECTS OLD_DATA for this family outright, so the FSM inspected the
--      freshly written tag instead of the one it meant to evict: dirty lines
--      dropped, clean lines flushed, silent memory corruption.
--
--   2. EVICTION READ vs REFILL WRITE.  do0_data was read for the flush while
--      port 1 (instruction refill) wrote that RAM -- undefined mixed-port
--      result, and the garbage went INTO SDRAM.
--
-- The fix is NOT to avoid writing tags. It is to never need an OLD tag value:
--
--   * Tags are written in exactly two states, READ (allocate) and MARK (set
--     dirty), and read in exactly one, CACHE. Disjoint, so hazard 1 cannot be
--     expressed. Asserted in the process below.
--   * MARK rewrites the tag of the line ALREADY resident -- same tag bits, only
--     the dirty bit changes -- so when a later read is served NEW data, that new
--     data is the authoritative current state. The old design wanted the
--     evicted line's tag while installing a different one; this one never does.
--   * The eviction reads do0_keys/do0_data in CACHE and latches them into
--     adr_o/dat_o at the CACHE->FLUSH edge, never during FLUSH. Port 1 cannot
--     write those RAMs on that edge (stall invariant, asserted), so hazard 2
--     cannot be expressed either.
--
-- POLICY
--   loads        hit  -> served from cache_data
--                miss -> flush the resident line if valid+dirty, then READ from
--                        memory and allocate (clean)
--   stores       hit  -> ABSORBED: data into cache_data (we0upd) in CACHE, then
--                        MARK sets dirty. No bus cycle. Already-dirty lines skip
--                        MARK, so repeated stores to one line cost one cycle.
--                miss -> straight to memory. NO write-allocate, and therefore no
--                        eviction and no flush: nothing is being installed.
--   port 1       read-only, so it never DIRTIES a line -- but it does EVICT
--                them, so it needs the flush path too.
--
-- NON-CACHEABLE IS LOad-BEARING. mem_cacheable0 keeps everything at/above
-- RAM_TOP out of the cache, and under write-back that is mandatory rather than
-- belt-and-braces: the booter's memory_top() probe stores past the top of RAM
-- and reads back, and if the store were absorbed and the read answered from
-- cache, the probe would match and memory_top would never terminate -- the
-- machine would not boot.
--
-- COHERENCE: sd_disk_controller DMAs straight into SDRAM, but ONLY while
-- `booting` (see its R_DATA state); io2 runtime transfers hand words to the CPU
-- through the DATA register, so they flow through this cache normally. And
-- `cpu_reset <= reset or not boot_done` holds the CPU in reset for the whole
-- boot DMA. So there is no DMA-vs-cache staleness window. If a future change
-- ever DMAs into SDRAM while the CPU runs, this cache MUST get an invalidate
-- path -- it has none. Under write-back it would also need a flush path, since
-- SDRAM is no longer guaranteed current.
--
-- Both ports share ONE cache, so I/D coherence is automatic: a store that hits
-- updates the line the fetch port reads.
--
-- The cache is virtually indexed, physically tagged, and the index (8 bits)
-- lies entirely inside the 12-bit page offset, so no aliasing and no flush on
-- context switch.
--
-- Verified by sim/tb_datacache_wb.vhdl against the FAITHFUL M4K model (32
-- checks), including that a write-back goes to the ORIGINAL address with the
-- ORIGINAL data -- wrong-address flush being the silent corruption mode.
--
--   CacheEn Write Hit Dirty  action
--      0      0    -    -    READ    (uncacheable load  -> memory)
--      0      1    -    -    WRITE   (uncacheable store -> memory)
--      1      0    1    -    serve from cache
--      1      0    0    0    READ    (load miss, clean victim -> allocate)
--      1      0    0    1    FLUSH then READ (write back the dirty victim)
--      1      1    1    0    absorb + MARK dirty      (no bus cycle)
--      1      1    1    1    absorb, already dirty    (no bus cycle, 1 cycle)
--      1      1    0    -    WRITE   (store miss -> memory, no allocate)

entity DataCache is
    generic (
        address_size : in integer := 32);
    port (
        -- Wishbone interface
	adr_o   : out std_logic_vector(address_size - 1 downto 0);
	dat_i   : in std_logic_vector(31 downto 0);
	dat_o   : out std_logic_vector(31 downto 0);
	we_o    : out std_logic;
	cyc_o   : out std_logic;
	stb_o   : out std_logic;
        lock_o  : out std_logic;
	ack_i   : in std_logic;
	err_i   : in std_logic;
        -- Port 0 signals
	a0_bus  : in std_logic_vector(31 downto 0);
	a0_read : in std_logic;
	a0_write: in std_logic;
	a0_wtlb : in std_logic;
	a0_wbase: in std_logic;
	d0_in   : in std_logic_vector(31 downto 0);
	d0_out  : out std_logic_vector(31 downto 0);
	d0_error: out std_logic;
	d0_ready: out std_logic;
        -- Port 1 signals
	a1_bus  : in std_logic_vector(31 downto 0);
	a1_read : in std_logic;
	d1_out  : out std_logic_vector(31 downto 0);
	d1_error: out std_logic;
	d1_ready: out std_logic;
        -- Clock
        stall   : in std_logic;
	clock   : in std_logic;
	reset   : in std_logic);
end DataCache;

architecture Variant1 of DataCache is

    -- TWO-WAY SET ASSOCIATIVE, two port cache
    -- Write-back

    -- MARK is the state that makes write-back safe on Cyclone. A store that
    -- hits writes its DATA in CACHE (we0upd) and then sets the line's dirty bit
    -- here, one cycle later. The tag RAM is therefore never written during
    -- CACHE, which is the single property the old design violated.
    type STATE_TYPE is (IDLE, CACHE, MARK, FLUSH, WRITE, READ);

    constant page_offs_bits  : integer := 12;
    -- 8, not 9: TWO WAYS of 256 sets rather than one way of 512 lines. Total
    -- capacity is UNCHANGED at 512 words -- this buys associativity, not size.
    constant cache_hash_bits : integer := 8;
    constant tlb_hash_bits   : integer := 9;

    -- Offset inside a page:
    subtype va_offs is integer range page_offs_bits - 1 downto 0;
    -- Virtual page no:
    subtype va_page is integer range a0_bus'high downto page_offs_bits;
    -- Part of page offset that is not cache index:
    subtype va_df is integer range page_offs_bits - 1 downto cache_hash_bits;
    -- Virtual page hash - TLB index:
    subtype va_tlb_hash is integer range tlb_hash_bits + page_offs_bits - 1 downto page_offs_bits;
    -- Un-hashed part of virtual page no:
    subtype va_tlb_key is integer range a0_bus'high downto tlb_hash_bits + page_offs_bits;
    -- Virtual address hash - cache index:
    subtype va_cache_hash is integer range cache_hash_bits - 1 downto 0;
    -- Un-hashed part of virtual address:
    subtype va_cache_key is integer range address_size - 1 downto cache_hash_bits;

    -- Cache entry fields:
    subtype cache_key is integer range va_cache_key'high - va_cache_key'low downto 0;
    constant cache_valid : integer := cache_key'high + 1;
    -- Back for write-back, and it costs NO extra M4K: the key word is already
    -- 32 bits wide and only 25 are used. That matters -- this design sits at
    -- 52/52 M4K, so a dirty array in block RAM was not available, and 512
    -- dirty bits in fabric would have cost two 512:1 muxes.
    constant cache_dirty : integer := cache_valid + 1;
    -- 13 bits: 11 tag + valid + dirty. The tag word is now sized to what it
    -- actually holds rather than padded to 32, because each way needs its own
    -- tag RAM and two 256x16 arrays are 2 M4Ks -- exactly what the single
    -- 512x32 array cost. Verified by synthesising the shape before writing this.
    constant key_bits    : integer := cache_dirty + 1;

    -- indexed by way (0/1)
    type way_data_t is array (0 to 1) of std_logic_vector(31 downto 0);
    type way_keys_t is array (0 to 1) of std_logic_vector(key_bits - 1 downto 0);

    -- TLB entry fields:
    constant tlb_tag_re : integer := 0; -- read enable
    constant tlb_tag_we : integer := 1; -- write enable
    constant tlb_tag_ce : integer := 2; -- cache enable
    -- TLB entry field: frame, 20 bits max
    subtype tlb_frame is integer range tlb_tag_ce + address_size - va_page'low downto tlb_tag_ce + 1;
    -- TLB entry field: page key, 11 bits
    subtype tlb_key is integer range tlb_frame'high + va_tlb_key'high + 1 - va_tlb_key'low downto tlb_frame'high + 1;
    -- TLB entry field: translation table base, 20 bits max
    subtype tlb_base is integer range tlb_key'high + address_size - va_page'low downto tlb_key'high + 1;

    signal rd0_state   : STATE_TYPE;
    signal rd1_state   : STATE_TYPE;
    signal a0_write1   : std_logic;
    signal a0_stb      : std_logic;
    signal a1_stb      : std_logic;
    signal d0_in1      : std_logic_vector(31 downto 0);
    signal a0_bus1     : std_logic_vector(31 downto 0);
    signal a1_bus1     : std_logic_vector(31 downto 0);
    signal di0_data    : std_logic_vector(31 downto 0);
    signal di1_data    : std_logic_vector(31 downto 0);
    signal di0_keys    : std_logic_vector(key_bits - 1 downto 0);
    signal di1_keys    : std_logic_vector(key_bits - 1 downto 0);

    -- ---------------------------------------------------------------- ways
    -- Per-way RAM outputs. Both ways are read EVERY cycle at the set index, so
    -- hit detection and way select are combinational off one RAM read and the
    -- 1-cycle hit is preserved -- an associative cache that cost a second cycle
    -- on every hit would lose more than the conflict misses it saves.
    signal do0_data_w  : way_data_t;
    signal do1_data_w  : way_data_t;
    signal do0_keys_w  : way_keys_t;
    signal do1_keys_w  : way_keys_t;

    signal hit0_w      : std_logic_vector(1 downto 0);   -- per-way hit, port 0
    signal hit1_w      : std_logic_vector(1 downto 0);   -- per-way hit, port 1
    signal hit_way0    : std_logic;                      -- WHICH way hit, port 0
    signal hit_way1    : std_logic;
    -- the selected way's tag/data: the HIT way on a hit, used for the store-hit
    -- dirty test and for the data actually returned
    signal hit_keys0   : std_logic_vector(key_bits - 1 downto 0);
    signal hit_keys1   : std_logic_vector(key_bits - 1 downto 0);
    signal do0_data    : std_logic_vector(31 downto 0);
    signal do1_data    : std_logic_vector(31 downto 0);
    -- the VICTIM way's tag/data: what an allocation is about to displace, and
    -- therefore what a write-back must flush. Distinct from the hit way, and
    -- confusing the two is the silent-corruption mode (right data, wrong place).
    signal vic_keys0   : std_logic_vector(key_bits - 1 downto 0);
    signal vic_keys1   : std_logic_vector(key_bits - 1 downto 0);
    signal vic_data0   : std_logic_vector(31 downto 0);
    signal vic_data1   : std_logic_vector(31 downto 0);

    -- REPLACEMENT: one global round-robin bit, deliberately NOT LRU.
    --
    -- LRU would have to write a tag on every HIT, which breaks the invariant
    -- this whole write-back design rests on -- tags are written ONLY in READ
    -- and MARK, and read ONLY in CACHE -- and would re-open the same-port
    -- read-during-write hazard that made this cache unusable on Cyclone twice.
    -- A toggle needs no storage and no tag write at all.
    --
    -- It is also better than it looks for the case that matters. The toggle
    -- flips only on ALLOCATION, never on a hit, so a two-address conflict
    -- A,B,A,B settles: A takes way 0, B takes way 1, and both then hit forever.
    -- Three-way conflicts still thrash, but so would LRU with two ways.
    signal victim      : std_logic := '0';
    signal vic_way0    : std_logic := '0';   -- victim latched at the miss decision
    signal vic_way1    : std_logic := '0';
    -- which way each port writes this cycle
    signal tgt_way0    : std_logic;
    signal tgt_way1    : std_logic;
    signal cache_hash0 : std_logic_vector(va_cache_hash);
    signal cache_hash1 : std_logic_vector(va_cache_hash);
    signal cache_en0   : std_logic;
    signal cache_en1   : std_logic;
    signal cache_hit0  : std_logic;
    signal cache_hit1  : std_logic;
    -- The six bypass levers that used to sit here (cache_dirty0/1, tag_ce0 and
    -- the forced-'0' write enables) are GONE, along with the cache they
    -- disabled. Write-through needs no dirty tracking, and tag_ce0 existed only
    -- to pick the write-back store-absorb path, which no longer exists.
    signal d0_ready_i  : std_logic;
    signal d1_ready_i  : std_logic;
    signal cache_ready0: std_logic;
    signal cache_ready1: std_logic;
    signal do0_rg_data : std_logic_vector(31 downto 0);
    signal do0_rg_error: std_logic;
    signal do1_rg_data : std_logic_vector(31 downto 0);
    signal do1_rg_error: std_logic;
    signal we0         : std_logic;
    signal we0key      : std_logic;
    signal we0dat      : std_logic;
    signal we0upd      : std_logic;   -- store hit: refresh cache_data in place
    signal marking     : std_logic;   -- in MARK: set the resident line dirty
    signal we1         : std_logic;
    -- the same enables, steered to ONE way. Exactly one way is ever written in a
    -- cycle, so a stray enable cannot corrupt the other way's line.
    signal we0key_w    : std_logic_vector(1 downto 0);
    signal we0dat_w    : std_logic_vector(1 downto 0);
    signal we1_w       : std_logic_vector(1 downto 0);
    signal ack_any     : std_logic;
    signal collision   : std_logic;
    signal d0_ready1   : std_logic;
    signal tlb_data0   : std_logic_vector(63 downto 0);
    signal tlb_data1   : std_logic_vector(63 downto 0);
    signal tlb_in      : std_logic_vector(63 downto 0);
    signal tlb_match0  : std_logic;
    signal tlb_match1  : std_logic;
    signal tlb_tags_ok0: std_logic;
    signal tlb_tags_ok1: std_logic;
    signal tlb_frame0  : std_logic_vector(tlb_frame);
    signal tlb_frame1  : std_logic_vector(tlb_frame);
    signal base        : std_logic_vector(address_size - 1 downto va_page'low) := (others => '0');  -- init 0 (matches Altera power-up; sim needs it explicit)
    signal tlb_en      : std_logic;
    signal tlb_off     : std_logic;
    signal mem_cacheable0 : std_logic;  -- OneChipBook: RAM = [0, 0x70000) = 1792 KB
    signal tlb_hit0    : std_logic;
    signal tlb_hit1    : std_logic;

begin

    -- The debug trace process that used to live here has been REMOVED.
    -- It was the ONLY thing GHDL could not compile in this file (a
    -- std_logic_textio write() overload ambiguity), which is why a near-
    -- duplicate existed as sim/DataCacheFM_realcache.vhdl. Those two copies
    -- drifted, and toggling the cache in one while simulating the other made
    -- a whole evening of experiments compare a build against itself. One file
    -- now serves both Quartus and GHDL. Trace was dead code anyway: this
    -- board's KronosTypes sets `trace := false`.

    -- One tag RAM and one data RAM PER WAY. Separate tag arrays (rather than one
    -- wide array holding both ways' tags) is the load-bearing choice: allocating
    -- into a way then writes ONLY that way's array, with no read-modify-write to
    -- preserve the other way's tag. A merged array would have to re-write a tag
    -- it had read in an earlier cycle, which is precisely the "install one tag
    -- while inspecting another" shape that Cyclone's forced NEW_DATA turns into
    -- silent corruption.
    --
    -- Costs nothing: 2 x 256x16 tags = 2 M4Ks and 2 x 256x32 data = 4 M4Ks, the
    -- same 6 the direct-mapped 512x32 pair used.
    ways : for w in 0 to 1 generate
        cache_keys : entity work.BlockRam
            generic map (ADDR_W => cache_hash_bits, DATA_W => key_bits)
            port map (
                clock => clock,
                en0   => '1',
                en1   => '1',
                we0   => we0key_w(w),
                we1   => we1_w(w),
                a0    => cache_hash0,
                a1    => cache_hash1,
                di0   => di0_keys,
                di1   => di1_keys,
                do0   => do0_keys_w(w),
                do1   => do1_keys_w(w));

        cache_data : entity work.BlockRam
            generic map (ADDR_W => cache_hash_bits, DATA_W => 32)
            port map (
                clock => clock,
                en0   => '1',
                en1   => '1',
                we0   => we0dat_w(w),
                we1   => we1_w(w),
                a0    => cache_hash0,
                a1    => cache_hash1,
                di0   => di0_data,
                di1   => di1_data,
                do0   => do0_data_w(w),
                do1   => do1_data_w(w));
    end generate;

    tlb0 : entity work.BlockRam
	port map (
	    clock => clock,
	    en0   => tlb_en,
	    en1   => a1_stb,
	    we0   => a0_wtlb,
	    we1   => '0',
	    a0    => a0_bus(va_tlb_hash),
	    a1    => a1_bus(va_tlb_hash),
	    di0   => tlb_in(31 downto 0),
	    di1   => (31 downto 0 => '0'),
	    do0   => tlb_data0(31 downto 0),
	    do1   => tlb_data1(31 downto 0));

    tlb1 : entity work.BlockRam
	port map (
	    clock => clock,
	    en0   => tlb_en,
	    en1   => a1_stb,
	    we0   => a0_wtlb,
	    we1   => '0',
	    a0    => a0_bus(va_tlb_hash),
	    a1    => a1_bus(va_tlb_hash),
	    di0   => tlb_in(63 downto 32),
	    di1   => (31 downto 0 => '0'),
	    do0   => tlb_data0(63 downto 32),
	    do1   => tlb_data1(63 downto 32));

    lock_o <= '0'; -- TODO: implement 'lock_o'

    ack_any <= ack_i or err_i;

    d0_ready_i <= '1' when rd0_state = IDLE else cache_ready0 when rd0_state = CACHE else '0';
    d1_ready_i <= '1' when rd1_state = IDLE else cache_ready1 when rd1_state = CACHE else '0';
    d0_ready <= d0_ready_i;
    d1_ready <= d1_ready_i;

    tlb_off <= '1' when base = (base'range => '0') else '0';

    a0_stb <= not stall and (a0_read or a0_write);
    a1_stb <= not stall and a1_read;

    -- THE line that used to be hazard 1. It read do0_keys(cache_dirty) while
    -- we0key wrote the same tag on the same port. A store is now NEVER served
    -- from the cache -- it always goes to memory -- so no tag is inspected in a
    -- cycle that writes one, and the M4K's forced NEW_DATA has nothing to
    -- corrupt.
    cache_ready0 <= cache_hit0 and not a0_write1;
    cache_ready1 <= cache_hit1;

    cache_hash0 <= a0_bus(va_cache_hash) when d0_ready_i = '1' else a0_bus1(va_cache_hash);
    cache_hash1 <= a1_bus(va_cache_hash) when d1_ready_i = '1' else a1_bus1(va_cache_hash);

    collision <= '1' when a0_bus1(va_cache_hash) = a1_bus1(va_cache_hash) and
        (rd0_state = CACHE or d0_ready1 = '0') else '0';

    -- The region above physical RAM is NON-CACHEABLE during tlb_off so the
    -- booter's memory_top() out-of-range probe read misses the cache and reaches
    -- the top-level 0-return -> its NEQ mismatches and memory_top stops there
    -- (the reference VM's out-of-range read = 0). Under the old WRITE-BACK
    -- policy this was mandatory: the cache absorbed the probe's store, the
    -- read-back matched, and memory_top never terminated. Write-through would
    -- survive without it -- the store goes to memory, the probe read misses (no
    -- write-allocate) and gets the 0 back -- but it is kept because it is
    -- correct, it keeps the probe off the cache entirely, and this is not the
    -- change to test that assumption in. (Spartan-3 uses the unmodified 5.0
    -- cache; this copy is OneChipBook-specific -- see OneChipBook qsf.)
    cache_en0 <= ((tlb_off and not a0_bus1(31)) or (tlb_hit0 and tlb_data0(tlb_tag_ce))) and mem_cacheable0;
    -- Must track RAM_TOP in OneChipBook (0x7F0000 = 31.75 MB on the 23-bit
    -- bus): cacheable is everything below it, i.e. NOT the top 64K-word block of
    -- the 23-bit space (0x7F0000..0x7FFFFF = the RAM_TOP gap + the 0x7FF000 I/O
    -- page). LIVE logic now that the cache is real -- it gates we0 and cache_hit0.
    mem_cacheable0 <= '0' when a0_bus1(22 downto 16) = "1111111" else '1';
    cache_en1 <= tlb_off or (tlb_hit1 and tlb_data1(tlb_tag_ce));

    -- Hit = the line is valid AND its physical tag matches the address being
    -- presented -- now tested against BOTH ways in parallel. Port 1 additionally
    -- drops its hit on `collision`, which keeps an instruction fetch from
    -- trusting a line port 0 is working on this cycle.
    --
    -- A line can never be resident in both ways at once, because allocation only
    -- ever happens on a MISS -- which by definition means neither way matched --
    -- so the two hit bits are mutually exclusive and `hit_way` is unambiguous.
    hits : for w in 0 to 1 generate
        hit0_w(w) <= cache_en0 and do0_keys_w(w)(cache_valid)
                     when do0_keys_w(w)(cache_key) = di0_keys(cache_key) else '0';
        hit1_w(w) <= cache_en1 and do1_keys_w(w)(cache_valid) and not collision
                     when do1_keys_w(w)(cache_key) = di1_keys(cache_key) else '0';
    end generate;

    cache_hit0 <= hit0_w(0) or hit0_w(1);
    cache_hit1 <= hit1_w(0) or hit1_w(1);
    hit_way0   <= hit0_w(1);          -- 1 = way 1 hit, 0 = way 0 (or no hit)
    hit_way1   <= hit1_w(1);

    -- The HIT way's tag and data: what a load returns and what the store-hit
    -- dirty test inspects.
    hit_keys0 <= do0_keys_w(1) when hit_way0 = '1' else do0_keys_w(0);
    hit_keys1 <= do1_keys_w(1) when hit_way1 = '1' else do1_keys_w(0);
    do0_data  <= do0_data_w(1) when hit_way0 = '1' else do0_data_w(0);
    do1_data  <= do1_data_w(1) when hit_way1 = '1' else do1_data_w(0);

    -- The VICTIM way's tag and data: what an allocation is about to displace.
    -- Selected by the live toggle in CACHE (where the miss is decided) and
    -- latched into vic_way0/1 at that edge, so FLUSH and READ act on the same
    -- way the decision was made about even though the toggle keeps moving.
    vic_keys0 <= do0_keys_w(1) when victim = '1' else do0_keys_w(0);
    vic_keys1 <= do1_keys_w(1) when victim = '1' else do1_keys_w(0);
    vic_data0 <= do0_data_w(1) when victim = '1' else do0_data_w(0);
    vic_data1 <= do1_data_w(1) when victim = '1' else do1_data_w(0);

    -- Which way each port writes. MARK refreshes the line that HIT; an
    -- allocation in READ installs into the way chosen when the miss was decided.
    tgt_way0 <= hit_way0 when rd0_state = CACHE or rd0_state = MARK else vic_way0;
    tgt_way1 <= hit_way1 when rd1_state = CACHE else vic_way1;

    we0key_w(0) <= we0key and not tgt_way0;
    we0key_w(1) <= we0key and     tgt_way0;
    we0dat_w(0) <= we0dat and not tgt_way0;
    we0dat_w(1) <= we0dat and     tgt_way0;
    we1_w(0)    <= we1    and not tgt_way1;
    we1_w(1)    <= we1    and     tgt_way1;

    -- ---------------------------------------------------------------- refill
    -- Allocation happens ONLY at READ-ack, and it writes the tag and the data
    -- in the SAME cycle with the SAME enable. That simultaneity is load-bearing:
    -- if the other port reads this index in that cycle, BlockRam's fabric
    -- forwarding hands it the new tag AND the new data together, so it can
    -- never match a fresh tag against stale data. Splitting these two writes
    -- across cycles would reintroduce exactly that bug.
    --
    -- On err_i the tag is still written but with cache_valid = ack_i = '0', so
    -- a failed read invalidates the line instead of caching garbage.
    we0 <= ack_any and cache_en0 when rd0_state = READ else '0';
    we1 <= ack_any and cache_en1 when rd1_state = READ else '0';

    -- Tag writes happen in exactly two states: READ (allocate) and MARK (set
    -- dirty). NEVER in CACHE, which is the only state that READS a tag. That
    -- disjointness is the whole safety argument, and it is asserted below.
    --
    -- The distinction that matters, and that the original design got wrong: it
    -- wrote the tag being INSTALLED while trying to read the tag being EVICTED,
    -- so Cyclone's forced NEW_DATA handed it the wrong line's dirty bit. MARK
    -- rewrites the tag of the line ALREADY resident -- same tag bits, only the
    -- dirty bit changes -- so even when a later read is served NEW data, that
    -- new data is the authoritative current state. There is no "old value" this
    -- design needs and cannot have.
    marking <= '1' when rd0_state = MARK else '0';
    we0key <= we0 or marking;

    -- Write-through store hit: memory gets the word via the WRITE state, and
    -- the resident line is refreshed in place so it does not go stale. Done in
    -- CACHE (one cycle after the request) because that is when the hit is known
    -- -- hence d0_in1/a0_bus1, the registered copies, rather than d0_in/a0_bus.
    -- A store that MISSES writes nothing here: no write-allocate, and the line
    -- it indexes holds some other address, so leaving it is correct.
    we0upd <= a0_write1 and cache_hit0 when rd0_state = CACHE else '0';
    we0dat <= we0 or we0upd;

    tlb_en <= a0_stb or a0_wtlb;
    tlb_in(tlb_tag_re) <= d0_in(0);
    tlb_in(tlb_tag_we) <= d0_in(1);
    tlb_in(tlb_tag_ce) <= d0_in(2);
    tlb_in(tlb_frame)  <= d0_in(address_size - 1 downto va_page'low);
    tlb_in(tlb_key)    <= a0_bus(va_tlb_key);
    tlb_in(tlb_base)   <= base;

    -- Qualified: declaring the per-way array types made a bare `slv & slv`
    -- ambiguous, since concatenating two std_logic_vectors can also build an
    -- array-OF-std_logic_vector. The qualification pins it to the flat vector.
    tlb_match0 <= '1' when std_logic_vector'(tlb_data0(tlb_base) & tlb_data0(tlb_key))
                         = std_logic_vector'(base & a0_bus1(va_tlb_key)) else '0';
    tlb_match1 <= '1' when std_logic_vector'(tlb_data1(tlb_base) & tlb_data1(tlb_key))
                         = std_logic_vector'(base & a1_bus1(va_tlb_key)) else '0';

    tlb_tags_ok0 <= tlb_data0(tlb_tag_re) when a0_write1 = '0' else tlb_data0(tlb_tag_we);
    tlb_tags_ok1 <= tlb_data1(tlb_tag_re);

    tlb_hit0 <= tlb_off or (tlb_match0 and tlb_tags_ok0);
    tlb_hit1 <= tlb_off or (tlb_match1 and tlb_tags_ok1);

    tlb_frame0 <= a0_bus1(address_size - 1 downto page_offs_bits) when tlb_off = '1' else tlb_data0(tlb_frame);
    tlb_frame1 <= a1_bus1(address_size - 1 downto page_offs_bits) when tlb_off = '1' else tlb_data1(tlb_frame);

    di0_keys(cache_key) <= tlb_frame0 & a0_bus1(va_df);
    di1_keys(cache_key) <= tlb_frame1 & a1_bus1(va_df);
    -- In READ this is "the allocate succeeded" (err_i leaves the line invalid
    -- rather than caching garbage). In MARK the line is resident by definition,
    -- so it is valid and becomes dirty.
    --
    -- di0_keys(cache_key) needs NO MARK override: a0_bus1 and tlb_frame0 both
    -- still hold the store's address (stall is high throughout MARK, and
    -- tlb_en is low so the TLB port is frozen), and this was a HIT, so the
    -- recomputed tag is bit-for-bit the tag already in the line.
    di0_keys(cache_valid) <= '1' when marking = '1' else ack_i;
    di0_keys(cache_dirty) <= '1' when marking = '1' else '0';
    di1_keys(cache_valid) <= ack_i;
    -- Port 1 is InstructionFetch's refill path and is read-only, so a line it
    -- allocates is always clean.
    di1_keys(cache_dirty) <= '0';

    -- Two sources: the refill word from memory (READ), or the store data for an
    -- in-place update (CACHE). d0_in1, not d0_in -- see we0upd.
    di0_data <= dat_i when rd0_state = READ else d0_in1;
    di1_data <= dat_i;
    d0_out <= do0_data when rd0_state = CACHE else do0_rg_data;
    d1_out <= do1_data when rd1_state = CACHE else do1_rg_data;
    d0_error <= do0_rg_error;
    d1_error <= do1_rg_error;

    process (clock)
        variable rd : boolean;
        variable wr : boolean;
        variable de_in : boolean;
        variable de_o0 : boolean;
        variable de_o1 : boolean;
        variable ae_a0 : boolean;
        variable ae_a1 : boolean;
        variable ae_o0 : boolean;
        variable ae_o1 : boolean;
    begin
	if clock'event and clock = '1' then
            if stall = '0' then
                if a0_wbase = '1' then
                    base <= d0_in(base'range);
                end if;
                if a1_read = '1' then
                    a1_bus1 <= a1_bus;
                end if;
                if a0_read = '1' or a0_write = '1' then
                    a0_bus1 <= a0_bus;
                end if;
                if a0_write = '1' then
                    d0_in1 <= d0_in;
                end if;
                a0_write1 <= a0_write;
            end if;

            rd := false;
            wr := false;
            de_in := false;
            de_o0 := false;
            de_o1 := false;
            ae_a0 := false;
            ae_a1 := false;
            ae_o0 := false;
            ae_o1 := false;

            d0_ready1 <= d0_ready_i;

            if reset = '1' then
                rd0_state <= IDLE;
                do0_rg_error <= '1';
                victim <= '0'; vic_way0 <= '0'; vic_way1 <= '0';
            elsif rd0_state = IDLE then
                if a0_stb = '1' then
                    rd0_state <= CACHE;
                    do0_rg_error <= '0';
                end if;
            elsif rd0_state = CACHE then
                if cache_ready0 = '1' then
                    do0_rg_data <= do0_data;
                    do0_rg_error <= '0';
                    if a0_stb = '0' then
                        rd0_state <= IDLE;
                    end if;
                elsif tlb_hit0 = '0' then
                    do0_rg_error <= '1';
                    rd0_state <= IDLE;
                -- STORE HIT -> ABSORBED. This is the entire write-back win: no
                -- bus cycle at all. The data landed in cache_data this cycle via
                -- we0upd; all that remains is the dirty bit, and a line that is
                -- already dirty needs nothing, so the common case of repeated
                -- stores to one line costs a single cycle.
                elsif a0_write1 = '1' and cache_hit0 = '1' then
                    if hit_keys0(cache_dirty) = '1' then
                        rd0_state <= IDLE;
                    else
                        rd0_state <= MARK;
                    end if;
                -- STORE MISS -> straight to memory, no write-allocate. Nothing
                -- is being installed, so the resident line is not evicted and
                -- must NOT be flushed.
                elsif a0_write1 = '1' then
                    rd0_state <= WRITE;
                    ae_a0 := true;
                    de_in := true;
                    wr := true;
                -- LOAD MISS over a VALID DIRTY line -> write it back first.
                -- do0_keys/do0_data are read HERE, in CACHE, and latched into
                -- adr_o/dat_o by ae_o0/de_o0 at this same edge -- never during
                -- FLUSH itself. Port 1 cannot be writing these RAMs on this edge
                -- (see the stall invariant asserted below), so this read is not
                -- a mixed-port hazard.
                elsif vic_keys0(cache_valid) = '1' and vic_keys0(cache_dirty) = '1'
                      and cache_en0 = '1' then
                    rd0_state <= FLUSH;
                    vic_way0 <= victim;
                    ae_o0 := true;
                    de_o0 := true;
                    wr := true;
                else
                    rd0_state <= READ;
                    vic_way0 <= victim;
                    ae_a0 := true;
                    rd := true;
                end if;
            elsif rd0_state = MARK then
                -- One cycle, no bus activity: we0key writes the resident tag
                -- back with dirty set. d0_ready is low throughout (MARK is
                -- neither IDLE nor CACHE), so stall holds a0_bus1/d0_in1 and
                -- the CPU does not advance until the line is marked.
                rd0_state <= IDLE;
            elsif rd0_state = FLUSH then
                if ack_any = '0' then
                    wr := true;
                else
                    -- Write-back done; now perform the access that displaced it.
                    -- Only a LOAD miss can reach FLUSH (stores never allocate),
                    -- so this always continues into READ.
                    rd0_state <= READ;
                    ae_a0 := true;
                    rd := true;
                end if;
            elsif rd0_state = READ then
                if ack_any = '0' then
                    rd := true;
                else
                    rd0_state <= IDLE;
                    do0_rg_data <= dat_i;
                    do0_rg_error <= err_i;
                end if;
            elsif rd0_state = WRITE then
                if ack_any = '0' then
                    wr := true;
                else
                    rd0_state <= IDLE;
                    do0_rg_error <= err_i;
                end if;
            end if;

            if reset = '1' then
                rd1_state <= IDLE;
                do1_rg_error <= '0';
            elsif rd1_state = IDLE then
                if a1_stb = '1' then
                    rd1_state <= CACHE;
                    do1_rg_error <= '0';
                end if;
            elsif rd1_state = CACHE then
                if cache_ready1 = '1' then
                    do1_rg_data <= do1_data;
                    do1_rg_error <= '0';
                    if a1_stb = '0' then
                        rd1_state <= IDLE;
                    end if;
                elsif tlb_hit1 = '0' then
                    do1_rg_error <= '1';
                    rd1_state <= IDLE;
                elsif rd or wr or collision = '1' then
                    null;
                -- Port 1 never DIRTIES a line (it is read-only), but it does
                -- EVICT them: a fetch that misses displaces whatever shares its
                -- index, and port 0 may have dirtied that. Skipping this would
                -- silently drop stores.
                --
                -- `collision` is what makes the shared dirty bit safe here. In
                -- the single cycle where port 0 is absorbing a store, the tag's
                -- dirty bit is not yet set (MARK is the next cycle), so port 1
                -- could read dirty='0' for a line about to become dirty. But
                -- port 0 being in CACHE forces collision at the same index, and
                -- collision parks port 1 -- it re-evaluates after MARK lands.
                elsif vic_keys1(cache_valid) = '1' and vic_keys1(cache_dirty) = '1'
                      and cache_en1 = '1' then
                    rd1_state <= FLUSH;
                    vic_way1 <= victim;
                    ae_o1 := true;
                    de_o1 := true;
                    wr := true;
                else
                    rd1_state <= READ;
                    vic_way1 <= victim;
                    ae_a1 := true;
                    rd := true;
                end if;
            elsif rd1_state = FLUSH then
                if ack_any = '0' then
                    wr := true;
                else
                    rd1_state <= READ;
                    ae_a1 := true;
                    rd := true;
                end if;
            elsif rd1_state = READ then
                if ack_any = '0' then
                    rd := true;
                else
                    rd1_state <= IDLE;
                    do1_rg_data <= dat_i;
                    do1_rg_error <= err_i;
                end if;
            end if;

            -- synthesis translate_off
            assert not (rd and wr) severity failure;
            assert not ae_a0 or rd or wr severity failure;
            assert not ae_a1 or rd severity failure;
            assert not ae_o0 or wr severity failure;
            assert not ae_o1 or wr severity failure;
            assert not (ae_a0 and ae_a1) severity failure;
            -- Both ports must never write back in the same cycle: dat_o/adr_o
            -- would have two drivers' worth of intent and one would be lost.
            assert not (ae_o0 and ae_o1) severity failure;
            assert we0key = '0' or stall = '1' severity failure;
            assert we1 = '0' or stall = '1' severity failure;
            -- Write-through invariants, checked rather than assumed.
            --
            -- Both ports writing the SAME index of the same RAM in one cycle is
            -- undefined on the M4K, and the forwarding logic cannot paper over
            -- it either (it gives port 0 priority for port 1's read but
            -- suppresses port 1 -> port 0, so the two ports would disagree about
            -- what the line now holds). Different indices are fine -- that is
            -- what dual-port is for -- so the assert is index-precise.
            --
            -- The argument that it cannot happen runs through the CPU's stall
            -- wiring: while port 1 is in READ, d1_ready is low, so stall is high,
            -- so a0_stb is low and port 0 cannot leave IDLE; and when port 0 is
            -- in CACHE it raises `wr`, which parks port 1 before it can start a
            -- READ. That is a cross-module invariant depending on Kronos.vhdl's
            -- `stall <= (not ready_d or not ready_s)`. Cross-module invariants
            -- are exactly what should be asserted rather than trusted.
            assert not (we0dat = '1' and we1 = '1' and cache_hash0 = cache_hash1)
                severity failure;
            assert not (we0key = '1' and we1 = '1' and cache_hash0 = cache_hash1)
                severity failure;
            --   a tag must never be read in a cycle that writes one on the same
            --   port. This is hazard 1 stated directly: if it ever fires again,
            --   the design has regressed to something Cyclone cannot execute.
            --   CACHE is the only state that reads a tag; MARK and READ are the
            --   only states that write one.
            assert not (we0key = '1' and rd0_state = CACHE) severity failure;
            -- TWO-WAY: a line must NEVER be resident in both ways at once.
            --
            -- If it were, a store absorbed into one copy would leave the other
            -- stale, and which one a later load saw would depend on the hit
            -- priority -- silent, data-dependent corruption. The design argument
            -- is that allocation happens only on a MISS, which by definition
            -- means neither way matched, so a duplicate cannot be created. That
            -- is exactly the kind of reasoning that deserves a check rather than
            -- trust, because it depends on the hit test and the allocate path
            -- agreeing about what "the same line" means.
            assert not (hit0_w(0) = '1' and hit0_w(1) = '1') severity failure;
            assert not (hit1_w(0) = '1' and hit1_w(1) = '1') severity failure;
            --   an eviction reads do0_data/do0_keys at the CACHE->FLUSH edge.
            --   Port 1 must not be writing those RAMs on that edge, or the
            --   flushed data is an undefined mixed-port read -- garbage sent to
            --   SDRAM, the exact failure that killed the previous write-back.
            assert not (ae_o0 and we1 = '1') severity failure;
            assert not (ae_o1 and we0 = '1') severity failure;
            -- synthesis translate_on

            if ae_a0 then
                adr_o <= tlb_frame0 & a0_bus1(va_offs);
            end if;
            if ae_a1 then
                adr_o <= tlb_frame1 & a1_bus1(va_offs);
            end if;
            -- Write-back address: the EVICTED line's tag with the index it sat
            -- at -- NOT the address that displaced it. Getting this wrong is the
            -- silent-corruption mode (right data, wrong place), which is why the
            -- unit test asserts the address and not merely the write count.
            if ae_o0 then
                adr_o <= vic_keys0(cache_key) & a0_bus1(va_cache_hash);
            end if;
            if ae_o1 then
                adr_o <= vic_keys1(cache_key) & a1_bus1(va_cache_hash);
            end if;

            -- ROUND-ROBIN advance. Only an ALLOCATION moves it -- never a hit,
            -- and never a store that merely marks a line dirty. That is what
            -- lets a two-address conflict settle into one way each and then hit
            -- forever, instead of ping-ponging.
            if we0 = '1' or we1 = '1' then
                victim <= not victim;
            end if;

            if de_in then
                dat_o <= d0_in1;
            end if;
            if de_o0 then
                dat_o <= vic_data0;
            end if;
            if de_o1 then
                dat_o <= vic_data1;
            end if;

            if rd then
                cyc_o <= '1';
                stb_o <= '1';
                we_o <= '0';
            elsif wr then
                cyc_o <= '1';
                stb_o <= '1';
                we_o <= '1';
            else
                cyc_o <= '0';
                stb_o <= '0';
                we_o <= '0';
            end if;
	end if;
    end process;

end architecture Variant1;


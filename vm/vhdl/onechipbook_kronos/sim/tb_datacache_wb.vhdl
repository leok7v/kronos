library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

-- Unit test for the WRITE-BACK DataCache against the FAITHFUL Cyclone M4K
-- BlockRam model (sim/BlockRam_sim.vhdl: same-port RDW = NEW_DATA, mixed-port
-- RDW = 'X', clocken freeze).
--
-- Successor to tb_datacache_wt.vhdl. Write-back is the policy that made this
-- cache unusable on Cyclone twice, and its characteristic failure is SILENT --
-- garbage flushed to a garbage address, corrupting memory long before anything
-- visibly breaks. So the checks here are deliberately about the write-back
-- machinery itself, not just "does a load return the right value":
--
--   * a store that hits is ABSORBED   -> no bus write, and MEMORY STILL HOLDS
--                                        THE OLD VALUE. Checking memory is the
--                                        point: "no bus write" alone would also
--                                        pass if the store vanished entirely.
--   * evicting a DIRTY line writes it back EXACTLY ONCE, to the ORIGINAL
--     address, with the ORIGINAL data (wrong-address flush is the silent
--     corruption mode -- assert the address, not just the count)
--   * evicting a CLEAN line writes back NOTHING (spurious flushes corrupt too)
--   * a non-cacheable store is NEVER absorbed -- the booter's memory_top()
--     probe stores past the top of RAM and reads back, and if write-back
--     answered that read from cache, memory_top would never terminate. This is
--     mandatory under write-back, where it was only belt-and-braces under
--     write-through.
--
-- Hits/misses are measured WITHOUT probing inside the cache (VHDL-93 has no
-- external names): a hit issues no bus cycle, so bus traffic counts misses.
--
-- NEGATIVE CONTROL: run this suite against the write-through design and the
-- absorb/write-back checks must FAIL while the correctness checks pass. A suite
-- that cannot fail proves nothing -- that is how the write-through suite was
-- validated against the old bypassed cache.

entity tb_datacache_wb is
end tb_datacache_wb;

architecture test of tb_datacache_wb is

    constant PERIOD : time := 20 ns;

    signal clk   : std_logic := '0';
    signal reset : std_logic := '1';
    signal stall : std_logic;
    signal done  : boolean := false;

    -- Wishbone
    signal adr_o : std_logic_vector(31 downto 0);
    signal dat_i : std_logic_vector(31 downto 0) := (others => '0');
    signal dat_o : std_logic_vector(31 downto 0);
    signal we_o, cyc_o, stb_o, lock_o : std_logic;
    signal ack_i : std_logic := '0';
    signal err_i : std_logic := '0';

    -- port 0 (data)
    signal a0_bus  : std_logic_vector(31 downto 0) := (others => '0');
    signal a0_read, a0_write, a0_wtlb, a0_wbase : std_logic := '0';
    signal d0_in   : std_logic_vector(31 downto 0) := (others => '0');
    signal d0_out  : std_logic_vector(31 downto 0);
    signal d0_error, d0_ready : std_logic;

    -- port 1 (instruction refill; read-only)
    signal a1_bus  : std_logic_vector(31 downto 0) := (others => '0');
    signal a1_read : std_logic := '0';
    signal d1_out  : std_logic_vector(31 downto 0);
    signal d1_error, d1_ready : std_logic;

    signal bus_reads  : integer := 0;
    signal bus_writes : integer := 0;
    -- what the LAST bus write actually was -- a write-back to the wrong address
    -- is the silent corruption mode, so the address is checked, not just a count
    signal last_wr_adr : std_logic_vector(31 downto 0) := (others => '0');
    signal last_wr_dat : std_logic_vector(31 downto 0) := (others => '0');

    constant MEM_WORDS : integer := 8192;
    type mem_t is array (0 to MEM_WORDS - 1) of std_logic_vector(31 downto 0);
    signal mem : mem_t := (others => (others => '0'));

    signal errors : integer := 0;

    -- Same cache SET (bits 7:0), different tag (bits 11:8). Under the 2-way
    -- cache two of these coexist, so THREE are needed to force an eviction --
    -- which is itself the property test 4 checks.
    constant ADDR_A : std_logic_vector(31 downto 0) := x"00000100";
    constant ADDR_B : std_logic_vector(31 downto 0) := x"00000300";
    constant ADDR_C : std_logic_vector(31 downto 0) := x"00000500";
    -- bits 18:16 = "111" -> above RAM_TOP, mem_cacheable0 forces a miss
    constant ADDR_NC : std_logic_vector(31 downto 0) := x"00070004";

    function idx(a : std_logic_vector(31 downto 0)) return integer is
    begin
        return conv_integer(unsigned(a(12 downto 0)));
    end function;

begin

    clk <= not clk after PERIOD / 2 when not done else '0';

    -- Mirrors Kronos.vhdl: stall <= (not ready_d or not ready_s). The cache's
    -- internal invariants depend on this exact wiring -- it is what keeps the
    -- two ports from writing one RAM index in the same cycle, and under
    -- write-back it is also what keeps a FLUSH from racing a refill.
    stall <= (not d0_ready) or (not d1_ready);

    uut : entity work.DataCache
        generic map (address_size => 32)
        port map (
            adr_o => adr_o, dat_i => dat_i, dat_o => dat_o,
            we_o => we_o, cyc_o => cyc_o, stb_o => stb_o, lock_o => lock_o,
            ack_i => ack_i, err_i => err_i,
            a0_bus => a0_bus, a0_read => a0_read, a0_write => a0_write,
            a0_wtlb => a0_wtlb, a0_wbase => a0_wbase,
            d0_in => d0_in, d0_out => d0_out,
            d0_error => d0_error, d0_ready => d0_ready,
            a1_bus => a1_bus, a1_read => a1_read,
            d1_out => d1_out, d1_error => d1_error, d1_ready => d1_ready,
            stall => stall, clock => clk, reset => reset);

    memory : process (clk)
        variable a : integer;
    begin
        if rising_edge(clk) then
            ack_i <= '0';
            if cyc_o = '1' and stb_o = '1' and ack_i = '0' then
                a := conv_integer(unsigned(adr_o(12 downto 0)));
                if we_o = '1' then
                    mem(a)      <= dat_o;
                    last_wr_adr <= adr_o;
                    last_wr_dat <= dat_o;
                    bus_writes  <= bus_writes + 1;
                else
                    dat_i     <= mem(a);
                    bus_reads <= bus_reads + 1;
                end if;
                ack_i <= '1';
            end if;
        end if;
    end process;

    -- 'X' on the fetch path means an undefined M4K read reached the datapath.
    -- Gated on a fetch genuinely being in flight: before the first one the
    -- output register is still 'U' and d1_ready reads '1' only because the port
    -- is IDLE, which the CPU never samples.
    xwatch : process (clk)
        variable p1_seen : boolean := false;
    begin
        if rising_edge(clk) and reset = '0' then
            if a1_read = '1' and d1_ready = '0' then
                p1_seen := true;
            end if;
            if p1_seen and a1_read = '1' and d1_ready = '1' then
                assert not is_x(d1_out)
                    report "FAIL: 'X' on d1_out -- undefined M4K read reached the fetch path"
                    severity failure;
            end if;
        end if;
    end process;

    stimulus : process

        procedure tick is
        begin
            wait until falling_edge(clk);
        end procedure;

        -- Follows the CPU's protocol: completion is `stall = '0'`, NOT
        -- `d0_ready = '1'`. d0_ready reads '1' whenever port 0 is merely IDLE,
        -- so while port 1 holds stall high -- port 0 has not been issued yet,
        -- since a0_stb is `not stall and ...` -- a d0_ready test would report
        -- "complete" for an access that never started and sample a stale bus.
        procedure access0 (
            constant addr : in  std_logic_vector(31 downto 0);
            constant wr   : in  boolean;
            constant wdat : in  std_logic_vector(31 downto 0);
            variable rdat : out std_logic_vector(31 downto 0)) is
        begin
            a0_bus   <= addr;
            d0_in    <= wdat;
            a0_read  <= '0';
            a0_write <= '0';
            if wr then a0_write <= '1'; else a0_read <= '1'; end if;
            tick;
            while stall = '1' loop tick; end loop;   -- the coming edge issues it
            tick;
            while stall = '1' loop tick; end loop;   -- ...and this one retires it
            rdat := d0_out;
            assert not (is_x(d0_out) and not wr)
                report "FAIL: 'X' on d0_out -- undefined M4K read reached the data path"
                severity failure;
            a0_read  <= '0';
            a0_write <= '0';
            tick;
        end procedure;

        procedure load0 (
            constant addr : in  std_logic_vector(31 downto 0);
            variable rdat : out std_logic_vector(31 downto 0)) is
        begin
            access0(addr, false, (31 downto 0 => '0'), rdat);
        end procedure;

        procedure store0 (
            constant addr : in std_logic_vector(31 downto 0);
            constant wdat : in std_logic_vector(31 downto 0)) is
            variable junk : std_logic_vector(31 downto 0);
        begin
            access0(addr, true, wdat, junk);
        end procedure;

        procedure check (
            constant ok  : in boolean;
            constant msg : in string) is
        begin
            if ok then
                report "PASS: " & msg;
            else
                report "FAIL: " & msg severity error;
                errors <= errors + 1;
            end if;
        end procedure;

        variable v      : std_logic_vector(31 downto 0);
        variable r0, w0 : integer;
    begin
        reset <= '1';
        tick; tick; tick;
        reset <= '0';
        tick; tick;

        -- 1 ------------------------------------- store miss: no write-allocate
        w0 := bus_writes;
        store0(ADDR_A, x"AAAA0001");
        check(bus_writes > w0, "store MISS goes to memory (no write-allocate)");
        check(mem(idx(ADDR_A)) = x"AAAA0001", "memory took the missing store");

        -- 2 ---------------------------------- load allocates; re-load hits
        load0(ADDR_A, v);
        check(v = x"AAAA0001", "load after store returns the stored value");
        r0 := bus_reads;
        load0(ADDR_A, v);
        check(bus_reads = r0, "re-load HIT: no bus read (the cache is caching)");

        -- 3 ------------------------------ THE WRITE-BACK WIN: store hit absorbed
        -- Checking memory is essential. "no bus write" alone would also pass if
        -- the store were dropped on the floor; memory must still hold the OLD
        -- value while the cache holds the new one.
        w0 := bus_writes;
        store0(ADDR_A, x"BBBB0002");
        check(bus_writes = w0,
              "store HIT is ABSORBED: no bus write (write-back win)");
        check(mem(idx(ADDR_A)) = x"AAAA0001",
              "memory still holds the OLD value -- the line is dirty, not written");
        load0(ADDR_A, v);
        check(v = x"BBBB0002", "the absorbed store is visible to a later load");

        -- 4 ------------------------------ ASSOCIATIVITY IS ACTUALLY ENGAGED
        -- THE test that a direct-mapped cache cannot pass, and the reason this
        -- design exists: A and B collide in the same set, and under 2-way they
        -- must BOTH stay resident. A cache that quietly used one way would sail
        -- through every correctness check above while delivering nothing.
        --
        -- Two independent symptoms are checked, because either alone is weak:
        -- no write-back (A was not evicted) and a subsequent HIT on A (A is
        -- genuinely still there, not merely un-flushed because it was clean).
        w0 := bus_writes;
        load0(ADDR_B, v);              -- second line into the SAME set
        check(bus_writes = w0,
              "2-WAY: a second line in the set evicts NOTHING (dirty A survives)");
        r0 := bus_reads;
        load0(ADDR_A, v);
        check(bus_reads = r0 and v = x"BBBB0002",
              "2-WAY: the first line still HITS after the second was allocated");

        -- 5 ------------------------------------------- DIRTY eviction writes back
        -- Now a THIRD line into the same set, which must displace one of them.
        -- Round-robin has A as the victim here, and A is the dirty one: the
        -- flush must go to A's address with A's data. A flush to the wrong
        -- address is the silent corruption mode that killed this design before.
        w0 := bus_writes;
        load0(ADDR_C, v);
        check(bus_writes = w0 + 1,
              "dirty eviction writes back EXACTLY ONCE");
        check(last_wr_adr = ADDR_A,
              "write-back went to the ORIGINAL address (not the evicting one)");
        check(last_wr_dat = x"BBBB0002",
              "write-back carried the ORIGINAL data");
        check(mem(idx(ADDR_A)) = x"BBBB0002",
              "memory now holds the absorbed store -- nothing was lost");

        -- 6 ------------------------------------------- CLEAN eviction is silent
        -- The set now holds C and B, both clean. Displacing one must write
        -- nothing; a spurious flush corrupts memory just as surely as a missing
        -- one.
        w0 := bus_writes;
        load0(ADDR_A, v);              -- evicts a CLEAN line
        check(v = x"BBBB0002", "refill after eviction returns memory's value");
        check(bus_writes = w0, "CLEAN eviction writes back NOTHING");

        -- 7 --------------------------- DIRTY eviction from the OTHER way
        -- Everything above evicts way 0, where the hit-way and victim-way tags
        -- happen to be the SAME register -- so a flush that wrongly used the hit
        -- way instead of the victim passed the whole suite unnoticed (caught by
        -- run_cache_checks.sh, not by inspection). Way 1 is where those two
        -- differ, and it was completely untested.
        --
        -- The set currently holds C in way 0 and A in way 1, with round-robin
        -- pointing at way 0. Dirty A, push the pointer past way 0, then force
        -- the eviction of way 1.
        store0(ADDR_A, x"CCCC0007");     -- hit in way 1 -> absorbed, way 1 dirty
        load0(ADDR_B, v);                -- evicts way 0 (C, clean): pointer -> way 1
        w0 := bus_writes;
        load0(ADDR_C, v);                -- evicts way 1 (A, DIRTY)
        check(bus_writes = w0 + 1,
              "dirty eviction from way 1 writes back EXACTLY ONCE");
        check(last_wr_adr = ADDR_A,
              "way-1 write-back went to the ORIGINAL address");
        check(last_wr_dat = x"CCCC0007",
              "way-1 write-back carried the ORIGINAL data");

        -- 8 ------------------------------------ non-cacheable is never absorbed
        -- Mandatory under write-back: the booter's memory_top() probe stores
        -- past the top of RAM and reads back. If the store were absorbed and the
        -- read answered from cache, the probe would match and memory_top would
        -- never terminate -- the machine would not boot.
        w0 := bus_writes;
        store0(ADDR_NC, x"EEEE0005");
        check(bus_writes > w0,
              "non-cacheable store reaches memory (memory_top probe terminates)");
        load0(ADDR_NC, v);
        r0 := bus_reads;
        load0(ADDR_NC, v);
        check(bus_reads > r0, "non-cacheable region is never cached");

        -- 9 ------------------- concurrent port-1 fetches on the SAME index
        -- Port 1 hammers the index port 0 is dirtying and flushing, so evictions
        -- collide with refills. The 'X' watchdog is the real assertion here.
        a1_bus  <= ADDR_A;
        a1_read <= '1';
        for i in 0 to 15 loop
            store0(ADDR_A, x"D0D0" & conv_std_logic_vector(i, 16));
            load0(ADDR_A, v);
            check(v = (x"D0D0" & conv_std_logic_vector(i, 16)),
                  "concurrent fetch + dirty store/load stays coherent");
            -- THREE lines in one 2-way set, so every round really does evict
            load0(ADDR_B, v);
            load0(ADDR_C, v);
            if (i mod 2) = 0 then
                a1_bus <= ADDR_B;
            else
                a1_bus <= ADDR_A;
            end if;
        end loop;
        a1_read <= '0';
        tick;

        -- 10 ------------------------------ everything dirty eventually lands
        load0(ADDR_A, v);
        check(v = (x"D0D0" & conv_std_logic_vector(15, 16)),
              "after 16 dirty rounds memory and cache still agree");

        if errors = 0 then
            report "*** ALL WRITE-BACK CACHE TESTS PASSED ***";
        else
            report "*** WRITE-BACK CACHE TESTS FAILED ***" severity failure;
        end if;
        done <= true;
        wait;
    end process;

end architecture test;

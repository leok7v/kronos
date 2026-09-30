library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Prove the simulation BlockRam reproduces the Cyclone M4K behaviours that
-- broke the cache on hardware. Until this existed, sim/BlockRam returned clean
-- read-first data and every simulation of the cache was quietly wrong.
--
-- Checks, in order of how much damage each did:
--   1. SAME-PORT read-during-write returns NEW data, not old. Cyclone FORCES
--      this (Quartus rejects OLD_DATA for the family). The cache wants
--      read-first -- it reads the tag of the line being EVICTED while writing
--      the tag being INSTALLED -- so this silently breaks eviction.
--   2. MIXED-PORT read-during-write is UNDEFINED without repair: the model
--      drives 'X' so a dependent design poisons itself here instead of on
--      silicon.
--   3. The forwarding repair fixes case 2 -- reader sees the written data.
--   4. A port with clocken low FREEZES. Forwarding must freeze with it; an
--      ungated version corrupted the TLB on hardware (attempt #1).

entity tb_blockram_rdw is
end tb_blockram_rdw;

architecture sim of tb_blockram_rdw is
    constant P : time := 20 ns;
    signal clk : std_logic := '0';
    signal en0, en1, we0, we1 : std_logic := '0';
    signal a0, a1 : std_logic_vector(8 downto 0) := (others => '0');
    signal di0, di1, do0, do1 : std_logic_vector(31 downto 0) := (others => '0');
    signal done : boolean := false;

    function hasX(v : std_logic_vector) return boolean is
    begin
        for i in v'range loop
            if v(i) = 'X' or v(i) = 'U' then return true; end if;
        end loop;
        return false;
    end function;
begin
    clk <= not clk after P/2 when not done else '0';

    dut : entity work.BlockRam
        port map (clock => clk, reset => '0', en0 => en0, en1 => en1,
                  we0 => we0, we1 => we1, a0 => a0, a1 => a1,
                  di0 => di0, di1 => di1, do0 => do0, do1 => do1);

    stim : process
        procedure tick is begin wait until rising_edge(clk); wait for 1 ns; end procedure;
    begin
        en0 <= '1'; en1 <= '1';
        -- seed address 5 with a known value via port 0
        a0 <= std_logic_vector(to_unsigned(5,9));
        a1 <= std_logic_vector(to_unsigned(200,9));   -- far away
        di0 <= x"AAAA0001"; we0 <= '1';
        tick;
        we0 <= '0';
        tick;

        -- ---- 1. SAME-PORT read-during-write must return NEW data
        di0 <= x"BBBB0002"; we0 <= '1';
        tick;
        we0 <= '0';
        assert do0 = x"BBBB0002"
            report "FAIL: same-port RDW did not return NEW data -- model is not "
                 & "faithful to Cyclone (which FORCES NEW_DATA_NO_NBE_READ)"
            severity failure;
        report "1. same-port RDW returns NEW data (as Cyclone forces)" severity note;
        tick;

        -- ---- 2/3. MIXED-PORT: port 1 reads address 5 while port 0 writes it.
        -- The forwarding repair should hand port 1 the written data.
        a1 <= std_logic_vector(to_unsigned(5,9));
        di0 <= x"CCCC0003"; we0 <= '1'; we1 <= '0';
        tick;
        we0 <= '0';
        assert not hasX(do1)
            report "FAIL: mixed-port RDW left 'X' on the reader -- the forwarding "
                 & "repair is not working"
            severity failure;
        assert do1 = x"CCCC0003"
            report "FAIL: forwarding gave the wrong value" severity failure;
        report "2/3. mixed-port RDW repaired by forwarding (reader got the write)"
            severity note;
        tick;

        -- ---- 4. a DISABLED port must freeze, and forwarding must freeze too
        a1 <= std_logic_vector(to_unsigned(200,9));
        tick;                                    -- let port 1 read address 200
        en1 <= '0';                              -- freeze port 1
        a1 <= std_logic_vector(to_unsigned(5,9));
        di0 <= x"DDDD0004"; we0 <= '1';          -- collide, but port 1 is frozen
        tick;
        we0 <= '0';
        assert do1 /= x"DDDD0004"
            report "FAIL: forwarding fired into a FROZEN port -- this is exactly "
                 & "the bug that corrupted the TLB on hardware (tlb0/tlb1 gate "
                 & "their enables)"
            severity failure;
        report "4. forwarding correctly freezes with a disabled port" severity note;

        report "*** PASS: sim BlockRam now models Cyclone M4K faithfully ***"
            severity note;
        done <= true;
        wait;
    end process;
end architecture sim;

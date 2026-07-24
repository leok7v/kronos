library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Behavioural SDR SDRAM model for simulation (16-bit, CAS latency 2, BL=2).
-- Models enough of the protocol to verify sdram_controller: ACTIVE latches the
-- open row per bank; READ drives DQ CAS_LAT cycles later for BL words; WRITE
-- captures DQ from the command cycle for BL words. Storage is a modest array
-- indexed by a truncated {bank,row,col}, so any address works (aliased far
-- apart, which the tests avoid). Not a timing-accurate device model.
--
-- ===========================================================================
-- PROTOCOL CHECKING (added 2026-07-20)
-- ===========================================================================
--
-- This model used to be PERMISSIVE IN THE ONE DIRECTION THAT MATTERS, and it
-- would have waved through the open-row work it was about to be used to verify:
--
--   * PRECHARGE was `when others => null` -- a literal no-op. A row, once
--     ACTIVEd, stayed open in the model forever.
--   * A10 was never examined, so the auto-precharge the controller relies on
--     (A10=1 on every READ/WRITE) was not modelled at all.
--   * No timing was checked: tRCD, tRP, tRAS, tWR, tRFC all unenforced.
--   * REFRESH was accepted with rows open, which a real device will not do.
--
-- Net effect: the model already behaved as though the row were ALWAYS OPEN. An
-- open-row controller therefore passed against it trivially -- including a
-- BROKEN one that forgot a required precharge on a row change, or refreshed
-- with a bank still active. Both of those corrupt on real silicon, and the
-- board is a ~5-minute flash per attempt, so that is the expensive place to
-- find them.
--
-- So the model now tracks per-bank {idle, active} state with timestamps and
-- asserts the protocol. The checks are severity FAILURE, deliberately: a
-- protocol violation is not a warning, it is a design that will not work on the
-- device. Set CHECK => false to silence them (no reason to, they are cheap).
--
-- NEGATIVE CONTROL. These assertions were validated by confirming they REJECT
-- deliberately broken controllers, not merely that they accept the good one --
-- see sim/run_sdram_checks.sh. A check that has never fired has not been
-- tested. That lesson is written down in [[cyclone-cache-cannot-work]] and it
-- cost this project a full evening once already.

entity sdram_model is
    generic (
        CAS_LAT  : integer := 2;
        BL       : integer := 2;
        MEM_HW   : integer := 65536;     -- storage in 16-bit half-words
        -- Protocol timing, in clock cycles. Defaults match the controller's own
        -- generics, which are the values the OneChipBook actually runs.
        CHECK    : boolean := true;
        T_RCD    : integer := 2;         -- ACTIVE -> READ/WRITE
        T_RP     : integer := 2;         -- PRECHARGE -> ACTIVE (same bank)
        T_RAS    : integer := 2;         -- ACTIVE -> PRECHARGE (same bank)
        T_WR     : integer := 2;         -- last write datum -> PRECHARGE
        T_RFC    : integer := 4          -- REFRESH -> next command
    );
    port (
        clk    : in    std_logic;
        a      : in    std_logic_vector(12 downto 0);
        ba     : in    std_logic_vector(1 downto 0);
        dq     : inout std_logic_vector(15 downto 0);
        cke    : in    std_logic;
        cs_n   : in    std_logic;
        ras_n  : in    std_logic;
        cas_n  : in    std_logic;
        we_n   : in    std_logic;
        dqml   : in    std_logic;
        dqmh   : in    std_logic
    );
end sdram_model;

architecture sim of sdram_model is
    -- The array is a PROCESS VARIABLE (below), not a signal, and that is not a
    -- style preference -- it is the difference between a 17 MB simulation and a
    -- 31 GB one.
    --
    -- As `signal mem : mem_t` with the tb's MEM_HW = 1048576, this is 16.7M
    -- std_logic SIGNAL elements, and GHDL carries per-element driver, event and
    -- waveform machinery for every one of them. Measured: ghdl-mcode at 30.8 GB
    -- RSS and still climbing ~1.2 GB/min on a 1100ms run -- enough to exhaust
    -- swap and get unrelated desktop applications OOM-killed. As a variable the
    -- same array is plain memory, ~17 MB, and it runs far faster besides,
    -- because signal assignment was dominating the event loop.
    --
    -- Only this one process touches the array, so a process variable is
    -- sufficient; no shared variable is needed.
    type mem_t is array (0 to MEM_HW-1) of std_logic_vector(15 downto 0);

    type row_t is array (0 to 3) of std_logic_vector(12 downto 0);
    signal open_row : row_t := (others => (others => '0'));

    -- linear half-word address from {bank,row,col}, truncated to the array
    function lin(bank : std_logic_vector; row : std_logic_vector; col : std_logic_vector)
        return integer is
        variable v : unsigned(23 downto 0);
    begin
        v := unsigned(bank) & unsigned(row(12 downto 0)) & unsigned(col(8 downto 0));
        return to_integer(v) mod MEM_HW;
    end function;

    signal rd_delay : integer range 0 to 7 := 0;
    signal rd_left  : integer range 0 to 4 := 0;
    signal rd_addr  : integer range 0 to MEM_HW-1 := 0;
    signal wr_left  : integer range 0 to 4 := 0;
    signal wr_addr  : integer range 0 to MEM_HW-1 := 0;
    signal wr_bank  : integer range 0 to 3 := 0;   -- bank of the burst in flight
    signal dq_drive : std_logic_vector(15 downto 0) := (others => '0');
    signal dq_oe    : std_logic := '0';

    -- ---------------------------------------------------------- bank state
    -- Timestamps are plain integers counting clock cycles, initialised far in
    -- the past so the first command of a run is never judged against a
    -- timestamp that never happened.
    constant LONG_AGO : integer := -1000;
    type bstate_t is array (0 to 3) of boolean;
    type bstamp_t is array (0 to 3) of integer;
    signal bank_active : bstate_t := (others => false);
    signal t_act       : bstamp_t := (others => LONG_AGO);  -- last ACTIVE
    signal t_pre       : bstamp_t := (others => LONG_AGO);  -- last PRECHARGE
    signal t_wrdat     : bstamp_t := (others => LONG_AGO);  -- last write datum
    signal t_ref       : integer  := LONG_AGO;              -- last REFRESH/MRS
    -- pending auto-precharge (A10=1 on READ/WRITE): bank, and the cycle the
    -- burst finishes, after which the device closes the row by itself.
    signal ap_pend     : boolean := false;
    signal ap_bank     : integer range 0 to 3 := 0;
    signal ap_at       : integer := 0;
    signal cyc         : integer := 0;
begin
    dq <= dq_drive when dq_oe = '1' else (others => 'Z');

    process (clk)
        variable cmd : std_logic_vector(3 downto 0);
        variable mem : mem_t := (others => (others => '0'));
        variable rd_val : std_logic_vector(15 downto 0);
        variable b : integer range 0 to 3;
    begin
        if rising_edge(clk) then
            cmd := cs_n & ras_n & cas_n & we_n;
            dq_oe <= '0';
            cyc <= cyc + 1;

            -- Capture the read BEFORE any write below, because that is what the
            -- signal version did. A signal read returns the value from the
            -- previous delta, so a write earlier in this same process body was
            -- NOT visible to the read that followed it; a variable write would
            -- be. Sampling here preserves the old semantics exactly, so the
            -- storage-class change is behaviour-neutral rather than a subtle
            -- read-during-write change to the memory model.
            rd_val := mem(rd_addr);

            -- ---- write data capture (BL words from the WRITE cycle onward) ----
            if wr_left > 0 then
                if dqml = '0' then mem(wr_addr)(7 downto 0)  := dq(7 downto 0);  end if;
                if dqmh = '0' then mem(wr_addr)(15 downto 8) := dq(15 downto 8); end if;
                wr_addr <= (wr_addr + 1) mod MEM_HW;
                wr_left <= wr_left - 1;
                t_wrdat(wr_bank) <= cyc;   -- latched at the WRITE, not re-decoded from ba
            end if;

            -- ---- read data pipeline ----
            if rd_delay > 1 then
                rd_delay <= rd_delay - 1;
            elsif rd_delay = 1 then
                rd_delay <= 0;
                dq_drive <= rd_val; dq_oe <= '1';
                rd_addr  <= (rd_addr + 1) mod MEM_HW;
                rd_left  <= BL - 1;
            elsif rd_left > 0 then
                dq_drive <= rd_val; dq_oe <= '1';
                rd_addr  <= (rd_addr + 1) mod MEM_HW;
                rd_left  <= rd_left - 1;
            end if;

            -- ---- deferred auto-precharge (A10=1 on the READ/WRITE) ----
            -- The device closes the row itself once the burst is done. Modelling
            -- this is what makes the CURRENT controller's behaviour (A10 always
            -- 1) distinguishable from a genuine open-row policy.
            if ap_pend and cyc >= ap_at then
                bank_active(ap_bank) <= false;
                t_pre(ap_bank)       <= cyc;
                ap_pend              <= false;
            end if;

            -- ---- command decode ----
            -- NOTE: `b` is decoded inside each branch that needs it, NOT hoisted
            -- above the case. CMD_NOP is "0111", i.e. cs_n = '0', so this block
            -- runs EVERY cycle including before init -- and a hoisted
            -- to_integer(unsigned(ba)) on an undriven 'U' bank emits a
            -- metavalue warning every one of those cycles. That flood buries
            -- the assertions this model exists to report.
            if cke = '1' and cs_n = '0' then
                case cmd is
                    when "0011" =>                       -- ACTIVE
                        b := to_integer(unsigned(ba));
                        if CHECK then
                            assert not bank_active(b)
                                report "SDRAM PROTOCOL: ACTIVE to bank " & integer'image(b) &
                                       " while it is already active (row must be precharged first)"
                                severity failure;
                            assert cyc - t_pre(b) >= T_RP
                                report "SDRAM TIMING: tRP violated -- ACTIVE " &
                                       integer'image(cyc - t_pre(b)) & " cycles after PRECHARGE (need " &
                                       integer'image(T_RP) & ")"
                                severity failure;
                        end if;
                        open_row(b)    <= a;
                        bank_active(b) <= true;
                        t_act(b)       <= cyc;

                    when "0101" =>                       -- READ
                        b := to_integer(unsigned(ba));
                        if CHECK then
                            assert bank_active(b)
                                report "SDRAM PROTOCOL: READ from bank " & integer'image(b) &
                                       " with no row active (missing ACTIVE, or the row was precharged)"
                                severity failure;
                            assert cyc - t_act(b) >= T_RCD
                                report "SDRAM TIMING: tRCD violated -- READ " &
                                       integer'image(cyc - t_act(b)) & " cycles after ACTIVE (need " &
                                       integer'image(T_RCD) & ")"
                                severity failure;
                        end if;
                        rd_addr  <= lin(ba, open_row(b), a(8 downto 0));
                        rd_delay <= CAS_LAT;
                        if a(10) = '1' then              -- read with auto-precharge
                            ap_pend <= true; ap_bank <= b; ap_at <= cyc + CAS_LAT + BL;
                        end if;

                    when "0100" =>                       -- WRITE (data present this cycle)
                        b := to_integer(unsigned(ba));
                        wr_bank <= b;
                        if CHECK then
                            assert bank_active(b)
                                report "SDRAM PROTOCOL: WRITE to bank " & integer'image(b) &
                                       " with no row active (missing ACTIVE, or the row was precharged)"
                                severity failure;
                            assert cyc - t_act(b) >= T_RCD
                                report "SDRAM TIMING: tRCD violated -- WRITE " &
                                       integer'image(cyc - t_act(b)) & " cycles after ACTIVE (need " &
                                       integer'image(T_RCD) & ")"
                                severity failure;
                        end if;
                        wr_addr <= (lin(ba, open_row(b), a(8 downto 0)) + 1) mod MEM_HW;
                        wr_left <= BL - 1;
                        t_wrdat(b) <= cyc;
                        if dqml = '0' then mem(lin(ba, open_row(b), a(8 downto 0)))(7 downto 0)  := dq(7 downto 0);  end if;
                        if dqmh = '0' then mem(lin(ba, open_row(b), a(8 downto 0)))(15 downto 8) := dq(15 downto 8); end if;
                        if a(10) = '1' then              -- write with auto-precharge
                            ap_pend <= true; ap_bank <= b; ap_at <= cyc + BL - 1 + T_WR;
                        end if;

                    when "0010" =>                       -- PRECHARGE (A10=1 -> all banks)
                        -- PRECHARGE ALL ignores `ba`, and the init sequence
                        -- issues one before the controller has ever driven it,
                        -- so decoding it unconditionally warns on 'U'.
                        if a(10) = '0' then b := to_integer(unsigned(ba)); else b := 0; end if;
                        for i in 0 to 3 loop
                            if a(10) = '1' or i = b then
                                -- Precharging an already-idle bank is legal and
                                -- is a no-op, so only an ACTIVE bank is checked.
                                if CHECK and bank_active(i) then
                                    assert cyc - t_act(i) >= T_RAS
                                        report "SDRAM TIMING: tRAS violated -- PRECHARGE bank " &
                                               integer'image(i) & " only " & integer'image(cyc - t_act(i)) &
                                               " cycles after ACTIVE (need " & integer'image(T_RAS) & ")"
                                        severity failure;
                                    assert cyc - t_wrdat(i) >= T_WR
                                        report "SDRAM TIMING: tWR violated -- PRECHARGE bank " &
                                               integer'image(i) & " only " & integer'image(cyc - t_wrdat(i)) &
                                               " cycles after the last write datum (need " &
                                               integer'image(T_WR) & ")"
                                        severity failure;
                                end if;
                                bank_active(i) <= false;
                                t_pre(i)       <= cyc;
                            end if;
                        end loop;
                        -- a precharge supersedes any auto-precharge in flight
                        ap_pend <= false;

                    when "0001" =>                       -- AUTO REFRESH
                        if CHECK then
                            for i in 0 to 3 loop
                                assert not bank_active(i)
                                    report "SDRAM PROTOCOL: REFRESH with bank " & integer'image(i) &
                                           " still active -- all banks must be precharged first"
                                    severity failure;
                            end loop;
                        end if;
                        t_ref <= cyc;

                    when "0000" =>                       -- MODE REGISTER SET
                        if CHECK then
                            for i in 0 to 3 loop
                                assert not bank_active(i)
                                    report "SDRAM PROTOCOL: MRS with bank " & integer'image(i) &
                                           " still active -- all banks must be precharged first"
                                    severity failure;
                            end loop;
                            assert cyc - t_ref >= T_RFC
                                report "SDRAM TIMING: tRFC violated -- MRS " &
                                       integer'image(cyc - t_ref) & " cycles after REFRESH (need " &
                                       integer'image(T_RFC) & ")"
                                severity failure;
                        end if;
                        t_ref <= cyc;

                    when others => null;                 -- NOP/DESELECT: no effect
                end case;
            end if;
        end if;
    end process;
end sim;

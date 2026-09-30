library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- SD-backed disk controller for the OneChipBook Kronos.
--
-- One block owns the SD card and provides both:
--   1. BOOT: after reset (and once the card is ready) it auto-loads the first
--      BOOT_SECTORS sectors into SDRAM at word 0 and raises boot_done -- this
--      replaces sd_boot_loader + bridge from M3a.
--   2. The Kronos "io2" (opcode 0x92) disk operations, VM-compatible
--      (vm/int/src/VM.cpp DiskOperation): the new microcode pops the io2
--      arguments and writes them into the registers below, then polls STATUS.
--
-- Registers (CPU I/O page, word offsets relative to 0x7F010):
--   +0  SEC    (w) 512-byte sector number (VM seeks sector*512)
--   +1  ADR    (w) unused by io2 (microcode keeps the target address); kept
--                  writable for debug
--   +2  LEN    (w) length in BYTES
--   +3  CMD    (w) VM op: 1 mount, 2 dismount, 3 getsize4kb, 4 read,
--                  5 write, 6 gettime  -- writing starts the operation
--       STATUS (r) bit0 = busy
--   +4  RESULT (r) result of the last op (valid at FLAGS.done): 1 ok, 0 fail
--   +5  FLAGS  (r) bit0 = data word available, bit1 = op done,
--                  bit2 = controller WANTS a word (write ops: the microcode
--                  reads mem[adr] and writes it to DATA)
--   +7  DATA   (w) next word to write (op 5)
--   +6  DSK    (w) disk number (only 0 is backed; others fail)
--   +7  DATA   (r) next data word; reading it consumes the word
--
-- CACHE COHERENCE is why data flows through the DATA register: the CPU's
-- write-back DataCache (shared by the instruction port) would go stale or
-- flush dirty lines over DMA-written memory. Instead the io2 MICROCODE reads
-- DATA (I/O page = uncached, address bit31 set via rg_io) and stores each
-- word through the cache -- coherent for both data reads and instruction
-- fetches by construction. The Wishbone DMA master below is used ONLY for
-- the boot phase, when the CPU is still in reset and the cache is cold.
--
-- Op support: 1/2 -> success; 3 -> one DATA word = DISK_4KB; 4 -> reads
-- (LEN+511)/512 sectors from SEC and delivers LEN/4 words via DATA
-- (little-endian assembly, matching the VM's x86 memory layout); 5/6 -> fail
-- (read-only bring-up; SD write is a later milestone).
--
-- Pacing: SD bytes arrive ~1000 clocks apart at CLK_DIV=64 while the
-- microcode drain loop takes tens of clocks per word, so a single delivery
-- word suffices (dma_over flags violations of that assumption).

entity sd_disk_controller is
    generic (
        CLK_DIV      : integer := 64;    -- sd_reader SPI divider
        BOOT_SECTORS : integer := 4;     -- sectors auto-loaded to word 0 at reset
        -- MULTI-DISK LAYOUT: the card carries a real MBR partition table at
        -- sector 0, and unit N is mapped to the Nth primary partition -- its
        -- base sector and size are READ FROM THE MBR at power-up (see the
        -- MBR_* states), so /dev/xd0..xd3 are genuinely different media whose
        -- offsets live on the card, not in this bitstream. A partition entry
        -- with zero size (or no partition table at all) makes that unit fail,
        -- exactly as the VM does for a missing /dev/xdN.
        --   partition 1 : xd0   (Excelsior root/boot volume, type 0xDA)
        --   partition 2 : xd1   (Excelsior, type 0xDA)
        --   partition 3 : FAT16 exchange volume (type 0x0E, also usable on Linux)
        -- The generics below are the LEGACY FALLBACK used only when sector 0
        -- carries no valid 0xAA55 MBR: the old fixed "sector = unit<<DISK_SHIFT"
        -- region map, so a raw (un-partitioned) card still boots.
        DISK_SHIFT   : integer := 16;        -- fallback: 65536 sectors per unit
        NDISKS       : integer := 2;         -- fallback unit count (superseded by MBR)
        DISK_4KB     : integer := 197;       -- fallback size of unit 0, in 4KB blocks
        DISK1_4KB    : integer := 5210;      -- fallback size of unit 1
        -- watchdog length; a testbench shortens it to prove the no-hang path
        WDOG_CYCLES  : integer := 10000000;   -- getsize4kb answer (xd0.dsk = 808960 B)
        -- ---- io2 gettime (op 6): this board IS the system clock ----
        -- XD.m calls gettime on EVERY disk operation and does
        --     time := tim.pack(y,m,d,hr,mn,sc);  tim.set_time(time)
        -- so whatever comes back here becomes the OS's clock, over and over.
        -- The date is fixed (no battery), but the time of day runs, so the
        -- clock advances during a session instead of standing still.
        -- YEAR MUST BE <= 2017: the shipped Time.pack rejects anything later
        -- (measured -- 2017 is accepted, 2020 and 2026 are refused), and a
        -- rejected date returns -1, which is precisely the bug this fixes.
        CLK_HZ       : integer := 21477270;
        RTC_YEAR     : integer := 2017;
        RTC_MONTH    : integer := 7;
        RTC_DAY      : integer := 19
    );
    port (
        clk       : in  std_logic;
        reset     : in  std_logic;                      -- active high
        -- register interface (decoded strobe from the I/O page)
        reg_adr   : in  std_logic_vector(2 downto 0);
        reg_dat_i : in  std_logic_vector(31 downto 0);
        reg_dat_o : out std_logic_vector(31 downto 0);
        reg_stb   : in  std_logic;                      -- one pulse per access
        reg_we    : in  std_logic;
        -- DMA master (to the SDRAM arbiter; cyc = stb)
        dma_adr   : out std_logic_vector(22 downto 0);
        dma_dat   : out std_logic_vector(31 downto 0);
        dma_stb   : out std_logic;
        dma_we    : out std_logic;
        dma_ack   : in  std_logic;
        -- status
        boot_done : out std_logic;
        boot_fail : out std_logic;
        busy_o    : out std_logic;
        -- SD card SPI
        sd_clk    : out std_logic;
        sd_cs     : out std_logic;
        sd_mosi   : out std_logic;
        sd_miso   : in  std_logic
    );
end sd_disk_controller;

architecture rtl of sd_disk_controller is
    -- sd_reader
    signal rd_req   : std_logic := '0';
    signal lba      : std_logic_vector(31 downto 0) := (others => '0');
    signal s_data   : std_logic_vector(7 downto 0);
    signal s_dvalid : std_logic;
    signal s_ready  : std_logic;
    signal s_error  : std_logic;

    -- registers
    signal r_sec    : unsigned(31 downto 0) := (others => '0');
    signal r_adr    : unsigned(22 downto 0) := (others => '0');
    signal r_len    : unsigned(31 downto 0) := (others => '0');
    signal r_dsk    : unsigned(31 downto 0) := (others => '0');
    -- first sector of the selected unit; 0 during boot, when r_dsk is 0
    signal disk_base : unsigned(31 downto 0) := (others => '0');
    -- Partition table read from the card's MBR at power-up. part_base(N) is the
    -- first 512-byte sector of unit N, part_size(N) its length in sectors; a
    -- zero size marks an absent unit. Loaded with the legacy sector=unit<<SHIFT
    -- map first (so a card with no MBR still works) and overwritten from the
    -- MBR when a valid 0xAA55 signature is found. t_base/t_size stage the bytes
    -- as they stream in, committed only once the signature checks out.
    type lba_arr is array(0 to 3) of unsigned(31 downto 0);
    signal part_base : lba_arr := (others => (others => '0'));
    signal part_size : lba_arr := (others => (others => '0'));
    signal t_base    : lba_arr := (others => (others => '0'));
    signal t_size    : lba_arr := (others => (others => '0'));
    signal part_valid: std_logic := '0';
    signal mbr_sig   : unsigned(15 downto 0) := (others => '0');
    signal r_result : std_logic := '0';
    signal r_busy   : std_logic := '0';
    signal op_done  : std_logic := '0';
    signal dat_valid: std_logic := '0';
    signal dat_reg  : std_logic_vector(31 downto 0) := (others => '0');

    -- engine
    -- W_FILL: ask the CPU for a word.  W_RUN: stream it out to the card.
    type st_t is (D_WAITCARD, MBR_REQ, MBR_DATA, MBR_DONE, MBR_BOOT,
                  D_BOOT, D_IDLE, D_DISPATCH,
                  R_REQ, R_DATA, R_NEXT, R_FINISH, D_SIZE, D_FAILED,
                  -- W_FILL: ask the CPU for a word. W_START: kick off CMD24.
                  -- W_RUN: stream bytes until the card releases busy.
                  W_FILL, W_START, W_RUN, W_NEXT, D_TIME);
    signal st        : st_t := D_WAITCARD;
    signal booting   : std_logic := '1';
    signal boot_done_r : std_logic := '0';
    signal boot_fail_r : std_logic := '0';

    signal cur_sec   : unsigned(31 downto 0) := (others => '0');
    signal secs_left : unsigned(15 downto 0) := (others => '0');
    signal words_left: unsigned(15 downto 0) := (others => '0');
    signal wr_ptr    : unsigned(22 downto 0) := (others => '0');
    signal bcount    : integer range 0 to 3 := 0;
    signal sbyte_cnt : integer range 0 to 511 := 0;
    signal wbuf      : std_logic_vector(23 downto 0) := (others => '0');

    -- DMA skid buffer
    signal dma_pend  : std_logic := '0';
    signal dma_adr_r : unsigned(22 downto 0) := (others => '0');
    signal dma_dat_r : std_logic_vector(31 downto 0) := (others => '0');
    signal dma_over  : std_logic := '0';   -- overrun diagnostic (should never set)

    signal cmd_op    : unsigned(3 downto 0) := (others => '0');

    -- write side. No 512-byte buffer: SPI is host-clocked, so the reader simply
    -- stalls between bytes if the CPU is slow, and words are streamed straight
    -- from the CPU through a 4-byte shift register. Bytes go out LOW BYTE FIRST
    -- to match the little-endian order the read path assembles.
    signal wsh       : std_logic_vector(31 downto 0) := (others => '0');
    signal bsel      : integer range 0 to 3 := 0;
    signal wants     : std_logic := '0';   -- FLAGS bit2: waiting for a word
    signal wr_req    : std_logic := '0';
    signal wr_next_i : std_logic;
    signal wr_valid_s: std_logic;
    -- PER-BYTE ready. "not wants" is not enough: it only drops at word
    -- boundaries, so within a word the reader could re-load wr_data before the
    -- shift had landed and send the same byte twice (the card then received one
    -- extra leading byte and lost the last one).
    signal byte_rdy  : std_logic := '0';
    -- Watchdog. A stuck write must NOT hang the machine: the CPU sits in the
    -- io2 poll loop waiting for FLAGS.done, so anything that never completes
    -- freezes the whole system rather than returning an error the OS can
    -- report. ~0.5 s at 21.5 MHz is far longer than any legitimate block write
    -- (a slow card takes a few ms).
    constant WDOG_MAX : integer := WDOG_CYCLES;
    signal wdog      : integer range 0 to WDOG_MAX := 0;
    signal wr_active : std_logic := '0';
    signal tsel      : integer range 0 to 5 := 0;   -- gettime word index

    -- free-running time of day for gettime (the date stays fixed)
    signal tick_cnt  : integer range 0 to CLK_HZ - 1 := 0;
    signal t_sec     : integer range 0 to 59 := 0;
    signal t_min     : integer range 0 to 59 := 0;
    signal t_hour    : integer range 0 to 23 := 0;

    -- re-init the card after a failed read, so the caller's retry loop
    -- (the booter does REPEAT read UNTIL ok) gets a fresh attempt
    signal rdr_clear : std_logic := '0';
    signal rdr_reset : std_logic;
begin
    -- Time of day for io2 gettime. The board has no battery, so the DATE is a
    -- fixed generic, but the clock has to RUN: XD.m re-reads gettime on every
    -- disk operation and overwrites the OS clock with it, so a frozen value
    -- here would freeze the whole machine's sense of time.
    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                tick_cnt <= 0; t_sec <= 0; t_min <= 0; t_hour <= 0;
            elsif tick_cnt = CLK_HZ - 1 then
                tick_cnt <= 0;
                if t_sec /= 59 then
                    t_sec <= t_sec + 1;
                else
                    t_sec <= 0;
                    if t_min /= 59 then
                        t_min <= t_min + 1;
                    else
                        t_min <= 0;
                        if t_hour /= 23 then t_hour <= t_hour + 1;
                        else t_hour <= 0; end if;
                    end if;
                end if;
            else
                tick_cnt <= tick_cnt + 1;
            end if;
        end if;
    end process;

    rdr_reset <= reset or rdr_clear;

    sd : entity work.sd_reader
        generic map (CLK_DIV => CLK_DIV)
        port map (clk => clk, reset => rdr_reset, rd_req => rd_req, lba => lba,
                  data => s_data, data_valid => s_dvalid, ready => s_ready, error => s_error,
                  wr_req => wr_req, wr_data => wsh(7 downto 0), wr_next => wr_next_i,
            -- hold the card's clock while we are still waiting on the CPU
            wr_valid => wr_valid_s,
            sd_clk => sd_clk, sd_cs => sd_cs, sd_mosi => sd_mosi, sd_miso => sd_miso);

    boot_done <= boot_done_r;
    boot_fail <= boot_fail_r;
    busy_o    <= r_busy;

    dma_stb <= dma_pend;
    dma_we  <= '1';
    dma_adr <= std_logic_vector(dma_adr_r);
    dma_dat <= dma_dat_r;

    -- register reads (combinational)
    -- hold the card's clock while we are still waiting on the CPU for a word
    wr_valid_s <= byte_rdy;

    reg_dat_o <=
        (0 => r_busy, others => '0')                     when reg_adr = "011" else
        (0 => r_result, others => '0')                   when reg_adr = "100" else
        (2 => wants, 1 => op_done, 0 => dat_valid, others => '0')
                                                         when reg_adr = "101" else
        dat_reg                                          when reg_adr = "111" else
        std_logic_vector(r_sec)                          when reg_adr = "000" else
        std_logic_vector(r_len(31 downto 0))             when reg_adr = "010" else
        (others => '0');

    process (clk)
        variable ent  : integer range 0 to 3;    -- MBR partition entry index
        variable fld  : integer range 0 to 15;   -- byte offset within an entry
        variable lane : integer range 0 to 3;    -- byte lane within a 32-bit field
    begin
        if rising_edge(clk) then
            rd_req <= '0';
            wr_req <= '0';        -- both are 1-cycle request pulses
            rdr_clear <= '0';
            if reset = '1' then
                st <= D_WAITCARD; booting <= '1';
                boot_done_r <= '0'; boot_fail_r <= '0';
                r_busy <= '1'; r_result <= '0';
                op_done <= '0'; dat_valid <= '0';
                dma_pend <= '0'; dma_over <= '0';
                r_sec <= (others => '0'); r_adr <= (others => '0');
                r_len <= (others => '0'); r_dsk <= (others => '0');
            else
                -- DMA completion (boot phase only)
                if dma_pend = '1' and dma_ack = '1' then
                    dma_pend <= '0';
                end if;

                -- reading DATA consumes the word (delivery below may override)
                if reg_stb = '1' and reg_we = '0' and reg_adr = "111" then
                    dat_valid <= '0';
                end if;

                -- WRITING DATA supplies the next word for a write op. This must
                -- sit OUTSIDE the r_busy guard below: the CPU hands words over
                -- precisely while the controller is busy streaming them to the
                -- card, so gating it on "idle" left `wants` set forever and the
                -- operation never finished.
                if reg_stb = '1' and reg_we = '1' and reg_adr = "111" then
                    wsh      <= reg_dat_i;
                    wants    <= '0';
                    byte_rdy <= '1';      -- fresh word: low byte is ready
                end if;

                -- register writes (allowed while idle; CMD dispatches)
                if reg_stb = '1' and reg_we = '1' and r_busy = '0' then
                    case reg_adr is
                        when "000" => r_sec <= unsigned(reg_dat_i);
                        when "001" => r_adr <= unsigned(reg_dat_i(22 downto 0));
                        when "010" => r_len <= unsigned(reg_dat_i);
                        when "110" => r_dsk <= unsigned(reg_dat_i);
                        when "011" =>
                            cmd_op <= unsigned(reg_dat_i(3 downto 0));
                            r_busy <= '1'; op_done <= '0';
                            dat_valid <= '0'; r_result <= '0';
                            wdog <= 0;
                            st <= D_DISPATCH;
                        when others => null;
                    end case;
                end if;

                -- WATCHDOG: only while an operation is outstanding. A stuck
                -- write must not hang the machine -- the CPU sits in the io2
                -- poll loop waiting for FLAGS.done, so anything that never
                -- finishes freezes the whole system instead of returning an
                -- error the OS can report and recover from.
                if r_busy = '1' then
                    if wdog = WDOG_MAX then
                        r_result <= '0'; op_done <= '1'; r_busy <= '0';
                        wants <= '0'; wr_active <= '0';
                        st <= D_IDLE;
                    else
                        wdog <= wdog + 1;
                    end if;
                end if;

                case st is
                    -- ---------------- boot: card init then sectors 0..N-1 to word 0
                    -- ---- power-up: wait for the card, then read the MBR ----
                    when D_WAITCARD =>
                        if s_error = '1' then
                            boot_fail_r <= '1'; st <= D_FAILED;
                        elsif s_ready = '1' then
                            -- legacy fallback map (used if there is no valid MBR)
                            part_base(0) <= (others => '0');
                            part_base(1) <= shift_left(to_unsigned(1, 32), DISK_SHIFT);
                            part_base(2) <= shift_left(to_unsigned(2, 32), DISK_SHIFT);
                            part_base(3) <= shift_left(to_unsigned(3, 32), DISK_SHIFT);
                            part_size(0) <= to_unsigned(DISK_4KB  * 8, 32);
                            part_size(1) <= to_unsigned(DISK1_4KB * 8, 32);
                            part_size(2) <= (others => '0');
                            part_size(3) <= (others => '0');
                            part_valid   <= '0';
                            t_base  <= (others => (others => '0'));
                            t_size  <= (others => (others => '0'));
                            mbr_sig <= (others => '0');
                            disk_base <= (others => '0');   -- the MBR is at sector 0
                            st <= MBR_REQ;
                        end if;

                    -- Read sector 0 and latch the four primary partition entries
                    -- (LBA start at entry+8, sector count at entry+12) plus the
                    -- 0xAA55 signature. Bytes stream in low-address first.
                    when MBR_REQ =>
                        if s_error = '1' then
                            st <= MBR_BOOT;             -- no MBR: legacy map stands
                        elsif s_ready = '1' then
                            lba <= (others => '0');
                            rd_req <= '1';
                            sbyte_cnt <= 0;
                            st <= MBR_DATA;
                        end if;

                    when MBR_DATA =>
                        if s_error = '1' then
                            st <= MBR_BOOT;
                        elsif s_dvalid = '1' then
                            if sbyte_cnt >= 446 and sbyte_cnt <= 509 then
                                ent  := (sbyte_cnt - 446) / 16;
                                fld  := (sbyte_cnt - 446) mod 16;
                                lane := fld mod 4;
                                if fld >= 8 and fld <= 11 then          -- LBA start
                                    case lane is
                                        when 0 => t_base(ent)(7 downto 0)   <= unsigned(s_data);
                                        when 1 => t_base(ent)(15 downto 8)  <= unsigned(s_data);
                                        when 2 => t_base(ent)(23 downto 16) <= unsigned(s_data);
                                        when others => t_base(ent)(31 downto 24) <= unsigned(s_data);
                                    end case;
                                elsif fld >= 12 then                    -- sector count
                                    case lane is
                                        when 0 => t_size(ent)(7 downto 0)   <= unsigned(s_data);
                                        when 1 => t_size(ent)(15 downto 8)  <= unsigned(s_data);
                                        when 2 => t_size(ent)(23 downto 16) <= unsigned(s_data);
                                        when others => t_size(ent)(31 downto 24) <= unsigned(s_data);
                                    end case;
                                end if;
                            elsif sbyte_cnt = 510 then
                                mbr_sig(7 downto 0)  <= unsigned(s_data);
                            elsif sbyte_cnt = 511 then
                                mbr_sig(15 downto 8) <= unsigned(s_data);
                            end if;
                            if sbyte_cnt = 511 then
                                st <= MBR_DONE;
                            else
                                sbyte_cnt <= sbyte_cnt + 1;
                            end if;
                        end if;

                    when MBR_DONE =>
                        -- A valid table (0xAA55 with a non-empty partition 1)
                        -- replaces the legacy map; otherwise the fallback stands.
                        if mbr_sig = x"AA55" and t_size(0) /= 0 then
                            part_base <= t_base;
                            part_size <= t_size;
                            part_valid <= '1';
                        else
                            part_valid <= '0';
                        end if;
                        st <= MBR_BOOT;

                    -- Boot from the first sector of unit 0 -- part_base(0), which
                    -- is the partition-1 start on a partitioned card and 0 on a
                    -- legacy one, so the same path serves both.
                    when MBR_BOOT =>
                        if BOOT_SECTORS > 0 then
                            disk_base  <= part_base(0);
                            cur_sec    <= (others => '0');
                            secs_left  <= to_unsigned(BOOT_SECTORS, 16);
                            words_left <= to_unsigned(BOOT_SECTORS*128, 16);
                            wr_ptr     <= (others => '0');
                            st <= R_REQ;
                        else
                            booting <= '0'; boot_done_r <= '1';
                            r_busy <= '0'; st <= D_IDLE;
                        end if;

                    when D_BOOT =>          -- (unused placeholder state)
                        st <= D_IDLE;

                    when D_IDLE =>
                        null;               -- waiting for a CMD write

                    when D_DISPATCH =>
                        -- Map the requested unit to its partition (base + size
                        -- read from the MBR at boot). Units past the table, or
                        -- ones whose partition is empty, fail like a missing
                        -- /dev/xdN. r_dsk(1:0) is always in 0..3, so indexing the
                        -- 4-entry table is safe even when r_dsk itself is larger.
                        if r_dsk > 3 or part_size(to_integer(r_dsk(1 downto 0))) = 0 then
                            r_result <= '0'; op_done <= '1'; r_busy <= '0'; st <= D_IDLE;
                        else
                            disk_base <= part_base(to_integer(r_dsk(1 downto 0)));
                            case to_integer(cmd_op) is
                                when 1 | 2 =>           -- mount / dismount
                                    r_result <= '1'; op_done <= '1'; r_busy <= '0'; st <= D_IDLE;
                                when 3 =>               -- getsize4kb: partition sectors / 8
                                    dat_reg <= std_logic_vector(
                                        shift_right(part_size(to_integer(r_dsk(1 downto 0))), 3));
                                    dat_valid <= '1';
                                    st <= D_SIZE;
                                when 4 =>               -- read LEN bytes from SEC
                                    if r_len = 0 then
                                        r_result <= '1'; op_done <= '1'; r_busy <= '0'; st <= D_IDLE;
                                    else
                                        cur_sec    <= r_sec;
                                        secs_left  <= resize(shift_right(r_len + 511, 9), 16);
                                        words_left <= resize(shift_right(r_len, 2), 16);
                                        wr_ptr     <= r_adr;
                                        st <= R_REQ;
                                    end if;
                                when 5 =>               -- write LEN bytes at SEC
                                    if r_len = 0 then
                                        r_result <= '1'; op_done <= '1'; r_busy <= '0'; st <= D_IDLE;
                                    else
                                        cur_sec    <= r_sec;
                                        secs_left  <= resize(shift_right(r_len + 511, 9), 16);
                                        words_left <= resize(shift_right(r_len + 3, 2), 16);
                                        bsel <= 0;
                                        st <= W_FILL;
                                    end if;
                                when 6 =>               -- gettime: 6 words via DATA
                                    tsel <= 0;
                                    -- FULL year. Handing back year-1900 (126)
                                    -- made Time.pack reject it and return -1,
                                    -- so every disk access reset the OS clock
                                    -- to the 1986 epoch.
                                    dat_reg <= std_logic_vector(to_unsigned(RTC_YEAR, 32));
                                    dat_valid <= '1';
                                    st <= D_TIME;
                                when others =>          -- unknown op
                                    r_result <= '0'; op_done <= '1'; r_busy <= '0'; st <= D_IDLE;
                            end case;
                        end if;

                    when D_SIZE =>
                        if dat_valid = '0' then          -- size word consumed
                            r_result <= '1'; op_done <= '1'; r_busy <= '0'; st <= D_IDLE;
                        end if;

                    -- ---------------- sector read engine (boot + op 4)
                    when R_REQ =>
                        if s_error = '1' then
                            st <= R_FINISH;
                        elsif s_ready = '1' then
                            lba <= std_logic_vector(disk_base + cur_sec);
                            rd_req <= '1';
                            bcount <= 0; sbyte_cnt <= 0;
                            st <= R_DATA;
                        end if;

                    when R_DATA =>
                        if s_error = '1' then
                            st <= R_FINISH;
                        elsif s_dvalid = '1' then
                            case bcount is
                                when 0 => wbuf(7 downto 0)   <= s_data;
                                when 1 => wbuf(15 downto 8)  <= s_data;
                                when 2 => wbuf(23 downto 16) <= s_data;
                                when others => null;
                            end case;
                            if bcount = 3 then
                                bcount <= 0;
                                if words_left /= 0 then
                                    if booting = '1' then         -- boot: DMA to SDRAM
                                        if dma_pend = '1' then dma_over <= '1'; end if;
                                        dma_adr_r <= wr_ptr;
                                        dma_dat_r <= s_data & wbuf;   -- little-endian word
                                        dma_pend  <= '1';
                                        wr_ptr    <= wr_ptr + 1;
                                    else                          -- io2: DATA register
                                        if dat_valid = '1' then dma_over <= '1'; end if;
                                        dat_reg   <= s_data & wbuf;
                                        dat_valid <= '1';
                                    end if;
                                    words_left <= words_left - 1;
                                end if;
                            else
                                bcount <= bcount + 1;
                            end if;
                            if sbyte_cnt = 511 then      -- sector exhausted
                                sbyte_cnt <= 0;
                                st <= R_NEXT;
                            else
                                sbyte_cnt <= sbyte_cnt + 1;
                            end if;
                        end if;

                    when R_NEXT =>
                        if secs_left = 1 then
                            st <= R_FINISH;
                        else
                            secs_left <= secs_left - 1;
                            cur_sec   <= cur_sec + 1;
                            st <= R_REQ;
                        end if;

                    when R_FINISH =>
                        if booting = '1' then
                            if dma_pend = '0' then       -- last boot word flushed
                                if s_error = '1' then
                                    boot_fail_r <= '1'; st <= D_FAILED;
                                else
                                    booting <= '0'; boot_done_r <= '1';
                                    r_busy <= '0'; st <= D_IDLE;
                                end if;
                            end if;
                        elsif dat_valid = '0' then       -- last word consumed by microcode
                            if s_error = '1' then
                                r_result <= '0';
                                rdr_clear <= '1';        -- re-init card for the retry
                            else
                                r_result <= '1';
                            end if;
                            op_done <= '1'; r_busy <= '0'; st <= D_IDLE;
                        end if;

                    -- ---------------- write (op 5) ----------------
                    when W_FILL =>
                        -- ask the CPU for the next word; it answers by writing
                        -- DATA, which loads wsh and clears `wants` above
                        if wants = '0' then
                            if words_left /= 0 then
                                wants <= '1'; words_left <= words_left - 1;
                                st <= W_START;
                            else
                                wsh <= (others => '0');   -- pad a short tail
                                st <= W_START;
                            end if;
                        end if;

                    when W_START =>
                        -- s_error MUST be checked here too: a card that rejects
                        -- CMD24 leaves the reader in its failure state, which
                        -- never asserts ready -- waiting on ready alone hung the
                        -- machine instead of reporting a failed write.
                        if s_error = '1' then
                            r_result <= '0'; op_done <= '1'; r_busy <= '0';
                            wants <= '0'; st <= D_IDLE;
                        elsif wants = '0' and s_ready = '1' then
                            lba <= std_logic_vector(disk_base + cur_sec);
                            wr_req <= '1';
                            wr_active <= '1';
                            bsel <= 0;
                            st <= W_RUN;
                        end if;

                    when W_RUN =>
                        if wr_next_i = '1' then
                            byte_rdy <= '0';        -- consumed; nothing valid yet
                            if bsel = 3 then
                                bsel <= 0;
                                -- next word: ask the CPU, or pad once the
                                -- caller's length has been satisfied
                                if words_left /= 0 then
                                    wants <= '1'; words_left <= words_left - 1;
                                else
                                    wsh <= (others => '0');
                                    byte_rdy <= '1';
                                end if;
                            else
                                bsel <= bsel + 1;
                                wsh <= x"00" & wsh(31 downto 8);
                                byte_rdy <= '1';    -- valid together with the shift
                            end if;
                        end if;
                        if s_error = '1' then
                            r_result <= '0'; op_done <= '1'; r_busy <= '0';
                            wr_active <= '0'; st <= D_IDLE;
                        elsif wr_active = '1' and s_ready = '1' and wr_req = '0' then
                            wr_active <= '0'; st <= W_NEXT;   -- block written
                        end if;

                    when W_NEXT =>
                        if secs_left = 1 then
                            r_result <= '1'; op_done <= '1'; r_busy <= '0';
                            st <= D_IDLE;
                        else
                            secs_left <= secs_left - 1;
                            cur_sec   <= cur_sec + 1;
                            -- straight to W_START, NOT W_FILL: the first word of
                            -- this sector was already requested at the previous
                            -- sector's last byte boundary. Going through W_FILL
                            -- asked for it a second time, so from sector 2 on the
                            -- word accounting drifted and the data was wrong.
                            st <= W_START;
                        end if;

                    -- ---------------- gettime (op 6) ----------------
                    -- The word order is the one XD.m actually reads into
                    --     RECORD y,m,d,hr,mn,sc END
                    -- and it matches the reference VM, which fills the buffer
                    -- with wYear / wMonth / wDay / wHour / wMinute / wSecond.
                    -- The previous code sent year-1900 and a WEEKDAY in the
                    -- day-of-month slot; Time.pack rejected the year outright
                    -- and returned -1, so XD.doio -- which runs this on EVERY
                    -- disk operation -- reset the system clock to the epoch
                    -- every time anything touched the disk. That is why the
                    -- clock kept falling back to 1-Jan-1986 with no reboot,
                    -- and why kc could never save a usable KC.SETUP.
                    when D_TIME =>
                        if dat_valid = '0' then
                            if tsel = 5 then
                                r_result <= '1'; op_done <= '1'; r_busy <= '0';
                                st <= D_IDLE;
                            else
                                tsel <= tsel + 1;
                                case tsel is
                                    when 0 => dat_reg <= std_logic_vector(to_unsigned(RTC_MONTH, 32));
                                    when 1 => dat_reg <= std_logic_vector(to_unsigned(RTC_DAY, 32));
                                    when 2 => dat_reg <= std_logic_vector(to_unsigned(t_hour, 32));
                                    when 3 => dat_reg <= std_logic_vector(to_unsigned(t_min, 32));
                                    when others => dat_reg <= std_logic_vector(to_unsigned(t_sec, 32));
                                end case;
                                dat_valid <= '1';
                            end if;
                        end if;

                    when D_FAILED =>
                        r_busy <= '0';                   -- boot failed; stay here
                end case;
            end if;
        end if;
    end process;
end rtl;

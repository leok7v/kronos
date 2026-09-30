library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

-- OneChipBook Kronos OneChipBook: boot the REAL Excelsior OS from the SD card.
--
-- Differences from M3a (stub boot):
--   * sd_disk_controller replaces sd_boot_loader+bridge: it auto-loads xd0.dsk
--     sectors 0-3 (the genuine two-stage boot code) into SDRAM at word 0, then
--     serves VM-compatible io2 disk reads via registers on the I/O page.
--   * dl11_console at I/O page 0xFB8-0xFBB (= word 0x7FFB8): the booter/OS
--     console, bridged to a real UART on the DB9 (57600 8N1).
--   * requires the VM-compatible microcode (local Microcode.vhdl with io0/io1/
--     io2 etc.) -- see Kronos.mkas changes in this port.
--
-- I/O page = top 4KB of the word address space (0x7F000-0x7FFFF, the CPU's
-- hardwired rg_io window):
--   0x000            legacy console (stub-boot diag; harmless to the real OS)
--   0x010-0x017      sd_disk_controller registers (io2 microcode target)
--   0xFB8-0xFBB      DL11 console (RCSR/RBUF/XCSR/XBUF)
--   anything else    reads 0, writes ignored (always acks)

entity OneChipBook is
    generic (
        RESET_CYCLES : integer := 200000;
        SD_T_INIT    : integer := 5000;
        SD_T_REFI    : integer := 150;
        SD_READ_LAT  : integer := 3;         -- measured on hardware (M2)
        SD_CLK_DIV   : integer := 64;        -- SPI = clk/(2*DIV); 64 => ~168 kHz
        -- System clock in Hz. EVERY time-derived constant below is computed
        -- from this, so raising the clock cannot silently leave the UART or the
        -- OS's 50Hz tick calibrated for the old frequency -- which would show up
        -- as console garbage and a fast system clock, neither obviously a
        -- clock-bump symptom.
        CLK_HZ       : integer := 21477270;
        BAUD_DIV     : integer := 373;       -- CLK_HZ / 57600
        DISK_4KB     : integer := 197;       -- xd0.dsk = 808960 bytes
        -- GHDL cannot elaborate altpll, so simulation substitutes the system
        -- clock for the pixel clock (video timing wrong, everything else fine).
        USE_PLL      : boolean := true;
        -- Console RECEIVE off the DB9. Leave FALSE on this board.
        -- uart_txd is PIN_3 and uart_rxd is PIN_2, right next to it, with
        -- nothing attached to the connector. The console's own transmissions
        -- capacitively couple into the floating RX pin (the same crosstalk the
        -- uart_beacon work characterised), and the DL11 dutifully decodes the
        -- occasional well-formed frame. That closes a FEEDBACK LOOP: the OS
        -- prints -> TX toggles -> a phantom character arrives -> the OS reports
        -- the receiver status -> which prints again. On hardware that showed up
        -- as "0080h" (= RCSR with rx_avail set, no rx_ien) repeating forever
        -- straight after "Loaded OK". Holding RX at its idle level makes the
        -- board behave like the reference VM, which simply waits for input.
        -- Real console input will come from the PS/2 keyboard (M5), not here.
        --
        -- ENABLED 2026-07-19, now that the serial console actually works (GND
        -- on DB9 pin 9, board TX on pin 2 -- the vendor pin table was wrong,
        -- see ONECHIPBOOK_PINOUT.md). The crosstalk above was a FLOATING-pin
        -- artifact: with a real adapter driving the line the pin is held at a
        -- defined level and the phantom frames cannot form. To keep that true
        -- when NO adapter is plugged in, uart_rxd now carries a WEAK PULL-UP
        -- (see the .qsf), which parks it at the UART idle/mark level so no
        -- start bit can ever be manufactured out of noise.
        UART_RX_EN   : boolean := true;
        -- Suspend-to-RAM sleep mode. OFF by default so a normal build is the
        -- validated bitstream unchanged (the whole sequencer sits inside a
        -- generate that is not emitted when false, and halt/suspend are tied to
        -- '0'). When true: after SUSPEND_IDLE_CYCLES with no keypress, the next
        -- time the CPU is parked on the IDLE instruction the machine drops the
        -- SDRAM into self-refresh and freezes the CPU; any key wakes it. See
        -- src/suspend_ctrl.vhdl. Default idle window ~1 s at 21.477 MHz -- well
        -- over the 20 ms tick period so ordinary between-tick idle never trips.
        SUSPEND_EN          : boolean := false;
        -- Power-on default sleep timeout, in SECONDS. The OS can change it at
        -- runtime by writing the I/O register at word 0x7FF020 (via out(0x20,s));
        -- 0 disables auto-sleep. 1200 = 20 min.
        SUSPEND_DEFAULT_SEC : integer := 1200;
        -- Diagnostic: when true, NORMAL mode repurposes the (otherwise dark)
        -- LED2..LED8 to show the suspend machinery live, so a board with no UART
        -- can see WHY sleep is/ isn't triggering. See the led_diag block. Off for
        -- a clean board.
        SUSPEND_DIAG        : boolean := false
    );
    port (
        clk      : in    std_logic;
        -- RAW oscillator, for the video PLL only. Cyclone I cannot drive one
        -- PLL from another's output ("inclk0 port of PLL must be driven by a
        -- non-inverted input pin"), and once clk became the SYSTEM PLL's output
        -- the video PLL had to keep a direct path to the pin. Defaults to clk so
        -- the testbench -- which drives the core straight from the oscillator --
        -- needs no change.
        clk_ref  : in    std_logic := '0';
        rst_n    : in    std_logic;
        led      : out   std_logic_vector(8 downto 0);
        sw       : in    std_logic_vector(7 downto 0);  -- DIP: debug readout select
        -- SD (SPI)
        sd_sclk  : out   std_logic;
        sd_mosi  : out   std_logic;
        sd_cs    : out   std_logic;
        sd_miso  : in    std_logic;
        -- UART (DB9 #1)
        uart_txd : out   std_logic;
        uart_rxd : in    std_logic;
        -- SDRAM
        sd_a     : out   std_logic_vector(12 downto 0);
        sd_ba    : out   std_logic_vector(1 downto 0);
        sd_dq    : inout std_logic_vector(15 downto 0);
        sd_cke   : out   std_logic;
        sd_cs_n  : out   std_logic;
        sd_ras_n : out   std_logic;
        sd_cas_n : out   std_logic;
        sd_we_n  : out   std_logic;
        sd_dqml  : out   std_logic;
        sd_dqmh  : out   std_logic;
        sd_clk   : out   std_logic;
        -- PS/2 keyboard (shared with the board's built-in keyboard MCU)
        ps2_clk   : in   std_logic;
        ps2_data  : in   std_logic;
        -- VGA text console (18-bit resistor DAC; we drive white-on-black)
        vga_hsync : out  std_logic;
        vga_vsync : out  std_logic;
        vga_r     : out  std_logic_vector(5 downto 0);
        vga_g     : out  std_logic_vector(5 downto 0);
        vga_b     : out  std_logic_vector(5 downto 0);
        -- debug taps (unconnected in synthesis; used by the testbench)
        dbg_adr  : out   std_logic_vector(22 downto 0);
        dbg_stb  : out   std_logic;
        dbg_we   : out   std_logic;
        dbg_dat  : out   std_logic_vector(31 downto 0);
        dbg_dati : out   std_logic_vector(31 downto 0);
        dbg_ack  : out   std_logic
    );
end OneChipBook;

architecture Behaviour of OneChipBook is
    signal reset     : std_logic := '1';
    signal reset_cnt : integer range 0 to RESET_CYCLES := 0;
    signal prescaler : std_logic_vector(23 downto 0) := (others => '0');

    -- CPU
    signal cpu_halted, cpu_idle : std_logic;
    signal cpu_ireq   : std_logic_vector(3 downto 0) := (others => '0');
    -- suspend-to-RAM (see suspend_ctrl); all constant '0' when SUSPEND_EN=false
    signal cpu_halt       : std_logic;
    signal sdc_suspend    : std_logic;
    signal sdc_suspended  : std_logic;
    signal sleeping       : std_logic;
    signal susp_activity  : std_logic;
    signal sc_reset       : std_logic;   -- suspend_ctrl reset: held until boot done
    -- OS-writable sleep timeout (seconds) at I/O word 0x7FF020, and a 1 Hz tick.
    signal timeout_sec    : std_logic_vector(15 downto 0);
    signal sel_timeout    : std_logic;
    signal sel_sleepnow   : std_logic;   -- I/O 0x7FF021: write = sleep now
    signal force_sleep    : std_logic;
    signal sec_tick       : std_logic := '0';
    signal sec_cnt        : integer range 0 to 49 := 0;   -- 50 x 20ms tick = 1 s
    -- VGA outputs are gated on `sleeping` (blank + DPMS) via these internal nets.
    signal vga_hs_i, vga_vs_i : std_logic;
    signal vga_r_i, vga_g_i, vga_b_i : std_logic_vector(5 downto 0);
    -- LED display mode. DEBUG (the historical view) vs NORMAL (quiet board:
    -- only LED1 gives a rare short blink while asleep). normal_mode is forced
    -- '0' unless SUSPEND_EN, so a non-suspend build keeps the exact LED wiring.
    signal normal_mode    : std_logic;
    signal sleep_blink    : std_logic;
    signal blink_cnt      : std_logic_vector(25 downto 0) := (others => '0');
    signal led_dbg        : std_logic_vector(8 downto 0);   -- the historical LEDs
    signal led_normal     : std_logic_vector(8 downto 0);   -- quiet-mode LEDs
    -- suspend diagnostics (only meaningful/driven when SUSPEND_EN)
    signal sc_ramp        : std_logic_vector(2 downto 0);   -- inact ramp from suspend_ctrl
    signal idle_seen      : std_logic := '0';   -- sticky: cpu_idle ever asserted
    signal ever_slept     : std_logic := '0';   -- sticky: sleeping ever asserted
    signal act_stretch    : std_logic;          -- kbd_frame, stretched to be visible
    signal act_cnt        : std_logic_vector(23 downto 0) := (others => '0');
    signal led_diag       : std_logic_vector(8 downto 0);   -- diagnostic LEDs
    -- 20 ms (50 Hz) timer tick -- the reference VM calls its source a "20 msec
    -- interrupt source" (VM.h) and raises Ipt=1 from it.
    constant TICK_DIV : integer := CLK_HZ / 50; -- OS 50 Hz tick, clock-derived
    signal tick_cnt   : integer range 0 to TICK_DIV - 1 := 0;
    signal tick       : std_logic := '0';
    signal cpu_adr    : std_logic_vector(22 downto 0);
    signal cpu_dat_i, cpu_dat_o : std_logic_vector(31 downto 0);
    signal cpu_we, cpu_cyc, cpu_stb, cpu_ack, cpu_err, cpu_lock : std_logic;
    signal cpu_reset  : std_logic;

    -- I/O page decode
    signal sel_iopage, sel_disk, sel_dl11, sel_legacy : std_logic;
    signal io_ack     : std_logic := '0';
    signal io_rdata   : std_logic_vector(31 downto 0);
    signal cpu_stb_sdram : std_logic;

    -- Top of physical RAM the OS may use (first word that reads 0 / drops writes).
    --
    -- 0x7F0000 = 8323072 words = 31.75 MB. This is the largest 64K-word multiple
    -- that still sits below the 0x7FF000 I/O page (the top 4K of the 23-bit
    -- space), and memory_top() fills a 64K block before probing its top, so the
    -- boundary must be such a multiple. (Was 0x70000 = 1792 KB when the bus was
    -- 19-bit / 2 MB; the datapath is now 23-bit so the SDRAM's full 32 MB is
    -- reachable. The I/O page rode the top of the space up from 0x7F000 to
    -- 0x7FF000 automatically -- it is rg_io=0xFFFFF000 truncated to address_size.)
    --
    -- It used to be 512 KB, and that was NOT enough for the OS: past "Loaded OK"
    -- it printed "0080h" forever. The reference VM reproduces that EXACTLY when
    -- built with MemorySize = 128*K (512 KB) and clears completely at 1 MB and
    -- above -- so the board was faithful and merely starved of RAM. 1024 KB,
    -- 1536 KB, 1792 KB and 2048 KB were all verified clean in the VM.
    --
    -- How termination works: memory_top() sizes RAM by storing a value at a
    -- block top and reading it back, so the read above RAM_TOP must return
    -- something different. The out-of-range logic below returns 0 and drops the
    -- write, which is what stops the fill -- REQUIRED on hardware, where the
    -- SDRAM physically backs far more than this. (With the cache fully bypassed
    -- every read reaches that logic; DataCacheFM's mem_cacheable0 no longer
    -- matters, though it is kept in step with this constant in case the cache is
    -- ever restored.)
    constant RAM_TOP  : std_logic_vector(22 downto 0) := "11111110000000000000000"; -- 0x7F0000 = 31.75 MB
    signal sel_oor    : std_logic;                 -- CPU access at/above RAM_TOP
    signal oor_ack    : std_logic := '0';

    -- SDRAM controller bus (arbitrated CPU / disk DMA)
    signal sdc_adr    : std_logic_vector(22 downto 0);
    signal sdc_dat_i, sdc_dat_o : std_logic_vector(31 downto 0);
    signal sdc_we, sdc_stb, sdc_cyc, sdc_ack, sdc_ready : std_logic;
    signal granted_dma : std_logic := '0';

    -- disk controller
    signal dsk_rdata  : std_logic_vector(31 downto 0);
    signal dma_adr    : std_logic_vector(22 downto 0);
    signal dma_dat    : std_logic_vector(31 downto 0);
    signal dma_stb, dma_we, dma_ack : std_logic;
    signal boot_done, boot_fail, disk_busy : std_logic;
    signal disk_reset : std_logic;
    signal disk_stb, dl11_stb : std_logic;

    -- DL11
    signal dl11_rdata : std_logic_vector(31 downto 0);
    signal dl11_rx_irq, dl11_tx_irq : std_logic;
    signal con_byte   : std_logic_vector(7 downto 0);
    signal con_strobe : std_logic;
    signal con_rxd    : std_logic;
    signal kbd_ascii  : std_logic_vector(7 downto 0);
    signal kbd_stb    : std_logic;
    signal kbd_frame  : std_logic;                     -- valid PS/2 frame arrived
    signal kbd_seen   : std_logic := '0';              -- latched: a key DECODED
    signal frame_seen : std_logic := '0';              -- latched: a frame ARRIVED
    signal rx_ien_now : std_logic;                     -- OS enabled rx interrupt
    signal rbuf_rd    : std_logic;                     -- OS read the rx buffer
    signal rbuf_seen  : std_logic := '0';              -- latched
    signal vga_ch     : std_logic_vector(7 downto 0);
    signal vga_stb    : std_logic;
    signal pclk       : std_logic;                     -- 65 MHz pixel clock
    signal cdc_byte   : std_logic_vector(7 downto 0) := (others => '0');
    signal cdc_tog    : std_logic := '0';

    -- "Loaded OK" console matcher: definitive proof the OS booted, since there is
    -- no serial console to read on the board. Watches the DL11 TX byte stream.
    type byte_arr is array(natural range <>) of std_logic_vector(7 downto 0);
    constant LOK : byte_arr(0 to 8) :=      -- 'L','o','a','d','e','d',' ','O','K'
        (x"4C", x"6F", x"61", x"64", x"65", x"64", x"20", x"4F", x"4B");
    signal lok_pos  : integer range 0 to 8 := 0;
    signal lok_seen : std_logic := '0';
    -- diagnostic: count io2 DATA words drained in the CURRENT op (reset each CMD),
    -- freeze on the LEDs at the panic -> tells us where in op#2 the CPU derails.
    signal io2_drain   : std_logic_vector(10 downto 0) := (others => '0');  -- 0..1024
    signal panic_drain : std_logic_vector(10 downto 0) := (others => '0');
    signal io2_ops     : std_logic_vector(6 downto 0) := (others => '0');   -- io2 ops issued
    signal panic_ops   : std_logic_vector(6 downto 0) := (others => '0');

    -- ---------------------------------------------------------------- panic capture
    -- There is no serial console on this board (the DB9 is a dead end), so the
    -- only way to read the panic printer's register dump -- "G xxxxxxxx F
    -- xxxxxxxx PC xxxxxxxx -> xxxxxxxx", which it writes as ASCII bytes to
    -- 0x7F000 -- is to capture it in the FPGA and clock it out through the LEDs.
    -- The 8 DIP switches pick WHAT to show, so one flash yields the whole dump
    -- instead of one value per flash/power-cycle round trip.
    constant PBUF_N   : integer := 64;
    type pbuf_t is array(0 to PBUF_N - 1) of std_logic_vector(7 downto 0);
    signal pbuf       : pbuf_t := (others => (others => '0'));
    signal pbuf_wp    : integer range 0 to PBUF_N := 0;      -- PBUF_N = full/frozen
    signal pcount     : std_logic_vector(15 downto 0) := (others => '0');
    signal legacy_wr  : std_logic;                           -- 1-cycle strobe per 0x7F000 write
    signal dbg_view   : std_logic_vector(1 downto 0);
    signal dbg_idx    : integer range 0 to 63;
    signal dbg_byte   : std_logic_vector(7 downto 0);
    signal readout    : std_logic;                           -- show dump instead of status
    signal is_status  : std_logic;

    -- ---------------------------------------------- performance counters
    -- Measured at the SDRAM bus, deliberately: a cache HIT never reaches this
    -- bus, so every transaction here IS a miss. That makes the interesting
    -- numbers observable without threading dbg_* ports down through
    -- Kronos/DataStorage/DataCache -- and 86 stray unplaced dbg_* pins have
    -- already cost this port one debugging session.
    --
    -- The controller splits the HALFWORD address as col=(8:0), row=(21:9),
    -- so with our word address that is row = adr(18:8) and a row holds 256
    -- WORDS. Sequential code or arrays therefore stay in one row for a long
    -- run, which is exactly what the current auto-precharge policy throws away
    -- by closing the row after every single word.
    --
    -- perf_same answers the question that decides the whole optimisation plan:
    -- what fraction of misses go to the row we just closed? Everything is
    -- windowed over 4096 transactions so the "rate" is a shift, never a divide.
    signal perf_row    : std_logic_vector(10 downto 0) := (others => '1');
    signal perf_txn    : std_logic_vector(11 downto 0) := (others => '0');
    signal perf_same   : std_logic_vector(11 downto 0) := (others => '0');
    signal perf_busy   : std_logic_vector(19 downto 0) := (others => '0');
    signal perf_clk    : std_logic_vector(19 downto 0) := (others => '0');
    signal perf_txn_w  : std_logic_vector(15 downto 0) := (others => '0');
    -- latched results, each already scaled to one byte
    signal m_samerow   : std_logic_vector(7 downto 0) := (others => '0');
    signal m_avgcyc    : std_logic_vector(7 downto 0) := (others => '0');
    signal m_txnrate   : std_logic_vector(7 downto 0) := (others => '0');
    signal perf_sel    : std_logic;
    signal perf_ch     : std_logic_vector(7 downto 0);
    signal perf_stb    : std_logic;
    signal perf_en     : std_logic;   -- = not sw(7); a named signal, since GHDL
                                      -- rejects an operator expression as a port actual
    signal con_tx_idle : std_logic;
    signal perf_ack    : std_logic;
    signal con_stb    : std_logic;
    signal st_byte    : std_logic_vector(7 downto 0);   -- console self-test
    signal st_stb     : std_logic;
    signal con_reg    : std_logic_vector(1 downto 0);

    -- diag
    signal tx_blink   : std_logic_vector(20 downto 0) := (others => '0');
    signal diag_wrote : std_logic := '0';
    signal con_seen   : std_logic := '0';   -- latched: DL11 console emitted a byte
    signal io2_seen   : std_logic := '0';   -- latched: an io2 disk read was issued
    signal rd_lat_sel : std_logic_vector(3 downto 0);  -- SDRAM read_lat (auto-tuned)
    signal rl         : std_logic_vector(3 downto 0) := "0011";      -- current read_lat (3)
    signal tune_cnt   : std_logic_vector(24 downto 0) := (others => '0');  -- ~1.5 s / attempt
    signal tuned      : std_logic := '0';   -- latched: boot reached the console -> lock rl
    signal cpu_boot_rst : std_logic;        -- hold CPU in reset at the start of each attempt
begin
    sd_clk <= clk;   -- no-PLL: SDRAM clock straight from the oscillator

    process (clk)
    begin
        if rising_edge(clk) then
            prescaler <= prescaler + 1;
            if rst_n = '0' then reset <= '1'; reset_cnt <= 0;
            elsif reset_cnt = RESET_CYCLES then reset <= '0';
            else reset <= '1'; reset_cnt <= reset_cnt + 1; end if;
        end if;
    end process;

    -- SDRAM read-capture latency FIXED at 3 -- the value the working M2 stub
    -- proved on this exact board. (The auto-tune sweep showed the derail is NOT
    -- read-latency dependent: every value 2..9 still panicked to 0x7F000, because
    -- the real cause was the dual-port BlockRam read-during-write corruption, now
    -- bypassed in DataCacheFM. Holding read_lat fixed removes the periodic CPU
    -- reset so the bypass boot -- which is slower, every access hits SDRAM -- runs
    -- uninterrupted.) rl/tune_cnt/tuned/cpu_boot_rst kept declared for easy
    -- restore of the sweep if 3 turns out wrong with the bypass in place.
    -- ...but drive it from the GENERIC, not a hardwired constant. Hardwiring
    -- "0011" here silently overrode both the tb's `SD_READ_LAT => 4` and its
    -- `sw => "00000100"`, so every OneChipBook sim after that ran with the HARDWARE
    -- latency of 3. In sim the correct capture latency is 4, so every SDRAM read
    -- returned 0: the CPU read mem[0] as 0 instead of 144, executed zeros and
    -- died ~28 accesses after release. Hardware keeps 3 via the generic default.
    -- SDRAM read latency, LIVE on the DIP switches so it can be swept without
    -- a reflash. This is the one parameter Quartus cannot check: it models the
    -- FPGA's internal paths, not the round trip out to the SDRAM, through the
    -- 33R series resistor and back. read_lat=3 was tuned at a 46.6ns period;
    -- at 37.2ns (26.85 MHz) that round trip is a much bigger fraction of a
    -- cycle, and marginal capture returns wrong data -- which lands wrong TAGS
    -- in the cache, so lookups spuriously miss and refetch. That is survivable
    -- (the refetch corrects it) but slow, and it matches the measured symptom
    -- exactly: cycles/miss unchanged at ~12.7, but 2.6x MORE misses per pass.
    --
    -- Switches are ACTIVE-LOW (OFF = 1), SW4 = LSB .. SW7 = MSB. All four ON
    -- reads as 0, which is not a legal latency, so that position falls back to
    -- the SD_READ_LAT default -- i.e. the rest position is the known-good value
    -- and any sweep is a deliberate act.
    -- FIXED, not switch-driven. Driving this from the DIP pins made the machine
    -- SLOWER (10845 -> 7400 drystones at the same nominal latency): `sw` is an
    -- unsynchronised asynchronous input with no input-delay constraint, so those
    -- paths were never timing-checked, and they fed the SDRAM controller's
    -- read-capture logic directly. The sweep it enabled did answer the question
    -- -- only 3 boots at all -- but it must not stay in a timing-critical path.
    rd_lat_sel <= conv_std_logic_vector(SD_READ_LAT, 4);
    cpu_reset  <= reset or not boot_done;

    cpu : entity work.Kronos
        generic map (address_size => 23)
        port map (rs_mode => "01", halt => cpu_halt, halted => cpu_halted, idle => cpu_idle,
            ireq => cpu_ireq, clk_i => clk, rst_i => cpu_reset,
            adr_o => cpu_adr, dat_i => cpu_dat_i, dat_o => cpu_dat_o,
            we_o => cpu_we, stb_o => cpu_stb, cyc_o => cpu_cyc,
            ack_i => cpu_ack, err_i => cpu_err, lock_o => cpu_lock);

    -- ------------------------------------------------ interrupts
    -- Console input is INTERRUPT-driven in this OS, which is why the login
    -- prompt ignored the keyboard: cpu_ireq was tied to zero, so a decoded
    -- keystroke set rx_avail and nothing ever told the CPU.
    --
    -- ireq is a 4-bit mask of interrupt LINES (not a vector), each gated by
    -- int_en, which DataStorage drives from the CPU's own mask register -- the
    -- same role the VM's M register plays (M bit 0 = SIO, bit 1 = timer). So a
    -- line can only ever fire once the OS has asked for it, which is what makes
    -- enabling these safe. Decode maps line -> microcode entry 0x6E0 + 2*line:
    --   line 0 -> trap 0x0C  console input   (retargeted from the 5.0 "RS232"
    --                                         0x10 to the Kronos-3 number this
    --                                         OS expects -- see src/Kronos.mkas)
    --   line 1 -> trap 0x01  timer           (already the right number)
    -- Line 1 is LATCHED in Decode and cleared on service, so it wants a pulse;
    -- line 0 is level and clears itself when the OS reads RBUF.
    -- Console OUTPUT (VM trap 0x0D) is deliberately not wired: transmit already
    -- works without it, and it would need a second line the OS does not enable.
    cpu_ireq(0) <= dl11_rx_irq;
    -- Mask the 50 Hz timer while asleep so nothing but a key ends the sleep and
    -- the interrupt latch does not accumulate ticks. `sleeping` is a constant
    -- '0' when SUSPEND_EN=false, so this is exactly `tick` in a normal build.
    cpu_ireq(1) <= tick and not sleeping;
    cpu_ireq(2) <= '0';
    cpu_ireq(3) <= '0';

    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                tick_cnt <= 0; tick <= '0';
            elsif tick_cnt = TICK_DIV - 1 then
                tick_cnt <= 0; tick <= '1';
            else
                tick_cnt <= tick_cnt + 1; tick <= '0';
            end if;
        end if;
    end process;

    -- 1 Hz tick for the sleep timer: 50 of the 50 Hz ticks. Seconds, not clocks,
    -- so a 20-minute timeout fits (see suspend_ctrl / the timeout register).
    process (clk)
    begin
        if rising_edge(clk) then
            sec_tick <= '0';
            if reset = '1' then
                sec_cnt <= 0;
            elsif tick = '1' then
                if sec_cnt = 49 then sec_cnt <= 0; sec_tick <= '1';
                else sec_cnt <= sec_cnt + 1; end if;
            end if;
        end if;
    end process;

    -- Sleep-timeout register: power-on default SUSPEND_DEFAULT_SEC, then whatever
    -- the OS writes via out(0x20, seconds). One write per I/O transaction.
    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                timeout_sec <= conv_std_logic_vector(SUSPEND_DEFAULT_SEC, 16);
            elsif sel_timeout = '1' and cpu_cyc = '1' and cpu_stb = '1'
                  and cpu_we = '1' and io_ack = '0' then
                timeout_sec <= cpu_dat_o(15 downto 0);
            end if;
        end if;
    end process;

    -- ------------------------------------------------ I/O page decode
    sel_iopage <= '1' when cpu_adr(22 downto 12) = "11111111111" else '0';
    sel_disk    <= '1' when sel_iopage = '1' and cpu_adr(11 downto 3) = "000000010" else '0';
    sel_dl11    <= '1' when sel_iopage = '1' and cpu_adr(11 downto 2) = "1111101110" else '0';
    sel_legacy  <= '1' when sel_iopage = '1' and cpu_adr(11 downto 0) = X"000" else '0';
    -- sleep-timeout register (free I/O offset, clear of disk 0x10-0x17 / DL11 /
    -- legacy 0x000). out(0x20, s) writes it; io0(0x20) reads it back.
    sel_timeout  <= '1' when sel_iopage = '1' and cpu_adr(11 downto 0) = X"020" else '0';
    -- sleep-NOW register: any write (out(0x21, x)) puts the machine to sleep
    -- immediately -- the `sleep` command.
    sel_sleepnow <= '1' when sel_iopage = '1' and cpu_adr(11 downto 0) = X"021" else '0';

    -- Out-of-range (>= RAM_TOP, not I/O): not a real SDRAM access; reads 0,
    -- writes dropped, one-cycle ack -- like unmapped memory. This is what lets
    -- memory_top() find the top of RAM on hardware (real SDRAM backs the full 2MB).
    sel_oor <= '1' when cpu_cyc = '1' and cpu_stb = '1' and sel_iopage = '0'
                        and cpu_adr >= RAM_TOP else '0';
    cpu_stb_sdram <= cpu_cyc and cpu_stb and not sel_iopage and not sel_oor;

    -- one ack (and one register strobe) per I/O-page transaction
    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                io_ack <= '0'; oor_ack <= '0'; diag_wrote <= '0';
            else
                io_ack  <= sel_iopage and cpu_cyc and cpu_stb and not io_ack;
                oor_ack <= sel_oor    and cpu_cyc and cpu_stb and not oor_ack;
                if sel_legacy = '1' and cpu_stb = '1' and cpu_we = '1' then
                    diag_wrote <= '1';
                end if;
            end if;
        end if;
    end process;

    io_rdata <= dsk_rdata          when sel_disk = '1' else
                dl11_rdata         when sel_dl11 = '1' else
                (X"0000" & timeout_sec) when sel_timeout = '1' else
                X"00000001"        when sel_legacy = '1' else
                (others => '0');

    cpu_dat_i <= (others => '0') when sel_oor = '1' else
                 io_rdata        when sel_iopage = '1' else
                 sdc_dat_o;
    cpu_ack   <= oor_ack when sel_oor = '1' else
                 io_ack  when sel_iopage = '1' else
                 (sdc_ack and not granted_dma);
    -- No bus error: memory_top detects the top of RAM via the non-cacheable
    -- out-of-range READ returning 0 (above), whose NEQ mismatch stops the fill --
    -- the reference VM's primary mechanism. Driving err instead raises a HARDWARE
    -- memory-fault trap, but during early boot interrupt 3 is masked and its
    -- handler is not set up, so the trap derails the CPU rather than letting
    -- memory_top return.
    cpu_err <= '0';

    -- ------------------------------------------------ SDRAM arbiter
    -- Disk DMA gets the bus only between CPU transactions; one word per grant.
    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                granted_dma <= '0';
            elsif granted_dma = '1' then
                if sdc_ack = '1' then granted_dma <= '0'; end if;
            elsif dma_stb = '1' and cpu_stb_sdram = '0' then
                granted_dma <= '1';
            end if;
        end if;
    end process;

    sdc_adr   <= dma_adr when granted_dma = '1' else cpu_adr;
    sdc_dat_i <= dma_dat when granted_dma = '1' else cpu_dat_o;
    sdc_we    <= dma_we  when granted_dma = '1' else cpu_we;
    sdc_stb   <= dma_stb when granted_dma = '1' else cpu_stb_sdram;
    sdc_cyc   <= dma_stb when granted_dma = '1' else cpu_stb_sdram;
    dma_ack   <= sdc_ack and granted_dma;

    sdc : entity work.sdram_controller
        generic map (ADDR_BITS => 23, T_INIT => SD_T_INIT, T_REFI => SD_T_REFI)
        port map (clk => clk, reset => reset,
            wb_adr => sdc_adr, wb_dat_i => sdc_dat_i, wb_dat_o => sdc_dat_o,
            wb_we => sdc_we, wb_stb => sdc_stb, wb_cyc => sdc_cyc, wb_ack => sdc_ack, ready => sdc_ready,
            read_lat => rd_lat_sel,
            suspend => sdc_suspend, suspended => sdc_suspended,
            sd_a => sd_a, sd_ba => sd_ba, sd_dq => sd_dq, sd_cke => sd_cke,
            sd_cs_n => sd_cs_n, sd_ras_n => sd_ras_n, sd_cas_n => sd_cas_n, sd_we_n => sd_we_n,
            sd_dqml => sd_dqml, sd_dqmh => sd_dqmh);

    -- ------------------------------------------------ disk controller
    -- held in reset until the SDRAM can accept the boot image
    disk_reset <= reset or not sdc_ready;
    disk_stb   <= sel_disk and cpu_cyc and cpu_stb and not io_ack;
    dl11_stb   <= sel_dl11 and cpu_cyc and cpu_stb and not io_ack;
    -- one strobe per 0x7F000 write (io_ack rises the cycle after the request,
    -- so the "and not io_ack" term makes this exactly one cycle wide)
    legacy_wr  <= sel_legacy and cpu_cyc and cpu_stb and cpu_we and not io_ack;
    -- one-cycle pulse on a write to the sleep-NOW register -> suspend_ctrl force
    force_sleep <= sel_sleepnow and cpu_cyc and cpu_stb and cpu_we and not io_ack;

    -- BOOT_SECTORS = 8: the VM's ReadBooter reads 4096 bytes = 8 sectors = 1024
    -- words of boot code (vm/int/src/Kronos3vm.cpp), and zero-inits memory. The
    -- disk's words 512-1023 are zero, so loading 8 sectors zero-inits that region
    -- to match the VM (loading only 4 left it as SDRAM garbage -> boot trapped).
    disk : entity work.sd_disk_controller
        generic map (CLK_DIV => SD_CLK_DIV, BOOT_SECTORS => 8, DISK_4KB => DISK_4KB)
        port map (clk => clk, reset => disk_reset,
            reg_adr => cpu_adr(2 downto 0), reg_dat_i => cpu_dat_o, reg_dat_o => dsk_rdata,
            reg_stb => disk_stb, reg_we => cpu_we,
            dma_adr => dma_adr, dma_dat => dma_dat, dma_stb => dma_stb,
            dma_we => dma_we, dma_ack => dma_ack,
            boot_done => boot_done, boot_fail => boot_fail, busy_o => disk_busy,
            sd_clk => sd_sclk, sd_cs => sd_cs, sd_mosi => sd_mosi, sd_miso => sd_miso);

    -- ------------------------------------------------ DL11 console + UART
    -- The real Excelsior boot chain writes its console via io1 (out) to the
    -- DL11 at 0xFB8-0xFBB (confirmed in the reference-VM trace), NOT a
    -- memory-mapped console at 0x7F000. 0x7F000 is just plain RAM the booter's
    -- memory_top() probe fills -- routing it to the UART produced phantom
    -- "trap dump" garbage. So the DL11 is the ONLY console.
    con_stb    <= dl11_stb;
    con_reg    <= cpu_adr(1 downto 0);

    console : entity work.dl11_console
        generic map (BAUD_DIV => BAUD_DIV)
        port map (clk => clk, reset => reset,
            reg_adr => con_reg, reg_dat_i => cpu_dat_o, reg_dat_o => dl11_rdata,
            reg_stb => con_stb, reg_we => cpu_we,
            rx_irq => dl11_rx_irq, tx_irq => dl11_tx_irq,
            uart_txd => uart_txd, uart_rxd => con_rxd,
            rx_inj => kbd_ascii, rx_inj_stb => kbd_stb,
            tx_inj => perf_ch, tx_inj_stb => perf_stb, tx_idle => con_tx_idle,
            tx_inj_ack => perf_ack,
            tx_byte => con_byte, tx_strobe => con_strobe,
            dbg_rx_ien => rx_ien_now, dbg_rbuf_rd => rbuf_rd);

    -- Prints "PERF SR=xx CYC=xx RT=xx" to the console every ~2s. The counters
    -- are unreadable on the LEDs -- they re-latch every ~2ms, so the low bits
    -- are a blur -- and the DIP indices collided with SW1/SW2, which belong to
    -- the board's VGA line-scan generator. As text they land in a host UART
    -- capture and can be diffed between builds.
    -- OFF BY DEFAULT. The report is a debug aid, so the resting console must be
    -- clean: with active-low switches (OFF = logic 1) the natural "all DIPs off"
    -- state has to be the quiet one. `enable => not sw(7)` gives exactly that --
    --   SW8 OFF (rest) -> sw(7)='1' -> enable='0' -> QUIET  (the default)
    --   SW8 ON         -> sw(7)='0' -> enable='1' -> reporting (opt-in profiling)
    -- The previous wiring (enable => sw(7)) reported by default and had to be
    -- switched OFF, which is backwards for scaffolding and is what polluted an
    -- ordinary session. ~4s between reports keeps it unobtrusive when it is on.
    perf_en <= not sw(7);
    perf_rep : entity work.perf_report
        generic map (PERIOD => CLK_HZ * 4)
        port map (clk => clk, reset => reset, enable => perf_en,
                  samerow => m_samerow, avgcyc => m_avgcyc, txnrate => m_txnrate,
                  tx_ready => con_tx_idle, tx_ack => perf_ack, cpu_busy => con_stb,
                  tx_byte => perf_ch, tx_stb => perf_stb);

    -- ------------------------------------------------ PS/2 keyboard (M5)
    -- Console INPUT. After "Loaded OK" the OS waits on the console, so this is
    -- what makes the machine interactive: decoded keys are injected into the
    -- DL11 receiver, which is exactly where the OS already looks for them.
    kbd : entity work.ps2_keyboard
        port map (clk => clk, reset => reset,
                  ps2_clk => ps2_clk, ps2_data => ps2_data,
                  ascii => kbd_ascii, stb => kbd_stb, frame_stb => kbd_frame);

    -- ------------------------------------------------ suspend to RAM (sleep)
    -- "Activity" = a PS/2 frame (key press OR release) OR the OS emitting a
    -- console byte (con_strobe). Either resets the inactivity timer while awake,
    -- so the machine stays awake not only while you type but while any program
    -- is producing output -- a compile, kmm, ls -- which is the compute-safety
    -- guard: with the cpu_idle gate gone (this OS busy-polls), a running task
    -- would otherwise look like inactivity and sleep. Only a PS/2 frame can WAKE
    -- it from sleep, because the CPU is halted then and con_strobe cannot fire.
    -- (A truly silent long compute -- no output at all for the timeout -- would
    -- still sleep; a keypress resumes it. If a task needs that, add an SDRAM-
    -- activity term here.) The wake key still reaches the OS via kbd_stb above.
    susp_activity <= kbd_frame or con_strobe;
    -- Hold the sequencer disabled until the OS has printed "Loaded OK": with the
    -- idle gate off (below) it would otherwise count the boot's long, keyless
    -- disk load as inactivity and sleep mid-boot. lok_seen latches on that
    -- string and stays set, so sleep is armed only once the OS is up.
    sc_reset <= reset or not lok_seen;
    gen_suspend : if SUSPEND_EN generate
        sc : entity work.suspend_ctrl
            -- IDLE_GATED=false: this OS busy-polls and never executes IDLE, so
            -- cpu_idle never asserts; sleep on inactivity alone. The timeout is
            -- the runtime register, counted in seconds via sec_tick.
            generic map (IDLE_GATED => false)
            port map (clk => clk, reset => sc_reset,
                      cpu_idle => cpu_idle, activity => susp_activity,
                      sleep_now => force_sleep,
                      sec_tick => sec_tick, timeout_sec => timeout_sec,
                      sdc_suspended => sdc_suspended,
                      cpu_halt => cpu_halt, sdc_suspend => sdc_suspend,
                      sleeping => sleeping, dbg_ramp => sc_ramp);

        -- Diagnostic latches/stretch (free even when SUSPEND_DIAG=false; the
        -- led_diag mux below only shows them when it is true). idle_seen answers
        -- the key question -- does the OS ever execute the IDLE instruction? --
        -- and act_stretch makes a brief kbd_frame visible so we can see whether a
        -- spurious PS/2 frame is resetting the inactivity timer.
        diag : process (clk)
        begin
            if rising_edge(clk) then
                if reset = '1' then
                    idle_seen <= '0'; ever_slept <= '0'; act_cnt <= (others => '0');
                else
                    if cpu_idle = '1' then idle_seen  <= '1'; end if;
                    if sleeping  = '1' then ever_slept <= '1'; end if;
                    if susp_activity = '1' then act_cnt <= (others => '1');   -- ~0.65 s stretch
                    elsif act_cnt /= 0     then act_cnt <= act_cnt - 1; end if;
                end if;
            end if;
        end process;
        act_stretch <= '0' when act_cnt = 0 else '1';

        -- SW4 picks the LED display. Active-low DIPs (OFF = '1'), so at rest the
        -- board is QUIET (normal_mode = '1'); flip SW4 ON for the debug view.
        --
        -- NOT SW3: on this board SW3 (sw(2), PIN_55) has an undocumented effect
        -- on the VGA line-scan generator -- driving it corrupts the on-screen
        -- font (partial characters), exactly as SW1/SW2 do. Only SW4..SW7 are
        -- safe live inputs (they carried the read_lat sweep with the VGA running
        -- and never disturbed it). SW4 = sw(3) = dbg_idx bit3, so the only cost is
        -- that the post-panic dump can't reach its bit3-set indices in debug
        -- mode (e.g. perf index 40); every counter and the boot-status LEDs are
        -- unaffected.
        normal_mode <= sw(3);

        -- Sleep indicator: a rare, brief flash on LED1 while asleep. blink_cnt
        -- wraps about every 3 s at 21.477 MHz; the top four bits are set for the
        -- last 1/16 of that, so LED1 pulses ~0.2 s roughly every 3 s.
        blink : process (clk)
        begin
            if rising_edge(clk) then blink_cnt <= blink_cnt + 1; end if;
        end process;
        sleep_blink <= '1' when (sleeping = '1' and blink_cnt(25 downto 22) = "1111")
                       else '0';
    end generate;
    gen_no_suspend : if not SUSPEND_EN generate
        cpu_halt    <= '0';
        sdc_suspend <= '0';
        sleeping    <= '0';
        normal_mode <= '0';        -- always the historical LEDs
        sleep_blink <= '0';
        sc_ramp     <= "000";
        idle_seen   <= '0';
        ever_slept  <= '0';
        act_stretch <= '0';
    end generate;

    -- Keyboard diagnostics. These answer the one question the screen cannot:
    -- is the keyboard talking to us at all? frame_seen lights if ANY well-formed
    -- PS/2 frame arrives (even a key release or an unmapped key), kbd_seen only
    -- if one decoded to ASCII. Dark frame_seen => nothing is reaching the pins
    -- (wrong port / FN+4 / wiring); frame_seen lit but no echo => the keyboard
    -- works and the problem is downstream, in the OS-facing path.
    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                kbd_seen <= '0'; frame_seen <= '0';
            else
                if kbd_stb   = '1' then kbd_seen   <= '1'; end if;
                if kbd_frame = '1' then frame_seen <= '1'; end if;
                if rbuf_rd   = '1' then rbuf_seen  <= '1'; end if;
            end if;
        end if;
    end process;

    -- sw(0) = LOCAL ECHO: mirror decoded keys straight to the VGA console,
    -- bypassing the CPU entirely. If characters appear with this on, the whole
    -- PS/2 -> ASCII chain is proven good in hardware and the fault is purely in
    -- getting them to the OS. Additive, so OS output still shows either way;
    -- OS output wins a same-cycle tie (the keyboard will simply repeat).
    -- sw(1) = CONSOLE SELF-TEST, sw(2) = its ESC J variant.
    -- The editor blanks the screen on the first scroll on hardware, but the
    -- console fed the REAL captured OS stream renders every row correctly in
    -- simulation. This drives the console from a canned sequence instead of
    -- the OS so the board can say which of the two is lying. See
    -- src/console_selftest.vhdl for how to read the result.
    -- Held disabled: its enable used to sit on sw(1) = SW2, which belongs to the
    -- board's VGA line-scan generator, so an innocent switch position replaced
    -- the live console with a test pattern. Kept instantiated (not deleted) so
    -- the self-test is one edit away if the VGA path ever needs proving again.
    selftest : entity work.console_selftest
        port map (clk => clk, reset => reset,
                  enable => '0', escj => '0',
                  byte => st_byte, stb => st_stb);

    -- SW1 and SW2 ARE NOT OURS TO USE. The board wires them to its own VGA
    -- line-scan generator (see ONECHIPBOOK_PINOUT.md), so SW1 must stay OFF or
    -- the font renders with half the pixels dark. The switches are ACTIVE-LOW --
    -- OFF = logic 1 -- which meant the mandatory SW1=OFF forced sw(0)='1' and
    -- turned LOCAL ECHO permanently on, so every keypress appeared TWICE on
    -- screen: once from us, once from the OS's own echo.
    --
    -- Both features hanging off these switches were console bring-up aids and
    -- are long obsolete: local echo proved the PS/2 chain on silicon, and the
    -- self-test proved the VGA path before the OS could drive it. The console
    -- has been driving a real interactive session for days. Removing them frees
    -- SW1/SW2 completely and makes the duplication impossible rather than
    -- switch-dependent.
    vga_stb <= con_strobe;
    vga_ch  <= con_byte;

    -- '1' is the UART idle level, so the receiver never sees a start bit
    con_rxd <= uart_rxd when UART_RX_EN else '1';

    -- ------------------------------------------------ VGA text console
    -- Renders the SAME console byte stream the DL11 sends to the (unusable)
    -- DB9 UART, so the OS's output is finally readable on a monitor without
    -- touching the OS. Costs no memory bandwidth: it has its own text RAM and
    -- font ROM and never touches SDRAM. The 21.477 MHz system clock IS the
    -- pixel clock -- no PLL, so the tuned SDRAM timing is left alone.
    -- 1024x768@60 needs a 65 MHz pixel clock, so the console lives in its own
    -- clock domain. The PLL synthesises the video clock ONLY -- clk (and with
    -- it the SDRAM interface) is untouched.
    vpll : entity work.video_pll
        generic map (USE_PLL => USE_PLL)
        port map (inclk => clk_ref, pclk => pclk, locked => open);

    -- Cross the console byte stream into the pixel domain: flip a toggle once
    -- per byte and let the console synchronise it. The data is stable for
    -- thousands of cycles either side (bytes are ~174 us apart at 57600 baud),
    -- so a toggle handshake is sufficient and no FIFO is needed.
    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                cdc_tog <= '0';
            elsif vga_stb = '1' then
                cdc_byte <= vga_ch;
                cdc_tog  <= not cdc_tog;
            end if;
        end if;
    end process;

    vga : entity work.vga_console
        port map (pclk => pclk, reset => reset,
                  ch => cdc_byte, ch_tog => cdc_tog,
                  hsync => vga_hs_i, vsync => vga_vs_i,
                  r => vga_r_i, g => vga_g_i, b => vga_b_i);

    -- Screen off while asleep: blank the pixels AND drop hsync/vsync so the
    -- monitor loses signal and enters power-save (DPMS off). `sleeping` is in the
    -- system-clock domain and the VGA runs on pclk, but this is just a slow,
    -- glitch-tolerant blank enable (stable for seconds), so no CDC is needed. The
    -- console's own text RAM is untouched, so the frame is intact again the
    -- instant it wakes (the monitor takes ~1 s to re-lock sync). When SUSPEND_EN
    -- is false, sleeping is a constant '0' and these are exactly the raw outputs.
    vga_hsync <= vga_hs_i when sleeping = '0' else '0';
    vga_vsync <= vga_vs_i when sleeping = '0' else '0';
    vga_r     <= vga_r_i  when sleeping = '0' else (others => '0');
    vga_g     <= vga_g_i  when sleeping = '0' else (others => '0');
    vga_b     <= vga_b_i  when sleeping = '0' else (others => '0');

    -- stretch UART TX activity so it is visible on a LED; and LATCH the two
    -- "the OS actually ran" milestones (they only ever happen on a correct boot,
    -- never on the old word-308 derail, which panics to 0x7F000 instead):
    --   con_seen -- the DL11 console emitted at least one byte ("512KB memory"..)
    --   io2_seen -- the booter issued at least one io2 disk read (loading the OS)
    process (clk)
    begin
        if rising_edge(clk) then
            if con_strobe = '1' then tx_blink <= (others => '1');
            elsif tx_blink /= 0 then tx_blink <= tx_blink - 1; end if;
            if reset = '1' then
                con_seen <= '0'; io2_seen <= '0';
            else
                if con_strobe = '1' then con_seen <= '1'; end if;
                if sel_disk = '1' and cpu_stb = '1' and cpu_we = '1' then io2_seen <= '1'; end if;
            end if;
        end if;
    end process;

    -- match "Loaded OK" in the console byte stream -> latch lok_seen (LED9).
    -- Naive restart-on-mismatch FSM; the string has no self-overlap so it's exact.
    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                lok_pos <= 0; lok_seen <= '0';
            elsif con_strobe = '1' then
                if con_byte = LOK(lok_pos) then
                    if lok_pos = 8 then lok_seen <= '1'; lok_pos <= 0;
                    else lok_pos <= lok_pos + 1; end if;
                elsif con_byte = LOK(0) then
                    lok_pos <= 1;
                else
                    lok_pos <= 0;
                end if;
            end if;
        end if;
    end process;

    -- io2 DATA-word drain counter for the CURRENT op: reset on each CMD write
    -- (offset 3), +1 per DATA read (offset 7). Frozen at the panic so the LEDs
    -- report how many of op#2's ~1024 words had transferred when the CPU derailed
    -- (near 1024 = it finished the transfer and died in PROCESSING; well under =
    -- it died mid-transfer).
    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                io2_drain <= (others => '0'); panic_drain <= (others => '0');
                io2_ops   <= (others => '0'); panic_ops   <= (others => '0');
            else
                if disk_stb = '1' and cpu_we = '1' and cpu_adr(2 downto 0) = "011" then
                    io2_drain <= (others => '0');                       -- new op -> reset
                    if io2_ops /= 127 then io2_ops <= io2_ops + 1; end if;
                elsif disk_stb = '1' and cpu_we = '0' and cpu_adr(2 downto 0) = "111" then
                    io2_drain <= io2_drain + 1;                         -- one DATA word drained
                end if;
                if diag_wrote = '0' then                               -- freeze at panic
                    panic_drain <= io2_drain;
                    panic_ops   <= io2_ops;
                end if;
            end if;
        end if;
    end process;

    -- capture the first PBUF_N bytes the panic printer writes to 0x7F000, then
    -- FREEZE (it loops forever, so freezing keeps the first, cleanest dump).
    -- pcount keeps counting: 1 means a lone write, not a printed dump at all.
    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                pbuf_wp <= 0; pcount <= (others => '0');
            elsif legacy_wr = '1' then
                if pbuf_wp /= PBUF_N then
                    pbuf(pbuf_wp) <= cpu_dat_o(7 downto 0);
                    pbuf_wp <= pbuf_wp + 1;
                end if;
                if pcount /= X"FFFF" then pcount <= pcount + 1; end if;
            end if;
        end if;
    end process;

    -- ------------------------------------------------ performance counters
    -- CPU traffic only: DMA grants are excluded, so this measures the cache's
    -- miss stream and not the disk's.
    perf : process (clk)
        variable txn  : boolean;
        variable same : boolean;
    begin
        if rising_edge(clk) then
            if reset = '1' then
                perf_txn <= (others => '0'); perf_same <= (others => '0');
                perf_busy <= (others => '0'); perf_clk <= (others => '0');
                perf_txn_w <= (others => '0'); perf_row <= (others => '1');
            else
                -- one transaction = one accepted CPU word on the SDRAM bus
                txn  := (cpu_stb_sdram = '1' and sdc_ack = '1' and granted_dma = '0');
                same := (cpu_adr(18 downto 8) = perf_row);

                -- busy = cycles the CPU spends waiting on SDRAM. Divided by the
                -- transaction count this is the average miss cost in cycles.
                if cpu_stb_sdram = '1' and granted_dma = '0' then
                    perf_busy <= perf_busy + 1;
                end if;

                if txn then
                    perf_row <= cpu_adr(18 downto 8);
                    if same then perf_same <= perf_same + 1; end if;

                    if perf_txn = X"FFF" then          -- window of 4096 closes
                        -- same-row fraction, scaled to 0..255: same*256/4096
                        m_samerow <= perf_same(11 downto 4);
                        -- average cycles per miss in 1/16ths: busy*16/4096.
                        -- SATURATE rather than wrap: an average of 20 cycles
                        -- would otherwise read as 64 and look BETTER than 12.
                        if perf_busy(19 downto 16) /= "0000" then
                            m_avgcyc <= X"FF";
                        else
                            m_avgcyc <= perf_busy(15 downto 8);
                        end if;
                        perf_txn  <= (others => '0');
                        perf_same <= (others => '0');
                        perf_busy <= (others => '0');
                    else
                        perf_txn <= perf_txn + 1;
                    end if;
                end if;

                -- how OFTEN we miss: transactions per 2^20 clocks (~49ms here)
                if perf_clk = X"FFFFF" then
                    m_txnrate  <= perf_txn_w(15 downto 8);   -- txns/1M clk, /256
                    perf_clk   <= (others => '0');
                    perf_txn_w <= (others => '0');
                else
                    perf_clk <= perf_clk + 1;
                    if txn and perf_txn_w /= X"FFFF" then
                        perf_txn_w <= perf_txn_w + 1;
                    end if;
                end if;
            end if;
        end if;
    end process;

    -- ------------------------------------------------ DIP-selected LED readout
    -- sw(7:6) = view, sw(5:0) = index. led(7:0) = the selected byte.
    --   00  live status      (default -- the normal at-a-glance boot state)
    --   01  panic dump       byte sw(5:0) of the 0x7F000 ASCII capture
    --   10  counters         0/1 = 0x7F000 write count lo/hi, 2 = io2 op count
    --                        at panic, 3/4 = io2 drain count at panic lo/hi
    --   11  switch echo      led(5:0) = sw(5:0) -- confirms the DIPs read as
    --                        expected (polarity/wiring sanity check)
    -- If the switches turn out to be inverted, sweeping all four view codes
    -- still finds the echo view, which then tells you the polarity.
    dbg_view <= sw(7 downto 6);
    dbg_idx  <= conv_integer(sw(5 downto 0));

    dbg_byte <= pbuf(dbg_idx)                       when dbg_view = "01" else
                pcount(7 downto 0)                  when dbg_view = "10" and dbg_idx = 0 else
                pcount(15 downto 8)                 when dbg_view = "10" and dbg_idx = 1 else
                "0" & panic_ops                     when dbg_view = "10" and dbg_idx = 2 else
                panic_drain(7 downto 0)             when dbg_view = "10" and dbg_idx = 3 else
                "00000" & panic_drain(10 downto 8)  when dbg_view = "10" and dbg_idx = 4 else
                -- Performance counters, live (no panic required).
                -- Indices 32/40/48 use ONLY sw(5:3) = SW6/SW5/SW4, deliberately:
                -- sw(0)=SW1 is local echo and sw(1)=SW2 is the console
                -- SELF-TEST, which replaces the console with a test pattern.
                -- The obvious indices 33 and 34 would have required exactly
                -- those two switches and wrecked the display being measured.
                m_samerow                           when dbg_view = "10" and dbg_idx = 32 else
                m_avgcyc                            when dbg_view = "10" and dbg_idx = 40 else
                m_txnrate                           when dbg_view = "10" and dbg_idx = 48 else
                "00" & sw(5 downto 0)               when dbg_view = "11" else
                (others => '0');

    -- The DIP readout is ENABLED ONLY ONCE THE CPU HAS PANICKED. Until then the
    -- LEDs always show live status whatever the switches happen to be set to --
    -- so a good boot can never be misread as a broken one just because a DIP was
    -- left in some position, and the switches only start mattering when there is
    -- actually a dump to read out. led(8) = the panic flag in every view.
    -- ...EXCEPT the performance counters, which are only interesting while the
    -- machine is running normally, so view 10 / index >= 32 reads out without a
    -- panic. Everything else keeps the original rule, so a good boot still
    -- cannot be misread as a broken one because a DIP was left somewhere.
    perf_sel <= '1' when dbg_view = "10" and dbg_idx >= 32 else '0';
    readout <= (diag_wrote or perf_sel) and not is_status;
    is_status <= '1' when dbg_view = "00" else '0';

    -- The historical "debug" LED view (led_dbg). led(0)=LED1 is a free-running
    -- heartbeat; led(1..7)=LED2..LED8 latch boot/status milestones; led(8)=LED9
    -- is the panic flag. In readout='1' they instead show the DIP-selected dump.
    led_dbg(0) <= dbg_byte(0) when readout = '1' else prescaler(23);   -- heartbeat
    led_dbg(1) <= dbg_byte(1) when readout = '1' else sdc_ready;
    led_dbg(2) <= dbg_byte(2) when readout = '1' else boot_done;
    led_dbg(3) <= dbg_byte(3) when readout = '1' else rbuf_seen;   -- OS CONSUMED a key
    led_dbg(4) <= dbg_byte(4) when readout = '1' else con_seen;
    led_dbg(5) <= dbg_byte(5) when readout = '1' else kbd_seen;    -- a key DECODED
    led_dbg(6) <= dbg_byte(6) when readout = '1' else rx_ien_now;  -- OS wants IRQ input
    led_dbg(7) <= dbg_byte(7) when readout = '1' else lok_seen;        -- *** Loaded OK ***
    led_dbg(8) <= diag_wrote;                                          -- panic

    -- The "normal" quiet view: only LED1 lives, giving a rare short blink while
    -- the machine is asleep and off otherwise (the board's own power LED already
    -- says "on"). LED9 keeps the panic flag -- a crash should never go dark.
    led_normal <= (0 => sleep_blink, 8 => diag_wrote, others => '0');

    -- The DIAGNOSTIC view (SUSPEND_DIAG=true, normal mode). Reads the suspend
    -- machinery straight off the LEDs on a UART-less board:
    --   LED1 sleep_blink   -- blinks iff it actually sleeps
    --   LED2 idle_seen     -- STICKY: has the CPU ever executed IDLE? (the key
    --                         question -- dark here means the OS never idles /
    --                         cpu_idle never reaches us, so sleep can't arm)
    --   LED3 cpu_idle      -- LIVE: is the CPU in IDLE right now
    --   LED4 act_stretch   -- a PS/2 frame fired (stretched ~0.65 s). If this
    --                         flickers while you are NOT typing, spurious frames
    --                         are resetting the inactivity timer
    --   LED5/6/7 ramp      -- inactivity timer past 1/4, 1/2, 3/4 of ~3 s: should
    --                         climb steadily when idle & untouched, then sleep
    --   LED8 ever_slept    -- STICKY: did it ever enter sleep
    --   LED9 panic
    led_diag <= (0 => sleep_blink, 1 => idle_seen, 2 => cpu_idle, 3 => act_stretch,
                 4 => sc_ramp(0), 5 => sc_ramp(1), 6 => sc_ramp(2),
                 7 => ever_slept, 8 => diag_wrote);

    -- SW4 selects between them, and ONLY in a SUSPEND_EN build (normal_mode is a
    -- hard '0' otherwise, so a non-suspend build drives led straight from
    -- led_dbg exactly as before). In a suspend build, SW4 OFF (rest) = quiet;
    -- SW4 ON = debug. Because SW4 = dbg_idx bit3, turning debug on pins that bit
    -- to 0, which costs the post-panic dump only its bit3-set indices (e.g. perf
    -- index 40); every counter and the boot-status LEDs stay reachable. (SW3 was
    -- avoided -- it disturbs the board VGA; see the normal_mode assignment.)
    led <= led_diag   when (normal_mode = '1' and SUSPEND_DIAG) else
           led_normal when normal_mode = '1' else
           led_dbg;

    dbg_adr  <= cpu_adr;
    dbg_stb  <= cpu_cyc and cpu_stb;
    dbg_we   <= cpu_we;
    dbg_dat  <= cpu_dat_o;
    dbg_dati <= cpu_dat_i;
    dbg_ack  <= cpu_ack;
end architecture Behaviour;

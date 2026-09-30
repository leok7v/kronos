library ieee;
use ieee.std_logic_1164.all;

-- Synthesis top: the OneChipBook core with ONLY the real board I/O.
--
-- OneChipBook carries dbg_* taps (dbg_adr/stb/we/dat/dati/ack) that the
-- testbench probes. They are testbench-only, but they are still ENTITY PORTS,
-- so synthesizing OneChipBook directly turns all 86 of them into real FPGA
-- pins at locations the fitter picks arbitrarily ("Critical Warning: No exact
-- pin location assignment(s) for 86 pins"). On a populated board that is bad in
-- two ways:
--   * those pins are wired to actual peripherals (VGA, PS/2, USB, DB9, DIP),
--     so the FPGA drives against whatever else is on the net;
--   * dbg_dat/dbg_dati are the full 32-bit CPU bus data and toggle on EVERY
--     memory access -- and with the cache bypassed every access goes to SDRAM.
--     64 outputs switching together is exactly the simultaneous-switching noise
--     that eats VCCIO/ground margin, and the SDRAM read-capture window on this
--     board is already narrow (read_lat=3 was the only value that worked).
-- Note the working M2/M3a tops have no such ports -- this was unique to OneChipBook.
--
-- Wrapping is the fix: dbg_* are left `open`, so the fitter prunes them and the
-- design presents exactly the 64 real board pins the .qsf assigns. The
-- testbench keeps instantiating OneChipBook directly and is unaffected.
entity OneChipBookTop is
    port (
        clk      : in    std_logic;
        rst_n    : in    std_logic;
        led      : out   std_logic_vector(8 downto 0);
        sw       : in    std_logic_vector(7 downto 0);
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
        -- PS/2 keyboard
        ps2_clk   : in   std_logic;
        ps2_data  : in   std_logic;
        -- VGA text console
        vga_hsync : out  std_logic;
        vga_vsync : out  std_logic;
        vga_r     : out  std_logic_vector(5 downto 0);
        vga_g     : out  std_logic_vector(5 downto 0);
        vga_b     : out  std_logic_vector(5 downto 0)
    );
end OneChipBookTop;

architecture Behaviour of OneChipBookTop is
    -- SYSTEM CLOCK MULTIPLIER -- currently BYPASSED (x1). Set SYS_PLL_ON true
    -- and pick MULT/DIV to re-run the experiment.
    --
    -- x5/4 (26.85 MHz) was tried and is a NET LOSS on this board: 12224 -> 10845
    -- drystones. Timing was clean (0 violations, +6.139ns, fmax 32.15 MHz), and
    -- the cost per miss did NOT rise -- 12.3 -> 12.7 cycles -- so it is not the
    -- ns-bound SDRAM effect that was predicted. What rose was the NUMBER of
    -- misses: 31.8 -> 82.7 per Dhrystone pass, 2.6x, for a byte-identical
    -- program and an unchanged cache.
    --
    -- SDRAM read_lat is NOT the explanation: 3 is the only value that boots at
    -- all (4,5,6,7 all fail), because the burst is 2 halfwords so the capture
    -- window is exactly two cycles -- there is nothing to tune.
    --
    -- Best remaining hypothesis, unproven: the cache's M4K read-during-write.
    -- The write-back design depends on mixed-port forwarding and RDW semantics
    -- that STA models as ordinary paths; at a 25% shorter period they may
    -- resolve marginally, yielding a wrong tag -> spurious miss -> refetch.
    -- That is functionally survivable and would look EXACTLY like this: right
    -- answers, far more misses. Proving it needs a hit/miss counter inside the
    -- cache, i.e. debug ports down through Kronos/DataStorage.
    -- EXPERIMENT 2026-07-20 (2-way cache build): x6/5 = 25.77 MHz.
    -- Chosen deliberately: the 2-way cache dropped fabric Fmax to 25.87 MHz
    -- (the way-select mux is the new critical path), so x5/4 = 26.85 MHz -- the
    -- ratio the earlier experiment used -- would now EXCEED Fmax and fail to
    -- close. x6/5 = 21.477 * 6/5 = 25.77 MHz is the highest clean PLL ratio that
    -- stays under the ceiling, with a sliver of slack. read_lat stays 3 (the
    -- only value that boots), and 25.77 < the 26.85 that booted before, so SDRAM
    -- capture should hold. If this too is a net loss, revert to false.
    constant SYS_PLL_ON : boolean := true;
    constant PLL_MULT   : integer := 6;
    constant PLL_DIV    : integer := 5;
    constant OSC_HZ     : integer := 21477270;
    -- a function, because VHDL-93 has no conditional expression in a constant
    function sys_hz return integer is
    begin
        if SYS_PLL_ON then return OSC_HZ * PLL_MULT / PLL_DIV;
        else return OSC_HZ; end if;
    end function;
    constant CLK_HZ_NEW : integer := sys_hz;
    signal   clk_sys    : std_logic;
    signal   pll_locked : std_logic;
begin
    -- generics left at their defaults: they are already the hardware values
    -- (SD_READ_LAT=3, SD_CLK_DIV=64, BAUD_DIV=373, DISK_4KB=197).
    -- ---------------------------------------------- system clock PLL
    -- The PLL lives HERE rather than inside OneChipBook so the core simply
    -- runs faster with no internal change, and the testbench -- which
    -- instantiates OneChipBook directly -- is unaffected. The .sdc already
    -- has derive_pll_clocks, so timing analysis picks this up automatically.
    --
    -- x5/4: 21.47727 -> 26.847 MHz. Chosen over x4/3 (28.6 MHz) for margin:
    -- measured slack was +13.622ns on a 46.560ns period, i.e. a ~33.0ns
    -- critical path (fmax ~30.3 MHz). x5/4 leaves about +4.2ns, x4/3 only
    -- ~+1.9ns -- and the SDRAM read-capture window shrinks with the period on
    -- top of that. One step at a time from a known-good build.
    syspll : entity work.system_pll
        generic map (MULT => PLL_MULT, DIV => PLL_DIV, USE_PLL => SYS_PLL_ON)
        port map (inclk => clk, outclk => clk_sys, locked => pll_locked);

    onechip : entity work.OneChipBook
        generic map (
            -- Every frequency-derived constant follows CLK_HZ. Getting these
            -- wrong does not look like a clock problem: a stale BAUD_DIV gives
            -- console garbage and a stale tick divisor makes the OS clock run
            -- fast, neither of which points at the PLL.
            CLK_HZ   => CLK_HZ_NEW,
            BAUD_DIV => CLK_HZ_NEW / 57600,
            -- Suspend-to-RAM sleep mode ON for this build. The idle window is
            -- clock-derived (like every other time constant here) so it stays
            -- ~3 s whatever the PLL ratio -- short enough to observe sleep/wake
            -- during bring-up; raise to CLK_HZ_NEW * 30 for daily use once the
            -- sleep/wake round trip is confirmed on hardware. Keep the multiplier
            -- under ~80 or CLK_HZ_NEW*N overflows a 32-bit integer.
            -- Fallbacks if this misbehaves: set halt => '0' in OneChipBook
            -- (sleep still works via SDRAM self-refresh), then SUSPEND_EN => false
            -- to rebuild the exact validated bitstream.
            SUSPEND_EN          => true,
            -- Power-on default sleep timeout in seconds; the OS overrides it at
            -- runtime via out(0x20, seconds). 1200 = 20 min.
            SUSPEND_DEFAULT_SEC => 1200,
            -- Clean board: diagnostics off. Normal mode shows only LED1 (sleep
            -- blink) and LED9 (panic); LED2..LED8 stay dark.
            SUSPEND_DIAG        => false)
        port map (
            clk => clk_sys, clk_ref => clk, rst_n => rst_n, led => led, sw => sw,
            sd_sclk => sd_sclk, sd_mosi => sd_mosi, sd_cs => sd_cs, sd_miso => sd_miso,
            uart_txd => uart_txd, uart_rxd => uart_rxd,
            sd_a => sd_a, sd_ba => sd_ba, sd_dq => sd_dq, sd_cke => sd_cke,
            sd_cs_n => sd_cs_n, sd_ras_n => sd_ras_n, sd_cas_n => sd_cas_n,
            sd_we_n => sd_we_n, sd_dqml => sd_dqml, sd_dqmh => sd_dqmh, sd_clk => sd_clk,
            ps2_clk => ps2_clk, ps2_data => ps2_data,
            vga_hsync => vga_hsync, vga_vsync => vga_vsync,
            vga_r => vga_r, vga_g => vga_g, vga_b => vga_b,
            -- testbench-only taps: left open so the fitter prunes them
            dbg_adr => open, dbg_stb => open, dbg_we => open,
            dbg_dat => open, dbg_dati => open, dbg_ack => open);
end architecture Behaviour;

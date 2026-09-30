library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Ideal drop-in replacement for sdram_controller (same entity name + ports):
-- a flat, zero-initialised word memory with a simple 1-cycle Wishbone ack and
-- NO latency/burst/refresh. Used ONLY in sim to isolate whether a boot failure
-- is in the real SDRAM controller (heavy cache write-back/refill traffic that
-- the M2 stub never exercised) versus elsewhere. Behaves like the Spartan-3
-- direct SRAM that boots the real OS. The SDRAM device pins are driven inert.

entity sdram_controller is
    generic (
        ADDR_BITS : integer := 19;
        T_INIT    : integer := 5000;
        T_REFI    : integer := 150;
        CAS_LAT   : integer := 2;
        T_RCD     : integer := 2;
        T_RP      : integer := 2;
        T_RFC     : integer := 4;
        T_MRD     : integer := 2;
        T_WR      : integer := 2
    );
    port (
        clk      : in  std_logic;
        reset    : in  std_logic;
        wb_adr   : in  std_logic_vector(ADDR_BITS-1 downto 0);
        wb_dat_i : in  std_logic_vector(31 downto 0);
        wb_dat_o : out std_logic_vector(31 downto 0);
        wb_we    : in  std_logic;
        wb_stb   : in  std_logic;
        wb_cyc   : in  std_logic;
        wb_ack   : out std_logic;
        ready    : out std_logic;
        read_lat : in  std_logic_vector(3 downto 0) := "0100";
        sd_a     : out std_logic_vector(12 downto 0);
        sd_ba    : out std_logic_vector(1 downto 0);
        sd_dq    : inout std_logic_vector(15 downto 0);
        sd_cke   : out std_logic;
        sd_cs_n  : out std_logic;
        sd_ras_n : out std_logic;
        sd_cas_n : out std_logic;
        sd_we_n  : out std_logic;
        sd_dqml  : out std_logic;
        sd_dqmh  : out std_logic
    );
end sdram_controller;

architecture ideal of sdram_controller is
    -- Backs the whole usable RAM window [0, RAM_TOP) that the OneChipBook top exposes
    -- (0x70000 = 458752 words = 1792 KB, below the 0x7F000 I/O page). The OneChipBook
    -- asserts a bus error at/above RAM_TOP, which is what terminates the
    -- booter's memory_top() fill loop (it fills RAM with MOVE blocks until a
    -- memory fault). Accesses past this array still ack (reading 0) but the
    -- top-level err makes the CPU trap, so no wrap/alias corruption occurs.
    constant RAM_WORDS : integer := 131072;  -- 0x20000 = 512 KB (= OneChipBook RAM_TOP, = cache RAM boundary 2^17)
    type mem_t is array (0 to RAM_WORDS - 1) of std_logic_vector(31 downto 0);
    signal mem     : mem_t := (others => (others => '0'));
    signal ack_r   : std_logic := '0';
    signal dat_r   : std_logic_vector(31 downto 0) := (others => '0');
    signal init_c  : integer range 0 to 63 := 0;
    signal rdy     : std_logic := '0';
begin
    sd_a <= (others => '0'); sd_ba <= (others => '0'); sd_dq <= (others => 'Z');
    sd_cke <= '1'; sd_cs_n <= '1'; sd_ras_n <= '1'; sd_cas_n <= '1';
    sd_we_n <= '1'; sd_dqml <= '1'; sd_dqmh <= '1';

    wb_ack   <= ack_r;
    wb_dat_o <= dat_r;
    ready    <= rdy;

    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                ack_r <= '0'; init_c <= 0; rdy <= '0';
            else
                if rdy = '0' then
                    if init_c = 32 then rdy <= '1'; else init_c <= init_c + 1; end if;
                end if;
                -- Ack for EXACTLY ONE cycle per transaction, then drop -- even
                -- if stb stays high. The data cache issues back-to-back cycles
                -- with stb held (a dirty instruction-fetch miss FLUSHes the old
                -- line then READs the refill without ever dropping stb). Holding
                -- ack across them let the refill READ complete against the
                -- FLUSH write's stale ack/data -> the code word read back as 0.
                -- A one-cycle ack delimits each transaction, so the refill gets
                -- its own read (and the ack=0 gap the cache waits on).
                if ack_r = '1' then
                    ack_r <= '0';
                elsif rdy = '1' and wb_cyc = '1' and wb_stb = '1' then
                    if to_integer(unsigned(wb_adr)) < RAM_WORDS then
                        if wb_we = '1' then
                            mem(to_integer(unsigned(wb_adr))) <= wb_dat_i;
                        else
                            dat_r <= mem(to_integer(unsigned(wb_adr)));
                        end if;
                    else
                        dat_r <= (others => '0');   -- past physical RAM: reads 0
                    end if;
                    ack_r <= '1';
                end if;
            end if;
        end if;
    end process;
end ideal;

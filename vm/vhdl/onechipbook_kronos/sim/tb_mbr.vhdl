library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Unit test for the MBR partition parsing added to sd_disk_controller.
--
-- It drives the controller's io2 register interface directly (no CPU) against
-- sd_model_xd0 loaded with mbrtest.dec (see gen_mbrtest.py). That image gives
-- every sector a first word equal to its own ABSOLUTE sector number, and a real
-- MBR with three partitions:
--     unit 0 -> LBA 8,  16 sectors
--     unit 1 -> LBA 32, 16 sectors
--     unit 2 -> LBA 64, 16 sectors
-- So an io2 read of unit U sector S must return part_base(U)+S, which is exactly
-- the disk_base+cur_sec the controller is meant to form from the parsed table.
-- It also checks getsize4kb (= sectors/8) and that an out-of-range unit fails.

entity tb_mbr is
end tb_mbr;

architecture sim of tb_mbr is
    signal clk   : std_logic := '0';
    signal reset : std_logic := '1';

    signal reg_adr   : std_logic_vector(2 downto 0) := (others => '0');
    signal reg_dat_i : std_logic_vector(31 downto 0) := (others => '0');
    signal reg_dat_o : std_logic_vector(31 downto 0);
    signal reg_stb   : std_logic := '0';
    signal reg_we    : std_logic := '0';

    signal dma_adr : std_logic_vector(18 downto 0);
    signal dma_dat : std_logic_vector(31 downto 0);
    signal dma_stb : std_logic;
    signal dma_we  : std_logic;
    signal dma_ack : std_logic := '1';   -- boot DMA: accept immediately

    signal boot_done : std_logic;
    signal boot_fail : std_logic;
    signal busy_o    : std_logic;

    signal sd_clk, sd_cs, sd_mosi, sd_miso : std_logic;

    signal done : boolean := false;
begin
    clk <= not clk after 10 ns when not done else '0';

    dut : entity work.sd_disk_controller
        -- CLK_DIV must exceed the reader's CLK_DIV_FAST (8) or its clkdiv_cnt
        -- range 0..CLK_DIV-1 overflows once init switches to the fast divider
        generic map (CLK_DIV => 16, BOOT_SECTORS => 1)
        port map (
            clk => clk, reset => reset,
            reg_adr => reg_adr, reg_dat_i => reg_dat_i, reg_dat_o => reg_dat_o,
            reg_stb => reg_stb, reg_we => reg_we,
            dma_adr => dma_adr, dma_dat => dma_dat, dma_stb => dma_stb,
            dma_we => dma_we, dma_ack => dma_ack,
            boot_done => boot_done, boot_fail => boot_fail, busy_o => busy_o,
            sd_clk => sd_clk, sd_cs => sd_cs, sd_mosi => sd_mosi, sd_miso => sd_miso);

    card : entity work.sd_model_xd0
        generic map (FNAME => "mbrtest.dec", SECTORS => 128)
        port map (sclk => sd_clk, cs => sd_cs, mosi => sd_mosi, miso => sd_miso,
                  dbg_wsec => open, dbg_wxor => open, dbg_wdone => open);

    stim : process
        -- one register write (accepted while the controller is idle)
        procedure rwrite(a : std_logic_vector(2 downto 0);
                         d : std_logic_vector(31 downto 0)) is
        begin
            wait until rising_edge(clk);
            reg_adr <= a; reg_dat_i <= d; reg_we <= '1'; reg_stb <= '1';
            wait until rising_edge(clk);
            reg_stb <= '0'; reg_we <= '0';
        end procedure;

        -- one register read; reading DATA (adr 111) also consumes the word
        procedure rread(a : std_logic_vector(2 downto 0);
                        v : out std_logic_vector(31 downto 0)) is
        begin
            wait until rising_edge(clk);
            reg_adr <= a; reg_we <= '0'; reg_stb <= '1';
            wait until rising_edge(clk);
            v := reg_dat_o;
            reg_stb <= '0';
        end procedure;

        variable v : std_logic_vector(31 downto 0);

        -- issue a read of `len` bytes at sector `sec` of unit `dsk`, return the
        -- first delivered data word
        -- Uses its OWN locals (not the shared v): the caller passes v as `word`,
        -- so polling into v here would clobber the result before it is checked.
        procedure io_read(dsk, sec, len : integer;
                          word : out std_logic_vector(31 downto 0)) is
            variable w  : std_logic_vector(31 downto 0);
            variable fl : std_logic_vector(31 downto 0);
        begin
            rwrite("000", std_logic_vector(to_unsigned(sec, 32)));   -- SEC
            rwrite("010", std_logic_vector(to_unsigned(len, 32)));   -- LEN
            rwrite("110", std_logic_vector(to_unsigned(dsk, 32)));   -- DSK
            rwrite("011", std_logic_vector(to_unsigned(4, 32)));     -- CMD=read
            loop                                                     -- wait DATA
                rread("101", fl);
                exit when fl(0) = '1';                               -- dat_valid
            end loop;
            rread("111", w);                                         -- read+consume
            loop
                rread("101", fl);
                exit when fl(1) = '1';                               -- op_done
            end loop;
            word := w;
        end procedure;

        procedure check(got, exp : std_logic_vector(31 downto 0); msg : string) is
        begin
            assert got = exp
                report "FAIL: " & msg & " got=" & integer'image(to_integer(unsigned(got)))
                       & " exp=" & integer'image(to_integer(unsigned(exp)))
                severity failure;
            report "ok: " & msg & " = " & integer'image(to_integer(unsigned(got)));
        end procedure;
    begin
        reset <= '1';
        wait for 200 ns;
        wait until rising_edge(clk);
        reset <= '0';

        -- card init + MBR read + boot must complete
        wait until boot_done = '1' for 20 ms;
        assert boot_done = '1'
            report "FAIL: controller never finished boot/MBR read" severity failure;

        -- unit 0 -> base 8
        io_read(0, 0, 4, v);  check(v, x"00000008", "unit0 sec0 -> LBA");
        io_read(0, 1, 4, v);  check(v, x"00000009", "unit0 sec1 -> LBA");
        -- unit 1 -> base 32
        io_read(1, 0, 4, v);  check(v, x"00000020", "unit1 sec0 -> LBA");
        io_read(1, 5, 4, v);  check(v, x"00000025", "unit1 sec5 -> LBA");
        -- unit 2 (the FAT partition) -> base 64
        io_read(2, 0, 4, v);  check(v, x"00000040", "unit2 sec0 -> LBA");
        io_read(2, 3, 4, v);  check(v, x"00000043", "unit2 sec3 -> LBA");

        -- getsize4kb: 16 sectors / 8 = 2 blocks
        rwrite("110", std_logic_vector(to_unsigned(1, 32)));   -- DSK=1
        rwrite("011", std_logic_vector(to_unsigned(3, 32)));   -- CMD=getsize
        loop rread("101", v); exit when v(0) = '1'; end loop;
        rread("111", v);      check(v, x"00000002", "unit1 getsize4kb");
        loop rread("101", v); exit when v(1) = '1'; end loop;

        -- unit 3 has no partition -> the op must fail at dispatch (RESULT=0),
        -- before SEC/LEN are even looked at
        rwrite("110", std_logic_vector(to_unsigned(3, 32)));   -- DSK=3
        rwrite("011", std_logic_vector(to_unsigned(4, 32)));   -- CMD=read
        loop rread("101", v); exit when v(1) = '1'; end loop;  -- op_done
        rread("100", v);
        assert v(0) = '0' report "FAIL: unit3 should have failed" severity failure;
        report "ok: unit3 rejected";

        report "*** PASS: MBR partition mapping verified ***";
        done <= true;
        wait;
    end process;
end sim;

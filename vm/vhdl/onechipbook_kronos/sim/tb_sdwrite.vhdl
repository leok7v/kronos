library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- End-to-end check of io2 WRITE (op 5): drives sd_disk_controller exactly the
-- way the io2 microcode does, through the REAL sd_reader and a card model that
-- captures CMD24 payloads, and verifies the bytes that reach the card are the
-- bytes the CPU handed over.
--
-- This is the first thing in the project that can WRITE to an SD card, so it is
-- worth proving in simulation before it ever runs against real media: a wrong
-- byte order or a miscounted length would corrupt a real filesystem rather than
-- just fail to boot.
--
-- The tb plays the part of the microcode's io2 loop: poll FLAGS, and whenever
-- bit2 ("wants a word") is set, hand over the next word.

entity tb_sdwrite is
    generic (
        -- FAILMODE=true: the card rejects CMD24. The write must then FAIL
        -- cleanly rather than hang -- the CPU waits on FLAGS.done inside the
        -- io2 loop, so a controller that never finishes freezes the machine.
        FAILMODE : boolean := false;
        -- which unit to write to: proves /dev/xd1 lands in its own region of
        -- the card rather than on top of xd0
        DISKNO   : integer := 0);
end tb_sdwrite;

architecture sim of tb_sdwrite is
    constant PERIOD : time := 46561 ps;      -- 21.47727 MHz

    signal clk    : std_logic := '0';
    signal reset  : std_logic := '1';
    signal done   : boolean := false;

    signal reg_adr   : std_logic_vector(2 downto 0) := (others => '0');
    signal reg_dat_i : std_logic_vector(31 downto 0) := (others => '0');
    signal reg_dat_o : std_logic_vector(31 downto 0);
    signal reg_stb   : std_logic := '0';
    signal reg_we    : std_logic := '0';

    signal dma_adr : std_logic_vector(18 downto 0);
    signal dma_dat : std_logic_vector(31 downto 0);
    signal dma_stb, dma_we : std_logic;
    signal boot_done, boot_fail, busy_o : std_logic;

    signal sclk, scs, smosi, smiso : std_logic;
    signal wsec, wxor : std_logic_vector(31 downto 0);
    signal wdone : std_logic;

    constant SEC_UNDER_TEST : integer := 100;
    constant NSECS  : integer := 3;          -- MULTI-sector: the single-sector
                                             -- case hid a word-accounting bug
    constant NWORDS : integer := 128 * NSECS;

    -- the data the "CPU" hands over: a recognisable pattern
    function testword(i : integer) return std_logic_vector is
    begin
        return std_logic_vector(to_unsigned(16#11223300# + i, 32));
    end function;
begin
    clk <= not clk after PERIOD / 2 when not done else '0';

    dut : entity work.sd_disk_controller
        generic map (CLK_DIV => 16, BOOT_SECTORS => 0, DISK_4KB => 197,
                     WDOG_CYCLES => 2000000)
        port map (clk => clk, reset => reset,
                  reg_adr => reg_adr, reg_dat_i => reg_dat_i, reg_dat_o => reg_dat_o,
                  reg_stb => reg_stb, reg_we => reg_we,
                  dma_adr => dma_adr, dma_dat => dma_dat, dma_stb => dma_stb,
                  dma_we => dma_we, dma_ack => '0',
                  boot_done => boot_done, boot_fail => boot_fail, busy_o => busy_o,
                  sd_clk => sclk, sd_cs => scs, sd_mosi => smosi, sd_miso => smiso);

    card : entity work.sd_model_xd0
        generic map (FNAME => "xd0.dec", SECTORS => 1580, FAIL_WRITE => FAILMODE)
        port map (sclk => sclk, cs => scs, mosi => smosi, miso => smiso,
                  dbg_wsec => wsec, dbg_wxor => wxor, dbg_wdone => wdone);

    stim : process
        variable expect_xor : unsigned(31 downto 0) := (others => '0');
        variable w : std_logic_vector(31 downto 0);
        variable sent : integer := 0;
        variable guard : integer := 0;

        procedure wreg(a : integer; d : std_logic_vector(31 downto 0)) is
        begin
            wait until rising_edge(clk);
            reg_adr <= std_logic_vector(to_unsigned(a, 3));
            reg_dat_i <= d; reg_we <= '1'; reg_stb <= '1';
            wait until rising_edge(clk);
            reg_stb <= '0'; reg_we <= '0';
        end procedure;
    begin
        -- expected checksum of the LAST sector (the model keeps the most recent
        -- CMD24 payload), so this also proves the sector-to-sector handover
        for i in NWORDS - 128 to NWORDS - 1 loop
            w := testword(i);
            for b in 0 to 3 loop
                expect_xor := expect_xor xor
                    resize(unsigned(w(8 * b + 7 downto 8 * b)), 32);
            end loop;
        end loop;

        wait for PERIOD * 20;
        reset <= '0';
        wait until boot_done = '1' for 50 ms;     -- BOOT_SECTORS=0: card init only
        assert boot_done = '1' report "FAIL: card never became ready" severity failure;
        report "card ready; starting write of sector "
               & integer'image(SEC_UNDER_TEST) severity note;

        wreg(0, std_logic_vector(to_unsigned(SEC_UNDER_TEST, 32)));   -- SEC
        wreg(2, std_logic_vector(to_unsigned(NWORDS * 4, 32)));       -- LEN bytes
        wreg(6, std_logic_vector(to_unsigned(DISKNO, 32)));           -- DSK
        wreg(3, std_logic_vector(to_unsigned(5, 32)));                -- CMD = write

        if FAILMODE or DISKNO >= 2 then
            -- Two no-data cases that must still COMPLETE rather than hang:
            -- a card refusing CMD24, and a unit that does not exist. The OS
            -- mounts /dev/xd2 at boot, so a non-completing unit check would
            -- freeze the machine during startup.
            guard := 0;
            loop
                wait until rising_edge(clk);
                reg_adr <= "101"; reg_stb <= '1'; reg_we <= '0';
                wait until rising_edge(clk);
                reg_stb <= '0';
                exit when reg_dat_o(1) = '1';
                guard := guard + 1;
                assert guard < 2000000
                    report "FAIL: stuck write never completed -- this would HANG the machine"
                    severity failure;
            end loop;
            assert reg_dat_o(1) = '1' severity failure;
            report "*** PASS: no-data case (FAILMODE=" & boolean'image(FAILMODE)
                   & " unit=" & integer'image(DISKNO)
                   & ") completed cleanly instead of hanging ***" severity note;
            done <= true;
            wait;
        end if;

        -- play the io2 microcode loop
        while sent < NWORDS loop
            wait until rising_edge(clk);
            reg_adr <= "101"; reg_stb <= '1'; reg_we <= '0';   -- FLAGS
            wait until rising_edge(clk);
            reg_stb <= '0';
            if reg_dat_o(2) = '1' then                          -- wants a word
                wreg(7, testword(sent));
                sent := sent + 1;
            end if;
            guard := guard + 1;
            assert guard < 8000000
                report "FAIL: controller never asked for all the words"
                severity failure;
        end loop;
        report "all " & integer'image(sent) & " words handed over" severity note;

        -- wait for the op to complete
        guard := 0;
        loop
            wait until rising_edge(clk);
            reg_adr <= "101"; reg_stb <= '1'; reg_we <= '0';
            wait until rising_edge(clk);
            reg_stb <= '0';
            exit when reg_dat_o(1) = '1';                       -- done
            if guard mod 200000 = 0 then
                report "waiting: FLAGS=" & integer'image(to_integer(unsigned(reg_dat_o(2 downto 0))))
                       & " busy=" & std_logic'image(busy_o)
                       & " t=" & time'image(now) severity note;
            end if;
            guard := guard + 1;
            assert guard < 8000000 report "FAIL: write never completed" severity failure;
        end loop;

        wait until rising_edge(clk);
        reg_adr <= "100"; reg_stb <= '1'; reg_we <= '0';        -- RESULT
        wait until rising_edge(clk);
        reg_stb <= '0';
        assert reg_dat_o(0) = '1'
            report "FAIL: controller reported write failure" severity failure;

        assert unsigned(wsec) = DISKNO * 65536 + SEC_UNDER_TEST + NSECS - 1
            report "FAIL: card was written at sector "
                   & integer'image(to_integer(unsigned(wsec)))
                   & ", expected " & integer'image(DISKNO * 65536 + SEC_UNDER_TEST + NSECS - 1)
            severity failure;
        assert unsigned(wxor) = expect_xor
            report "FAIL: card received different bytes (xor "
                   & integer'image(to_integer(unsigned(wxor))) & " vs expected "
                   & integer'image(to_integer(expect_xor)) & ")"
            severity failure;

        report "*** PASS: io2 write of " & integer'image(NSECS)
               & " sectors to unit " & integer'image(DISKNO)
               & ", last card sector " & integer'image(DISKNO * 65536 + SEC_UNDER_TEST + NSECS - 1)
               & " with the right bytes (xor "
               & integer'image(to_integer(expect_xor)) & ") ***" severity note;
        done <= true;
        wait;
    end process;
end architecture sim;

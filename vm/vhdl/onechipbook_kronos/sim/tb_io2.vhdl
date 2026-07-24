library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;

-- Isolated io2 read bench at the REAL hardware SPI rate (CLK_DIV=64). Drives just
-- sd_disk_controller + sd_reader + sd_model_xd0 (no CPU / no SDRAM / no memory_top)
-- through op #1 (read 4096B from sector 16) then op #2 (from sector 336), draining
-- the DATA register exactly like the io2 microcode. Reports each op's word count,
-- XOR checksum, and first 8 words -- so we can tell if the DELIVERED data corrupts
-- under the slow SPI (which the fast full-sim never exposed).
entity tb_io2 is end;
architecture sim of tb_io2 is
    signal clk       : std_logic := '0';
    signal reset     : std_logic := '1';
    signal reg_adr   : std_logic_vector(2 downto 0) := "000";
    signal reg_dat_i : std_logic_vector(31 downto 0) := (others => '0');
    signal reg_dat_o : std_logic_vector(31 downto 0);
    signal reg_stb   : std_logic := '0';
    signal reg_we    : std_logic := '0';
    signal dma_adr   : std_logic_vector(18 downto 0);
    signal dma_dat   : std_logic_vector(31 downto 0);
    signal dma_stb, dma_we : std_logic;
    signal dma_ack   : std_logic := '0';
    signal boot_done, boot_fail, busy_o : std_logic;
    signal sd_clk, sd_cs, sd_mosi, sd_miso : std_logic;
    signal done      : boolean := false;
begin
    clk <= not clk after 23.28 ns when not done else '0';

    dut : entity work.sd_disk_controller
        generic map (CLK_DIV => 64, BOOT_SECTORS => 1, DISK_4KB => 197)
        port map (clk => clk, reset => reset,
                  reg_adr => reg_adr, reg_dat_i => reg_dat_i, reg_dat_o => reg_dat_o,
                  reg_stb => reg_stb, reg_we => reg_we,
                  dma_adr => dma_adr, dma_dat => dma_dat, dma_stb => dma_stb,
                  dma_we => dma_we, dma_ack => dma_ack,
                  boot_done => boot_done, boot_fail => boot_fail, busy_o => busy_o,
                  sd_clk => sd_clk, sd_cs => sd_cs, sd_mosi => sd_mosi, sd_miso => sd_miso);

    card : entity work.sd_model_xd0
        generic map (FNAME => "xd0.dec", SECTORS => 1580)
        port map (sclk => sd_clk, cs => sd_cs, mosi => sd_mosi, miso => sd_miso);

    -- ack the preload DMA immediately (we don't care about the preloaded data)
    process (clk) begin
        if rising_edge(clk) then dma_ack <= dma_stb; end if;
    end process;

    stim : process
        procedure clkw(n : integer) is begin
            for i in 1 to n loop wait until rising_edge(clk); end loop;
        end procedure;
        procedure regwrite(a : std_logic_vector(2 downto 0); d : integer) is begin
            wait until rising_edge(clk);
            reg_adr <= a; reg_dat_i <= std_logic_vector(to_unsigned(d, 32));
            reg_we <= '1'; reg_stb <= '1';
            wait until rising_edge(clk);
            reg_stb <= '0'; reg_we <= '0';
        end procedure;
        procedure regread(a : std_logic_vector(2 downto 0); v : out std_logic_vector(31 downto 0)) is begin
            wait until rising_edge(clk);
            reg_adr <= a; reg_we <= '0'; reg_stb <= '1';
            wait until rising_edge(clk);
            reg_stb <= '0';
            v := reg_dat_o;
        end procedure;

        variable flags, dat : std_logic_vector(31 downto 0);
        variable nwords, guard : integer;
        variable csum : unsigned(31 downto 0);

        procedure do_read(sec : integer; tag : string) is begin
            regwrite("010", 4096);   -- LEN bytes
            regwrite("000", sec);    -- SEC
            regwrite("110", 1);      -- DSK
            regwrite("011", 4);      -- CMD = read (dispatches)
            nwords := 0; csum := (others => '0'); guard := 0;
            loop
                regread("101", flags);           -- FLAGS
                if flags(0) = '1' then           -- data word available
                    regread("111", dat);         -- DATA (consumes)
                    csum := csum xor unsigned(dat);
                    if nwords < 8 then
                        report tag & " w" & integer'image(nwords) & "=" &
                               integer'image(to_integer(signed(dat))) severity note;
                    end if;
                    nwords := nwords + 1; guard := 0;
                elsif flags(1) = '1' then        -- op done
                    exit;
                else
                    clkw(40); guard := guard + 1;
                    if guard > 120000 then
                        report tag & " HANG after " & integer'image(nwords) & " words" severity note;
                        exit;
                    end if;
                end if;
            end loop;
            report tag & " DONE nwords=" & integer'image(nwords) &
                   " xor=" & integer'image(to_integer(signed(std_logic_vector(csum)))) severity note;
        end procedure;
    begin
        reset <= '1'; clkw(100); reset <= '0';
        for i in 0 to 80 loop
            exit when boot_done = '1';
            clkw(50000);
        end loop;
        report "boot_done=" & std_logic'image(boot_done) severity note;
        do_read(16,  "OP1s16");
        do_read(336, "OP2s336");
        report "=== ALL DONE ===" severity note;
        done <= true;
        wait;
    end process;
end sim;

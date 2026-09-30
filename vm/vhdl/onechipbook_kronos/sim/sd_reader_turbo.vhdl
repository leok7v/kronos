library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;

-- Turbo (sim-only) drop-in for sd_reader: SAME entity/ports, but instead of
-- bit-banging SPI to sd_model it serves sectors DIRECTLY from the disk image
-- (xd0.dec, one decimal byte per line -- same file sd_model_xd0 loads). This
-- keeps the exact byte-stream the real reader would produce, but delivers a
-- byte every DELAY clocks (no SPI clock toggling), so the OS's ~98 io2 disk
-- reads finish in a fraction of the sim time AND far fewer events -> "Loaded
-- OK" is reachable without an hour-long run. DELAY is paced ABOVE the io2
-- microcode drain (~tens of clocks/word) so the disk controller never overruns
-- its one-word delivery buffer (dma_over). Analyse this INSTEAD of
-- src/sd_reader.vhdl in the sim build; the SD pins are
-- driven inert and sd_model_xd0 goes unused.

entity sd_reader is
    generic (
        CLK_DIV  : integer := 64;
        POLL_MAX : integer := 1000000;
        ACMD_MAX : integer := 5000
    );
    port (
        clk        : in  std_logic;
        reset      : in  std_logic;
        rd_req     : in  std_logic;
        -- Write side: this fast model ACCEPTS and DISCARDS written blocks. It
        -- exists to keep the boot sim from hanging if the OS writes; use the
        -- real sd_reader + a writable card model to verify write CONTENT.
        wr_req     : in  std_logic := '0';
        wr_data    : in  std_logic_vector(7 downto 0) := (others => '0');
        wr_valid   : in  std_logic := '1';   -- ignored: this model discards writes
        wr_next    : out std_logic;
        lba        : in  std_logic_vector(31 downto 0);
        data       : out std_logic_vector(7 downto 0);
        data_valid : out std_logic;
        ready      : out std_logic;
        error      : out std_logic;
        sd_clk     : out std_logic;
        sd_cs      : out std_logic;
        sd_mosi    : out std_logic;
        sd_miso    : in  std_logic;
        dbg_ph     : out std_logic_vector(2 downto 0);
        dbg_r1     : out std_logic_vector(7 downto 0);
        dbg_flags  : out std_logic_vector(5 downto 0);
        dbg_acmd_r1 : out std_logic_vector(7 downto 0);
        dbg_cmd1_r1 : out std_logic_vector(7 downto 0)
    );
end sd_reader;

architecture turbo of sd_reader is
    constant SECTORS : integer := 1580;    -- xd0.dsk = 808960 bytes
    constant DELAY   : integer := 12;      -- clocks between delivered bytes (> drain)
    type img_t is array(0 to SECTORS*512-1) of integer range 0 to 255;

    impure function load_img(fname : string) return img_t is
        file f      : text;
        variable l  : line;
        variable v  : integer;
        variable im : img_t := (others => 0);
    begin
        file_open(f, fname, read_mode);
        for i in img_t'range loop
            exit when endfile(f);
            readline(f, l);
            read(l, v);
            im(i) := v;
        end loop;
        file_close(f);
        return im;
    end function;

    signal image  : img_t := load_img("xd0.dec");
    signal wbusy : std_logic := '0';
    signal wcnt  : integer range 0 to 511 := 0;
    signal rdy    : std_logic := '0';
    signal initc  : integer range 0 to 127 := 0;
    signal busy   : std_logic := '0';
    signal base   : integer := 0;           -- byte offset of current sector
    signal bidx   : integer range 0 to 512 := 0;
    signal tick   : integer range 0 to DELAY := 0;
begin
    -- SD pins inert; debug taps zero
    sd_clk <= '0'; sd_cs <= '1'; sd_mosi <= '1';
    dbg_ph <= (others => '0'); dbg_r1 <= (others => '0'); dbg_flags <= (others => '0');
    dbg_acmd_r1 <= (others => '0'); dbg_cmd1_r1 <= (others => '0');
    ready <= rdy;

    error <= '0';

    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                rdy <= '0'; initc <= 0; busy <= '0'; data_valid <= '0'; bidx <= 0; tick <= 0;
                wr_next <= '0'; wbusy <= '0'; wcnt <= 0;
            else
                data_valid <= '0';
                wr_next    <= '0';
                -- swallow a written block, one byte per tick
                if wbusy = '1' then
                    wr_next <= '1';
                    if wcnt = 511 then wbusy <= '0'; rdy <= '1'; wcnt <= 0;
                    else wcnt <= wcnt + 1; end if;
                elsif wr_req = '1' then
                    wbusy <= '1'; rdy <= '0'; wcnt <= 0;
                end if;
                if rdy = '0' then                       -- brief "card init"
                    if initc = 64 then rdy <= '1'; else initc <= initc + 1; end if;
                elsif busy = '0' then
                    if rd_req = '1' then                 -- start a sector
                        busy <= '1';
                        base <= to_integer(unsigned(lba)) * 512;
                        bidx <= 0;
                        tick <= 0;
                    end if;
                else                                     -- deliver 512 bytes, paced
                    if tick = DELAY then
                        tick <= 0;
                        if base + bidx < SECTORS*512 then
                            data <= std_logic_vector(to_unsigned(image(base + bidx), 8));
                        else
                            data <= (others => '0');   -- past image: 0 (like a short disk)
                        end if;
                        data_valid <= '1';
                        if bidx = 511 then
                            busy <= '0';
                        else
                            bidx <= bidx + 1;
                        end if;
                    else
                        tick <= tick + 1;
                    end if;
                end if;
            end if;
        end if;
    end process;
end architecture turbo;

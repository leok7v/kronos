library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;

-- Behavioural SPI-mode SDHC card model that serves a FULL DISK IMAGE
-- (default xd0.dec = all 808960 bytes of xd0.dsk, one decimal byte per line;
-- generate with sim/gen_xd0_dec.py). Unlike sd_model_boot it captures the full
-- 32-bit CMD17 argument, so any sector of the image can be read -- which is
-- what the OneChipBook io2 disk controller does while booting the real OS.
-- Block-addressed (SDHC, CCS=1): CMD17 arg = sector number.

entity sd_model_xd0 is
    generic (
        FNAME   : string  := "xd0.dec";
        SECTORS : integer := 1580;
        -- reject CMD24 with an illegal-command R1, to exercise the host's
        -- failure path (a real card that will not take a write)
        FAIL_WRITE : boolean := false
    );
    port (
        sclk : in  std_logic;
        cs   : in  std_logic;   -- active low
        mosi : in  std_logic;
        miso : out std_logic;
        -- what the last CMD24 actually delivered, so a testbench can check that
        -- the bytes reaching the card are the bytes the CPU sent
        dbg_wsec  : out std_logic_vector(31 downto 0);
        dbg_wxor  : out std_logic_vector(31 downto 0);
        dbg_wdone : out std_logic
    );
end sd_model_xd0;

architecture sim of sd_model_xd0 is
    -- (write-capture taps are driven concurrently at the end)
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

    signal image : img_t := load_img(FNAME);

    type resp_t is (R_IDLE, R_GAP, R_R1, R_EXTRA, R_TOKGAP, R_TOKEN, R_DATA, R_CRC,
                    -- CMD24: swallow the host's data token + 512 bytes + CRC,
                    -- then answer 0x05 (accepted) and hold busy briefly
                    W_TOK, W_RECV, W_CRC, W_RESP, W_BUSY);
    signal resp     : resp_t := R_IDLE;
    signal resp_cnt : integer := 0;

    signal bitcnt   : integer range 0 to 7 := 0;
    signal rx_sr    : std_logic_vector(7 downto 0) := (others => '0');
    signal tx_sr    : std_logic_vector(7 downto 0) := (others => '1');

    signal in_cmd   : boolean := false;
    signal cmd_cnt  : integer range 0 to 5 := 0;
    signal cmd_idx  : std_logic_vector(5 downto 0) := (others => '0');
    signal wcnt     : integer range 0 to 511 := 0;
    signal wxor_r   : std_logic_vector(31 downto 0) := (others => '0');
    signal wsec_r   : std_logic_vector(31 downto 0) := (others => '0');
    signal arg32    : std_logic_vector(31 downto 0) := (others => '0');
    signal app_next : boolean := false;

    signal extra    : std_logic_vector(31 downto 0) := (others => '0');
begin

    process (sclk, cs)
        variable rxb     : std_logic_vector(7 downto 0);
        variable next_tx : std_logic_vector(7 downto 0);
        variable bidx    : integer;
    begin
        if cs = '1' then
            bitcnt <= 0; tx_sr <= (others => '1'); miso <= '1';
            in_cmd <= false; resp <= R_IDLE; resp_cnt <= 0;
        elsif rising_edge(sclk) then
            rxb := rx_sr(6 downto 0) & mosi;
            rx_sr <= rxb;
            if bitcnt = 7 then
                bitcnt <= 0;

                -- ---- command parser ---------------------------------------
                if resp = W_TOK or resp = W_RECV or resp = W_CRC then
                    null;                       -- payload, not commands
                elsif not in_cmd then
                    if rxb(7 downto 6) = "01" then
                        in_cmd  <= true; cmd_cnt <= 0;
                        cmd_idx <= rxb(5 downto 0);
                    end if;
                else
                    if cmd_cnt <= 3 then arg32 <= arg32(23 downto 0) & rxb; end if;
                    if cmd_cnt = 5 then
                        in_cmd <= false;
                        resp <= R_GAP; resp_cnt <= 0;
                        case to_integer(unsigned(cmd_idx)) is
                            when 0  => extra <= (others => '0');
                            when 8  => extra <= x"000001AA";
                            when 55 => app_next <= true;
                            when 41 => null;
                            when 58 => extra <= x"C0FF8000";        -- OCR: CCS=1
                            when 17 => null;
                            when 24 => wsec_r <= arg32; wxor_r <= (others => '0');
                            when others => null;
                        end case;
                        if to_integer(unsigned(cmd_idx)) /= 55 then app_next <= false; end if;
                    else
                        cmd_cnt <= cmd_cnt + 1;
                    end if;
                end if;

                -- ---- response generator ------------------------------------
                next_tx := x"FF";
                case resp is
                    when R_GAP =>
                        resp <= R_R1;
                    when R_R1 =>
                        if cmd_idx = "101001" then next_tx := x"00";
                        elsif cmd_idx = "111010" then next_tx := x"00";
                        elsif cmd_idx = "010001" then next_tx := x"00";
                        elsif cmd_idx = "011000" then
                            if FAIL_WRITE then next_tx := x"04";   -- illegal cmd
                            else next_tx := x"00"; end if;
                        else next_tx := x"01"; end if;
                        if cmd_idx = "001000" or cmd_idx = "111010" then
                            resp <= R_EXTRA; resp_cnt <= 0;
                        elsif cmd_idx = "010001" then
                            resp <= R_TOKGAP; resp_cnt <= 0;
                        elsif cmd_idx = "011000" and not FAIL_WRITE then
                            resp <= W_TOK;
                        else
                            resp <= R_IDLE;
                        end if;
                    -- ---- CMD24 write capture ----
                    -- Command parsing must NOT run here: payload bytes can look
                    -- exactly like a command (bits 7:6 = "01").
                    when W_TOK =>
                        if rxb = x"FE" then resp <= W_RECV; wcnt <= 0; end if;
                    when W_RECV =>
                        wxor_r <= wxor_r xor (x"000000" & rxb);
                        if wcnt = 511 then resp <= W_CRC; resp_cnt <= 0;
                        else wcnt <= wcnt + 1; end if;
                    when W_CRC =>
                        if resp_cnt = 1 then resp <= W_RESP;
                        else resp_cnt <= resp_cnt + 1; end if;
                    when W_RESP =>
                        next_tx := x"05";               -- data accepted
                        resp <= W_BUSY; resp_cnt <= 0;
                    when W_BUSY =>
                        if resp_cnt = 2 then
                            next_tx := x"FF"; resp <= R_IDLE;   -- released
                        else
                            next_tx := x"00";                   -- programming
                            resp_cnt <= resp_cnt + 1;
                        end if;

                    when R_EXTRA =>
                        case resp_cnt is
                            when 0 => next_tx := extra(31 downto 24);
                            when 1 => next_tx := extra(23 downto 16);
                            when 2 => next_tx := extra(15 downto 8);
                            when others => next_tx := extra(7 downto 0);
                        end case;
                        if resp_cnt = 3 then resp <= R_IDLE; else resp_cnt <= resp_cnt + 1; end if;
                    when R_TOKGAP =>
                        next_tx := x"FF";
                        if resp_cnt = 1 then resp <= R_TOKEN; else resp_cnt <= resp_cnt + 1; end if;
                    when R_TOKEN =>
                        next_tx := x"FE"; resp <= R_DATA; resp_cnt <= 0;
                    when R_DATA =>
                        bidx := to_integer(unsigned(arg32(20 downto 0)))*512 + resp_cnt;
                        if bidx < SECTORS*512 then
                            next_tx := std_logic_vector(to_unsigned(image(bidx), 8));
                        else
                            next_tx := x"00";        -- reads past the image return zeros
                        end if;
                        if resp_cnt = 511 then resp <= R_CRC; resp_cnt <= 0;
                        else resp_cnt <= resp_cnt + 1; end if;
                    when R_CRC =>
                        next_tx := x"FF";
                        if resp_cnt = 1 then resp <= R_IDLE; else resp_cnt <= resp_cnt + 1; end if;
                    when others =>
                        next_tx := x"FF";
                end case;
                tx_sr <= next_tx;
            else
                bitcnt <= bitcnt + 1;
            end if;

        elsif falling_edge(sclk) then
            if cs = '0' then
                miso  <= tx_sr(7);
                tx_sr <= tx_sr(6 downto 0) & '1';
            end if;
        end if;
    end process;

    dbg_wsec  <= wsec_r;
    dbg_wxor  <= wxor_r;
    dbg_wdone <= '1' when resp = W_RESP else '0';

end sim;

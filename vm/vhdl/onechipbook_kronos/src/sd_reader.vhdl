library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Minimal SPI-mode SD / SDHC / SDXC sector reader (SPI bit-banged).
--
-- Initialises the card in SPI mode (CMD0 / CMD8 / ACMD41 / CMD58) and reads
-- 512-byte sectors with CMD17, streaming the bytes out one per strobe. Supports
-- SDv2 / SDHC (block addressing) and SDv1 / SDSC (byte addressing). If CMD8 is
-- rejected (v1 card) ACMD41 is sent with arg 0 (HCS/window bits are reserved
-- there); if ACMD41 gets nowhere for ACMD_MAX/2 rounds, falls back to CMD1.
--
-- OneChipBook SD pins (see kronos_onechip.qsf): SCLK=63, MOSI/CMD=64,
-- MISO/DAT0=62, CS/DAT3=65.
-- SPI clock = clk / (2*CLK_DIV); at the 21.477 MHz system clock, CLK_DIV=64 =>
-- ~168 kHz (safe for the 400 kHz init limit; raise later for read speed).
-- mode-0 SPI (CPOL=0,CPHA=0).

entity sd_reader is
    generic (
        CLK_DIV      : integer := 64;
        CLK_DIV_FAST : integer := 8;    -- faster SPI divider used AFTER init (data reads);
                                        -- init MUST stay slow (<=400kHz) but reads may go fast
        POLL_MAX : integer := 1000000;  -- response poll timeout (SPI bytes) before error
        ACMD_MAX : integer := 5000      -- ACMD41 init retries before error (~5 s @ 168 kHz)
    );
    port (
        clk        : in  std_logic;                      -- system clock
        reset      : in  std_logic;                      -- active high
        rd_req     : in  std_logic;                      -- pulse to read sector `lba`
        -- write side (CMD24): pulse wr_req, then feed 512 bytes -- the reader
        -- pulls each one by strobing wr_next, and wr_data must be valid when it
        -- does. SPI is host-clocked, so stalling between bytes is harmless: the
        -- card simply waits, which is why no 512-byte buffer is needed.
        wr_req     : in  std_logic := '0';
        wr_data    : in  std_logic_vector(7 downto 0) := (others => '0');
        -- '0' means the source has no byte ready yet. SPI is host-clocked, so
        -- simply not clocking is safe -- the card waits. Without this the
        -- reader re-loaded the previous (already-shifted) byte while the CPU
        -- was still fetching the next word, and the card received one wrong
        -- byte per word boundary.
        wr_valid   : in  std_logic := '1';
        wr_next    : out std_logic;                      -- 1-cycle strobe per byte consumed
        lba        : in  std_logic_vector(31 downto 0);
        data       : out std_logic_vector(7 downto 0);
        data_valid : out std_logic;                      -- 1-cycle strobe per byte (512/sector)
        ready      : out std_logic;                      -- init done, idle, accepts rd_req
        error      : out std_logic;                      -- init or read failed (sticky)
        sd_clk     : out std_logic;
        sd_cs      : out std_logic;
        sd_mosi    : out std_logic;
        sd_miso    : in  std_logic;
        -- debug taps (leave unconnected in normal use)
        dbg_ph     : out std_logic_vector(2 downto 0);  -- init phase (freezes at failure point)
        dbg_r1     : out std_logic_vector(7 downto 0);  -- last R1 response
        dbg_flags  : out std_logic_vector(5 downto 0);  -- sticky per-command init results:
                                                        -- 0 CMD0 R1=01, 1 CMD8 R1=01, 2 CMD8 illegal,
                                                        -- 3 CMD8 CRC err, 4 CMD8 R7 echo ok, 5 CMD55 R1=01
        dbg_acmd_r1 : out std_logic_vector(7 downto 0); -- last R1 from ACMD41
        dbg_cmd1_r1 : out std_logic_vector(7 downto 0)  -- last R1 from CMD1 (FF = never sent)
    );
end sd_reader;

architecture rtl of sd_reader is

    -- SPI half-period tick
    signal clkdiv_cnt : integer range 0 to CLK_DIV-1 := 0;
    -- Range must cover BOTH dividers: div_lim takes CLK_DIV_FAST-1 after init,
    -- which overflows a 0..CLK_DIV-1 range whenever CLK_DIV <= CLK_DIV_FAST
    -- (a fast simulation divider, for instance) -- a bound-check failure that
    -- has nothing to do with the caller's actual mistake.
    signal div_lim    : integer range 0 to (CLK_DIV + CLK_DIV_FAST) := CLK_DIV-1;
    signal spi_tick   : std_logic := '0';
    signal sclk_r     : std_logic := '0';

    -- byte-level SPI engine (mode 0): shift tx_byte out, capture rx_byte
    signal spi_start  : std_logic := '0';
    signal spi_busy   : std_logic := '0';
    signal spi_done   : std_logic := '0';
    signal tx_byte    : std_logic_vector(7 downto 0) := (others => '1');
    signal rx_byte    : std_logic_vector(7 downto 0) := (others => '0');
    signal bit_cnt    : integer range 0 to 7 := 0;
    signal phase      : std_logic := '0';

    -- control FSM
    type state_t is (
        ST_RESET, ST_POWERUP, ST_SEND, ST_RESP, ST_AFTER_R1,
        ST_EXTRA, ST_TOKEN, ST_DATA, ST_CRC, ST_NEXT, ST_IDLE, ST_FAIL,
        -- CMD24 write: gap byte, 0xFE start token, 512 data bytes, 2 CRC
        -- bytes, the data-response token, then wait out the card's busy
        ST_WPRE, ST_WTOKEN, ST_WDATA, ST_WGAP, ST_WCRC, ST_WRESP, ST_WBUSY);
    signal state    : state_t := ST_RESET;

    -- what to do after the current command's R1 comes back
    type kind_t is (K_INIT_NEXT, K_EXTRA_NEXT, K_ACMD41, K_READ, K_WRITE);
    signal kind     : kind_t := K_INIT_NEXT;

    signal cmd_idx  : std_logic_vector(5 downto 0) := (others => '0');
    signal cmd_arg  : std_logic_vector(31 downto 0) := (others => '0');
    signal cmd_crc  : std_logic_vector(7 downto 0) := (others => '0');
    signal cmd_step : integer range 0 to 5 := 0;
    signal r1       : std_logic_vector(7 downto 0) := (others => '1');
    signal extra    : std_logic_vector(31 downto 0) := (others => '0');
    signal extra_n  : integer range 0 to 4 := 0;

    signal init_ph  : integer range 0 to 5 := 0;   -- CMD0,CMD8,CMD55,ACMD41,CMD58,done
    signal byte_cnt : integer range 0 to 511 := 0;
    signal crc_cnt  : integer range 0 to 7 := 0;
    signal wgap_cnt : integer range 0 to 3 := 0;  -- 2 CRC bytes + extra trailing clocks
    signal poll_cnt : integer range 0 to 2000000 := 0;
    signal acmd_cnt : integer range 0 to 2000000 := 0;
    signal sdhc     : std_logic := '0';
    signal card_v2  : std_logic := '1';  -- CMD8 accepted (0 = v1/legacy card)
    signal use_cmd1 : std_logic := '0';  -- ACMD41 gave up -> legacy CMD1 init
    signal powerup  : integer range 0 to 9 := 0;
    signal err_r    : std_logic := '0';
    signal cs_r     : std_logic := '1';
    signal dbg_f    : std_logic_vector(5 downto 0) := (others => '0');
    signal acmd_r1_r : std_logic_vector(7 downto 0) := (others => '1');
    signal cmd1_r1_r : std_logic_vector(7 downto 0) := (others => '1');

    -- launch a command: load idx/arg/crc + the "kind" of post-R1 handling
    procedure load_cmd(signal s_idx : out std_logic_vector(5 downto 0);
                       signal s_arg : out std_logic_vector(31 downto 0);
                       signal s_crc : out std_logic_vector(7 downto 0);
                       idx : std_logic_vector(5 downto 0);
                       arg : std_logic_vector(31 downto 0);
                       crc : std_logic_vector(7 downto 0)) is
    begin
        s_idx <= idx; s_arg <= arg; s_crc <= crc;
    end procedure;

begin
    sd_clk <= sclk_r;
    sd_cs  <= cs_r;
    error  <= err_r;
    dbg_ph <= std_logic_vector(to_unsigned(init_ph, 3));
    dbg_r1 <= r1;
    dbg_flags <= dbg_f;
    dbg_acmd_r1 <= acmd_r1_r;
    dbg_cmd1_r1 <= cmd1_r1_r;

    -- SPI speed: slow (CLK_DIV) through init, fast (CLK_DIV_FAST) once init completes
    -- (init_ph reaches 5). A card re-init resets init_ph to 0 -> reverts to slow.
    div_lim <= CLK_DIV_FAST - 1 when init_ph = 5 else CLK_DIV - 1;

    -- SPI half-period tick
    process (clk)
    begin
        if rising_edge(clk) then
            if reset = '1' then
                clkdiv_cnt <= 0; spi_tick <= '0';
            elsif clkdiv_cnt >= div_lim then
                clkdiv_cnt <= 0; spi_tick <= '1';
            else
                clkdiv_cnt <= clkdiv_cnt + 1; spi_tick <= '0';
            end if;
        end if;
    end process;

    -- byte SPI engine
    process (clk)
    begin
        if rising_edge(clk) then
            spi_done <= '0';
            if reset = '1' then
                spi_busy <= '0'; sclk_r <= '0'; sd_mosi <= '1';
                bit_cnt <= 0; phase <= '0';
            elsif spi_busy = '0' then
                sclk_r <= '0';
                if spi_start = '1' then
                    spi_busy <= '1'; bit_cnt <= 7; phase <= '0';
                    sd_mosi  <= tx_byte(7);
                end if;
            elsif spi_tick = '1' then
                if phase = '0' then
                    sclk_r  <= '1';                       -- rising edge: card samples MOSI
                    rx_byte <= rx_byte(6 downto 0) & sd_miso;  -- master samples MISO
                    phase   <= '1';
                else
                    sclk_r <= '0';                        -- falling edge: present next bit
                    phase  <= '0';
                    if bit_cnt = 0 then
                        spi_busy <= '0'; spi_done <= '1';
                    else
                        bit_cnt <= bit_cnt - 1;
                        sd_mosi <= tx_byte(bit_cnt - 1);
                    end if;
                end if;
            end if;
        end if;
    end process;

    -- control FSM
    process (clk)
    begin
        if rising_edge(clk) then
            spi_start  <= '0';
            data_valid <= '0';
            wr_next    <= '0';        -- both are 1-cycle strobes

            if reset = '1' then
                state <= ST_RESET; cs_r <= '1'; ready <= '0'; err_r <= '0';
                tx_byte <= x"FF"; init_ph <= 0; sdhc <= '0'; powerup <= 0;
            else
                case state is

                    when ST_RESET =>                       -- 80 clocks, CS high
                        cs_r <= '1'; ready <= '0'; err_r <= '0';
                        tx_byte <= x"FF"; powerup <= 0; acmd_cnt <= 0;
                        card_v2 <= '1'; use_cmd1 <= '0';
                        dbg_f <= (others => '0');
                        acmd_r1_r <= (others => '1'); cmd1_r1_r <= (others => '1');
                        spi_start <= '1'; state <= ST_POWERUP;
                    when ST_POWERUP =>
                        if spi_done = '1' then
                            if powerup = 9 then
                                cs_r <= '0'; init_ph <= 0; state <= ST_NEXT;
                            else
                                powerup <= powerup + 1; tx_byte <= x"FF"; spi_start <= '1';
                            end if;
                        end if;

                    -- pick the next command (init sequence, or a read when idle)
                    when ST_NEXT =>
                        cmd_step <= 0; r1 <= x"FF"; extra <= (others => '0');
                        case init_ph is
                            when 0 => load_cmd(cmd_idx,cmd_arg,cmd_crc,"000000",x"00000000",x"95");  -- CMD0
                                      kind <= K_INIT_NEXT; extra_n <= 0; state <= ST_SEND;
                            when 1 => load_cmd(cmd_idx,cmd_arg,cmd_crc,"001000",x"000001AA",x"87");  -- CMD8
                                      kind <= K_EXTRA_NEXT; extra_n <= 4; state <= ST_SEND;
                            when 2 => if use_cmd1 = '1' then                                         -- legacy CMD1 init (last resort)
                                          load_cmd(cmd_idx,cmd_arg,cmd_crc,"000001",x"00000000",x"F9");
                                          kind <= K_ACMD41;
                                      else                                                           -- CMD55 (ACMD prefix)
                                          load_cmd(cmd_idx,cmd_arg,cmd_crc,"110111",x"00000000",x"65");
                                          kind <= K_INIT_NEXT;
                                      end if;
                                      extra_n <= 0; state <= ST_SEND;
                            when 3 => if card_v2 = '1' then  -- ACMD41: HCS + 2.7-3.6V window (v2)
                                          load_cmd(cmd_idx,cmd_arg,cmd_crc,"101001",x"40FF8000",x"17");
                                      else                   -- v1 card: HCS/window bits are RESERVED, must be 0
                                          load_cmd(cmd_idx,cmd_arg,cmd_crc,"101001",x"00000000",x"E5");
                                      end if;
                                      kind <= K_ACMD41; extra_n <= 0; state <= ST_SEND;
                            when 4 => load_cmd(cmd_idx,cmd_arg,cmd_crc,"111010",x"00000000",x"75");  -- CMD58
                                      kind <= K_EXTRA_NEXT; extra_n <= 4; state <= ST_SEND;
                            when others => ready <= '1'; state <= ST_IDLE;
                        end case;

                    when ST_IDLE =>
                        ready <= '1';
                        if wr_req = '1' then
                            ready <= '0'; cmd_step <= 0; r1 <= x"FF";
                            cmd_idx <= "011000";           -- CMD24 write single block
                            if sdhc = '1' then cmd_arg <= lba;
                            else cmd_arg <= std_logic_vector(shift_left(unsigned(lba), 9)); end if;
                            cmd_crc <= x"FF"; kind <= K_WRITE; extra_n <= 0;
                            state <= ST_SEND;
                        elsif rd_req = '1' then
                            ready <= '0'; cmd_step <= 0; r1 <= x"FF";
                            cmd_idx <= "010001";           -- CMD17
                            if sdhc = '1' then cmd_arg <= lba;
                            else cmd_arg <= std_logic_vector(shift_left(unsigned(lba), 9)); end if;
                            cmd_crc <= x"FF"; kind <= K_READ; extra_n <= 0;
                            state <= ST_SEND;
                        end if;

                    -- send the 6 command bytes
                    when ST_SEND =>
                        if spi_busy = '0' and spi_start = '0' and spi_done = '0' then
                            case cmd_step is
                                when 0 => tx_byte <= "01" & cmd_idx;
                                when 1 => tx_byte <= cmd_arg(31 downto 24);
                                when 2 => tx_byte <= cmd_arg(23 downto 16);
                                when 3 => tx_byte <= cmd_arg(15 downto 8);
                                when 4 => tx_byte <= cmd_arg(7 downto 0);
                                when others => tx_byte <= cmd_crc;
                            end case;
                            spi_start <= '1';
                        elsif spi_done = '1' then
                            if cmd_step = 5 then
                                poll_cnt <= 0; state <= ST_RESP;
                            else
                                cmd_step <= cmd_step + 1;
                            end if;
                        end if;

                    -- poll 0xFF until R1 (MSB=0) arrives
                    when ST_RESP =>
                        if spi_busy = '0' and spi_start = '0' and spi_done = '0' then
                            tx_byte <= x"FF"; spi_start <= '1';
                        elsif spi_done = '1' then
                            if rx_byte(7) = '0' then
                                r1 <= rx_byte; state <= ST_AFTER_R1;
                            elsif poll_cnt >= POLL_MAX then
                                err_r <= '1'; state <= ST_FAIL;
                            else
                                poll_cnt <= poll_cnt + 1;
                            end if;
                        end if;

                    when ST_AFTER_R1 =>
                        -- debug: record how each init command was answered
                        if init_ph = 0 and r1 = x"01" then dbg_f(0) <= '1'; end if;
                        if init_ph = 1 then
                            if r1 = x"01" then dbg_f(1) <= '1'; end if;
                            dbg_f(2) <= r1(2);           -- CMD8 illegal (SDv1 card)
                            dbg_f(3) <= r1(3);           -- CMD8 CRC rejected
                            card_v2  <= not r1(2);       -- v1/legacy: ACMD41 arg must be 0
                        end if;
                        if init_ph = 2 and use_cmd1 = '0' and r1 = x"01" then dbg_f(5) <= '1'; end if;
                        case kind is
                            when K_INIT_NEXT =>
                                init_ph <= init_ph + 1; state <= ST_NEXT;
                            when K_EXTRA_NEXT =>
                                extra <= (others => '0'); state <= ST_EXTRA;
                            when K_ACMD41 =>
                                -- debug: remember how each init command is answered
                                if use_cmd1 = '1' then cmd1_r1_r <= r1;
                                else acmd_r1_r <= r1; end if;
                                if r1 = x"00" then
                                    init_ph <= 4; state <= ST_NEXT;  -- card ready -> CMD58
                                elsif acmd_cnt >= ACMD_MAX then
                                    err_r <= '1'; state <= ST_FAIL;  -- card never left idle
                                else
                                    acmd_cnt <= acmd_cnt + 1;
                                    -- halfway through the budget, give up on ACMD41 and
                                    -- fall back to legacy CMD1 init (oldest cards)
                                    if acmd_cnt >= ACMD_MAX/2 then use_cmd1 <= '1'; end if;
                                    init_ph <= 2; state <= ST_NEXT;  -- retry CMD55+ACMD41 / CMD1
                                end if;
                            when K_READ =>
                                if r1 = x"00" then poll_cnt <= 0; state <= ST_TOKEN;
                                else err_r <= '1'; state <= ST_FAIL; end if;
                            when K_WRITE =>
                                if r1 = x"00" then state <= ST_WPRE;
                                else err_r <= '1'; state <= ST_FAIL; end if;
                        end case;

                    -- read extra_n trailing bytes (R3/R7), then advance init
                    when ST_EXTRA =>
                        if spi_busy = '0' and spi_start = '0' and spi_done = '0' then
                            tx_byte <= x"FF"; spi_start <= '1';
                        elsif spi_done = '1' then
                            extra <= extra(23 downto 0) & rx_byte;
                            -- CCS = OCR bit 30 = bit 6 of the FIRST OCR byte (byte0)
                            if init_ph = 4 and extra_n = 4 then sdhc <= rx_byte(6); end if;
                            -- debug: CMD8 R7 = [ver][rsvd][voltage=01][echo=AA]; at the last
                            -- byte, extra(7:0) holds byte3 (voltage) and rx_byte byte4 (echo)
                            if init_ph = 1 and extra_n = 1
                               and extra(7 downto 0) = x"01" and rx_byte = x"AA" then
                                dbg_f(4) <= '1';
                            end if;
                            if extra_n = 1 then
                                init_ph <= init_ph + 1; state <= ST_NEXT;
                            else
                                extra_n <= extra_n - 1;
                            end if;
                        end if;

                    -- poll for the data start token 0xFE
                    when ST_TOKEN =>
                        if spi_busy = '0' and spi_start = '0' and spi_done = '0' then
                            tx_byte <= x"FF"; spi_start <= '1';
                        elsif spi_done = '1' then
                            if rx_byte = x"FE" then
                                byte_cnt <= 0; state <= ST_DATA;
                            elsif poll_cnt >= POLL_MAX then
                                err_r <= '1'; state <= ST_FAIL;
                            else
                                poll_cnt <= poll_cnt + 1;
                            end if;
                        end if;

                    -- stream 512 data bytes
                    when ST_DATA =>
                        if spi_busy = '0' and spi_start = '0' and spi_done = '0' then
                            tx_byte <= x"FF"; spi_start <= '1';
                        elsif spi_done = '1' then
                            data <= rx_byte; data_valid <= '1';
                            if byte_cnt = 511 then crc_cnt <= 0; state <= ST_CRC;
                            else byte_cnt <= byte_cnt + 1; end if;
                        end if;

                    -- consume the two CRC bytes
                    -- read the 2 CRC bytes, then send extra trailing 0xFF clocks so
                    -- the card releases the bus before the next command (some cards
                    -- hold the line for a few bytes after a read).
                    when ST_CRC =>
                        if spi_busy = '0' and spi_start = '0' and spi_done = '0' then
                            tx_byte <= x"FF"; spi_start <= '1';
                        elsif spi_done = '1' then
                            if crc_cnt = 5 then ready <= '1'; state <= ST_IDLE;
                            else crc_cnt <= crc_cnt + 1; end if;
                        end if;

                    -- ---------------- CMD24 write ----------------
                    -- one idle byte between the response and the data token
                    when ST_WPRE =>
                        if spi_busy = '0' and spi_start = '0' and spi_done = '0' then
                            tx_byte <= x"FF"; spi_start <= '1';
                        elsif spi_done = '1' then
                            state <= ST_WTOKEN;
                        end if;

                    when ST_WTOKEN =>
                        if spi_busy = '0' and spi_start = '0' and spi_done = '0' then
                            tx_byte <= x"FE"; spi_start <= '1';   -- start block
                        elsif spi_done = '1' then
                            byte_cnt <= 0; state <= ST_WDATA;
                        end if;

                    when ST_WDATA =>
                        if spi_busy = '0' and spi_start = '0' and spi_done = '0'
                           and wr_valid = '1' then
                            tx_byte <= wr_data; spi_start <= '1';
                        elsif spi_done = '1' then
                            wr_next <= '1';                 -- consumed: advance source
                            if byte_cnt = 511 then crc_cnt <= 0; state <= ST_WCRC;
                            else byte_cnt <= byte_cnt + 1; wgap_cnt <= 0; state <= ST_WGAP; end if;
                        end if;

                    -- Let the source actually advance before loading the next
                    -- byte. wr_next is registered here, so the source only sees
                    -- it a cycle later and updates the cycle after that --
                    -- reloading immediately re-sent the same byte, which put an
                    -- extra byte at the head of the block and pushed the last
                    -- one off the end. SPI is host-clocked, so idling here is
                    -- free.
                    when ST_WGAP =>
                        if wgap_cnt = 3 then state <= ST_WDATA;
                        else wgap_cnt <= wgap_cnt + 1; end if;

                    -- CRC is not checked in SPI mode unless explicitly enabled
                    when ST_WCRC =>
                        if spi_busy = '0' and spi_start = '0' and spi_done = '0' then
                            tx_byte <= x"FF"; spi_start <= '1';
                        elsif spi_done = '1' then
                            if crc_cnt = 1 then poll_cnt <= 0; state <= ST_WRESP;
                            else crc_cnt <= crc_cnt + 1; end if;
                        end if;

                    -- data response: xxx0sss1, sss=010 means the block was accepted
                    when ST_WRESP =>
                        if spi_busy = '0' and spi_start = '0' and spi_done = '0' then
                            tx_byte <= x"FF"; spi_start <= '1';
                        elsif spi_done = '1' then
                            if (rx_byte and x"11") = x"01" then
                                if (rx_byte and x"1F") = x"05" then
                                    poll_cnt <= 0; state <= ST_WBUSY;
                                else
                                    err_r <= '1'; state <= ST_FAIL;   -- CRC/write error
                                end if;
                            elsif poll_cnt >= POLL_MAX then
                                err_r <= '1'; state <= ST_FAIL;
                            else
                                poll_cnt <= poll_cnt + 1;
                            end if;
                        end if;

                    -- the card holds MISO low while it programs the block
                    when ST_WBUSY =>
                        if spi_busy = '0' and spi_start = '0' and spi_done = '0' then
                            tx_byte <= x"FF"; spi_start <= '1';
                        elsif spi_done = '1' then
                            if rx_byte /= x"00" then
                                ready <= '1'; state <= ST_IDLE;
                            elsif poll_cnt >= POLL_MAX then
                                err_r <= '1'; state <= ST_FAIL;
                            else
                                poll_cnt <= poll_cnt + 1;
                            end if;
                        end if;

                    when ST_FAIL =>
                        ready <= '0'; err_r <= '1';        -- assert reset to retry

                    when others =>
                        state <= ST_RESET;
                end case;
            end if;
        end if;
    end process;

end rtl;

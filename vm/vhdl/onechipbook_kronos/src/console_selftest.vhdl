library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use ieee.std_logic_arith.all;

-- Drive the VGA console from a canned byte sequence instead of the OS.
--
-- The ex editor blanks the screen on the first scroll ON HARDWARE, but the
-- same console fed the REAL captured OS byte stream renders all 50 rows
-- correctly in simulation. Something differs between the board and the
-- simulator, and no amount of reading the RTL has found it -- so let the board
-- answer directly.
--
-- The sequence fills every row with its own number, then scrolls the way ex
-- does. It is emitted at the DL11's byte rate, so the console sees exactly the
-- timing it sees in service.
--
--   ESCJ='0'  scroll WITHOUT the ESC J  (ESC[50;1H, LF, write bottom line)
--   ESCJ='1'  scroll WITH    the ESC J  (what ex actually sends)
--
-- Expected picture, verified in tb_selftest for BOTH variants (they render
-- identically -- 49 of 50 rows lit -- which is what makes any DIFFERENCE
-- between the two switch positions on hardware meaningful):
--
--   rows  0..44   ROW 05 =============== .. ROW 49 ===============
--   rows 45..48   SCROLL-1 * .. SCROLL-4 *      (the four rolled-in lines)
--   row  49       blank (ex's status line; the self-test never redraws it)
--
-- Read the result like this:
--   both variants scroll correctly      -> the console is fine on silicon, and
--                                          the OS must be sending something
--                                          other than what the VM sends
--   ESCJ=0 scrolls, ESCJ=1 blanks       -> ESC J erases too much on hardware
--   neither works / nothing appears     -> the board is not running this build

entity console_selftest is
    generic (
        -- clk cycles between bytes: one 10-bit frame at 57600 baud, i.e. the
        -- rate the console is fed in service (21.47727 MHz / 57600 * 10)
        GAP : integer := 3730);
    port (
        clk    : in  std_logic;
        reset  : in  std_logic;
        enable : in  std_logic;                      -- run the test
        escj   : in  std_logic;                      -- include the ESC J
        byte   : out std_logic_vector(7 downto 0);
        stb    : out std_logic);
end console_selftest;

architecture rtl of console_selftest is

    constant ROWS   : integer := 49;      -- text rows ex uses (50th = status)
    constant NSCROLL: integer := 4;

    constant POS_LEN : integer := 7;      -- ESC [ d d ; 1 H
    constant TXT_LEN : integer := 25;     -- ROW dd ==================
    constant SCR_LEN : integer := 30;     -- see scr_byte below

    type st_t is (S_IDLE, S_CLR, S_POS, S_TXT, S_SCR, S_DONE);
    signal st   : st_t := S_IDLE;
    signal idx  : integer range 0 to 63 := 0;
    signal row  : integer range 1 to 63 := 1;
    signal scn  : integer range 0 to 15 := 1;
    signal tick : integer range 0 to GAP := 0;

    signal b_out : std_logic_vector(7 downto 0) := (others => '0');
    signal b_stb : std_logic := '0';

    function ch(c : character) return std_logic_vector is
    begin
        return conv_std_logic_vector(character'pos(c), 8);
    end function;

    function dig(n : integer) return std_logic_vector is
    begin
        return conv_std_logic_vector(48 + (n mod 10), 8);
    end function;

    constant ESC : std_logic_vector(7 downto 0) := x"1B";
    constant LFC : std_logic_vector(7 downto 0) := x"0A";

    -- ESC [ H  ESC J   -- home, then erase to end of screen = full clear
    function clr_byte(i : integer) return std_logic_vector is
    begin
        case i is
            when 0 => return ESC;       when 1 => return ch('[');
            when 2 => return ch('H');   when 3 => return ESC;
            when others => return ch('J');
        end case;
    end function;

    -- ESC [ <row> ; 1 H     (row is 1-based, always two digits)
    function pos_byte(i, r : integer) return std_logic_vector is
    begin
        case i is
            when 0 => return ESC;          when 1 => return ch('[');
            when 2 => return dig(r / 10);  when 3 => return dig(r);
            when 4 => return ch(';');      when 5 => return ch('1');
            when others => return ch('H');
        end case;
    end function;

    -- "ROW dd " then a run of '=' so the row is unmistakably non-blank
    function txt_byte(i, r : integer) return std_logic_vector is
    begin
        case i is
            when 0 => return ch('R');      when 1 => return ch('O');
            when 2 => return ch('W');      when 3 => return ch(' ');
            when 4 => return dig(r / 10);  when 5 => return dig(r);
            when 6 => return ch(' ');
            when others => return ch('=');
        end case;
    end function;

    -- One ex-style scroll. Bytes 0..6 are the ESC J part and are skipped when
    -- escj='0', which is what makes this a controlled experiment.
    function scr_byte(i, n : integer) return std_logic_vector is
    begin
        case i is
            when  0 => return ESC;        when  1 => return ch('[');
            when  2 => return ch('5');    when  3 => return ch('0');
            when  4 => return ch('H');    when  5 => return ESC;
            when  6 => return ch('J');
            when  7 => return ESC;        when  8 => return ch('[');
            when  9 => return ch('5');    when 10 => return ch('0');
            when 11 => return ch(';');    when 12 => return ch('1');
            when 13 => return ch('H');    when 14 => return LFC;
            when 15 => return ESC;        when 16 => return ch('[');
            when 17 => return ch('4');    when 18 => return ch('9');
            when 19 => return ch('H');
            when 20 => return ch('S');    when 21 => return ch('C');
            when 22 => return ch('R');    when 23 => return ch('O');
            when 24 => return ch('L');    when 25 => return ch('L');
            when 26 => return ch('-');    when 27 => return dig(n);
            when 28 => return ch(' ');
            when others => return ch('*');
        end case;
    end function;

begin

    byte <= b_out;
    stb  <= b_stb;

    process (clk)
        variable start : integer range 0 to 63;
    begin
        if rising_edge(clk) then
            b_stb <= '0';
            if reset = '1' or enable = '0' then
                st <= S_IDLE; idx <= 0; row <= 1; scn <= 1; tick <= 0;
            else
                case st is
                    when S_IDLE =>
                        st <= S_CLR; idx <= 0; tick <= 0;

                    when S_DONE =>
                        null;                        -- hold the picture

                    when others =>
                        if tick = GAP then
                            tick <= 0;
                            b_stb <= '1';
                            case st is
                                when S_CLR => b_out <= clr_byte(idx);
                                when S_POS => b_out <= pos_byte(idx, row);
                                when S_TXT => b_out <= txt_byte(idx, row);
                                when others => b_out <= scr_byte(idx, scn);
                            end case;
                            -- advance
                            if st = S_CLR then
                                if idx = 4 then st <= S_POS; idx <= 0;
                                else idx <= idx + 1; end if;
                            elsif st = S_POS then
                                if idx = POS_LEN - 1 then st <= S_TXT; idx <= 0;
                                else idx <= idx + 1; end if;
                            elsif st = S_TXT then
                                if idx = TXT_LEN - 1 then
                                    idx <= 0;
                                    if row = ROWS then
                                        st <= S_SCR;
                                        if escj = '1' then idx <= 0; else idx <= 7; end if;
                                    else
                                        st <= S_POS; row <= row + 1;
                                    end if;
                                else
                                    idx <= idx + 1;
                                end if;
                            else                      -- S_SCR
                                if idx = SCR_LEN - 1 then
                                    if scn = NSCROLL then
                                        st <= S_DONE;
                                    else
                                        scn <= scn + 1;
                                        if escj = '1' then idx <= 0; else idx <= 7; end if;
                                    end if;
                                else
                                    idx <= idx + 1;
                                end if;
                            end if;
                        else
                            tick <= tick + 1;
                        end if;
                end case;
            end if;
        end if;
    end process;

end architecture rtl;

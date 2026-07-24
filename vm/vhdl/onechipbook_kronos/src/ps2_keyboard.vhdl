library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

-- PS/2 keyboard -> ASCII for the OneChipBook console (M5).
--
-- The board's built-in keyboard MCU speaks PS/2 to the FPGA on CLK 68 /
-- DATA 67, in parallel with the external PS/2 socket (FN+4 switches between
-- them), so one receiver serves both.
--
-- RECEIVE ONLY: the pins are plain inputs and we never drive them. A PS/2
-- keyboard powers up in scan code set 2 and starts sending unprompted, so no
-- host-to-device command is needed; not driving also removes any risk of
-- fighting the keyboard's open-collector outputs. (The cost is no LED control
-- and no way to force set 2 -- neither matters here.)
--
-- Frame: 11 bits, keyboard-clocked, sampled on the FALLING edge of PS2_CLK --
-- start(0), 8 data LSB first, odd parity, stop(1). Frames failing the
-- start/stop/parity check are DROPPED: a keyboard cable is an unshielded
-- external wire, and this board has already shown what happens when noise on a
-- floating input is trusted (the "0080h" console loop). A stuck mid-frame
-- receiver is recovered by an inter-bit timeout.
--
-- Set 2 uses 0xF0 as a key-RELEASE prefix and 0xE0 for extended keys, so codes
-- must be tracked as a small state machine, not decoded one byte at a time.

entity ps2_keyboard is
    generic (
        CLK_HZ : integer := 21477270);
    port (
        clk      : in  std_logic;
        reset    : in  std_logic;
        ps2_clk  : in  std_logic;
        ps2_data : in  std_logic;
        ascii    : out std_logic_vector(7 downto 0);
        stb      : out std_logic;       -- 1 cycle per translated key press
        -- diagnostic: a frame passed start/stop/parity, whatever it decoded to.
        -- Splits "keyboard is silent / miswired" from "keys arrive but the OS
        -- never sees them", which the LEDs alone cannot distinguish.
        frame_stb : out std_logic);
end ps2_keyboard;

architecture rtl of ps2_keyboard is

    -- ~250 us: a PS/2 bit period is 60..100 us, so this only fires when a frame
    -- has genuinely been abandoned part-way through
    constant TIMEOUT : integer := CLK_HZ / 4000;

    signal clk_s, dat_s   : std_logic_vector(1 downto 0) := (others => '1');
    signal clk_f          : std_logic_vector(7 downto 0) := (others => '1');
    signal clk_st, clk_st1 : std_logic := '1';

    signal bitc           : integer range 0 to 10 := 0;
    signal shifter        : std_logic_vector(10 downto 0) := (others => '0');
    signal idle_cnt       : integer range 0 to TIMEOUT := 0;

    signal code           : std_logic_vector(7 downto 0);
    signal code_stb       : std_logic := '0';

    -- decoder state
    signal is_break, is_ext : std_logic := '0';
    signal shift_l, shift_r, ctrl, caps : std_logic := '0';

    -- Scan code set 2 -> ASCII. Returns 0 for keys with no printable meaning,
    -- which the caller drops.
    function to_ascii(c : std_logic_vector(7 downto 0);
                      up : boolean) return std_logic_vector is
    begin
        if not up then
            case c is
                when x"1C" => return x"61";  -- a
                when x"32" => return x"62";  -- b
                when x"21" => return x"63";  -- c
                when x"23" => return x"64";  -- d
                when x"24" => return x"65";  -- e
                when x"2B" => return x"66";  -- f
                when x"34" => return x"67";  -- g
                when x"33" => return x"68";  -- h
                when x"43" => return x"69";  -- i
                when x"3B" => return x"6A";  -- j
                when x"42" => return x"6B";  -- k
                when x"4B" => return x"6C";  -- l
                when x"3A" => return x"6D";  -- m
                when x"31" => return x"6E";  -- n
                when x"44" => return x"6F";  -- o
                when x"4D" => return x"70";  -- p
                when x"15" => return x"71";  -- q
                when x"2D" => return x"72";  -- r
                when x"1B" => return x"73";  -- s
                when x"2C" => return x"74";  -- t
                when x"3C" => return x"75";  -- u
                when x"2A" => return x"76";  -- v
                when x"1D" => return x"77";  -- w
                when x"22" => return x"78";  -- x
                when x"35" => return x"79";  -- y
                when x"1A" => return x"7A";  -- z
                when x"16" => return x"31";  -- 1
                when x"1E" => return x"32";  -- 2
                when x"26" => return x"33";  -- 3
                when x"25" => return x"34";  -- 4
                when x"2E" => return x"35";  -- 5
                when x"36" => return x"36";  -- 6
                when x"3D" => return x"37";  -- 7
                when x"3E" => return x"38";  -- 8
                when x"46" => return x"39";  -- 9
                when x"45" => return x"30";  -- 0
                when x"4E" => return x"2D";  -- -
                when x"55" => return x"3D";  -- =
                when x"0E" => return x"60";  -- `
                when x"5D" => return x"5C";  -- backslash
                when x"54" => return x"5B";  -- [
                when x"5B" => return x"5D";  -- ]
                when x"4C" => return x"3B";  -- ;
                when x"52" => return x"27";  -- '
                when x"41" => return x"2C";  -- ,
                when x"49" => return x"2E";  -- .
                when x"4A" => return x"2F";  -- /
                when others => return x"00";
            end case;
        else
            case c is
                when x"1C" => return x"41";  -- A
                when x"32" => return x"42";
                when x"21" => return x"43";
                when x"23" => return x"44";
                when x"24" => return x"45";
                when x"2B" => return x"46";
                when x"34" => return x"47";
                when x"33" => return x"48";
                when x"43" => return x"49";
                when x"3B" => return x"4A";
                when x"42" => return x"4B";
                when x"4B" => return x"4C";
                when x"3A" => return x"4D";
                when x"31" => return x"4E";
                when x"44" => return x"4F";
                when x"4D" => return x"50";
                when x"15" => return x"51";
                when x"2D" => return x"52";
                when x"1B" => return x"53";
                when x"2C" => return x"54";
                when x"3C" => return x"55";
                when x"2A" => return x"56";
                when x"1D" => return x"57";
                when x"22" => return x"58";
                when x"35" => return x"59";
                when x"1A" => return x"5A";  -- Z
                when x"16" => return x"21";  -- !
                when x"1E" => return x"40";  -- @
                when x"26" => return x"23";  -- #
                when x"25" => return x"24";  -- $
                when x"2E" => return x"25";  -- %
                when x"36" => return x"5E";  -- ^
                when x"3D" => return x"26";  -- &
                when x"3E" => return x"2A";  -- *
                when x"46" => return x"28";  -- (
                when x"45" => return x"29";  -- )
                when x"4E" => return x"5F";  -- _
                when x"55" => return x"2B";  -- +
                when x"0E" => return x"7E";  -- ~
                when x"5D" => return x"7C";  -- |
                when x"54" => return x"7B";  -- {
                when x"5B" => return x"7D";  -- }
                when x"4C" => return x"3A";  -- :
                when x"52" => return x"22";  -- "
                when x"41" => return x"3C";  -- <
                when x"49" => return x"3E";  -- >
                when x"4A" => return x"3F";  -- ?
                when others => return x"00";
            end case;
        end if;
    end function;

    function is_letter(c : std_logic_vector(7 downto 0)) return boolean is
        variable a : std_logic_vector(7 downto 0);
    begin
        a := to_ascii(c, false);
        return a >= x"61" and a <= x"7A";
    end function;

begin

    frame_stb <= code_stb;

    -- ------------------------------------------------------ frame receiver
    process (clk)
        variable parity : std_logic;
    begin
        if rising_edge(clk) then
            code_stb <= '0';

            -- synchronise, then majority-free glitch filter: PS2_CLK is only
            -- ~15 kHz, so requiring 8 stable samples costs nothing and rejects
            -- the ringing a metre of keyboard cable picks up
            clk_s <= clk_s(0) & ps2_clk;
            dat_s <= dat_s(0) & ps2_data;
            clk_f <= clk_f(6 downto 0) & clk_s(1);
            if clk_f = x"FF" then
                clk_st <= '1';
            elsif clk_f = x"00" then
                clk_st <= '0';
            end if;
            clk_st1 <= clk_st;

            if reset = '1' then
                bitc <= 0; idle_cnt <= 0; clk_st1 <= '1';
            else
                -- abandon a half-received frame
                if bitc /= 0 then
                    if idle_cnt = TIMEOUT then
                        bitc <= 0; idle_cnt <= 0;
                    else
                        idle_cnt <= idle_cnt + 1;
                    end if;
                end if;

                if clk_st1 = '1' and clk_st = '0' then     -- falling edge
                    idle_cnt <= 0;
                    shifter <= dat_s(1) & shifter(10 downto 1);
                    if bitc = 10 then
                        bitc <= 0;
                        -- This edge carries the STOP bit, still on dat_s(1);
                        -- the assignment above has not landed yet, so `shifter`
                        -- holds the 10 bits already clocked in. Each shift puts
                        -- the new bit at 10 and pushes older ones down, so after
                        -- ten shifts:  start=1, d0..d7=2..9 (LSB first, so
                        -- shifter(9 downto 2) reads MSB..LSB), parity=10.
                        parity := shifter(9) xor shifter(8) xor shifter(7) xor
                                  shifter(6) xor shifter(5) xor shifter(4) xor
                                  shifter(3) xor shifter(2) xor shifter(10);
                        -- odd parity: data+parity must contain an odd number of 1s
                        if shifter(1) = '0' and dat_s(1) = '1' and parity = '1' then
                            code <= shifter(9 downto 2);
                            code_stb <= '1';
                        end if;
                    else
                        bitc <= bitc + 1;
                    end if;
                end if;
            end if;
        end if;
    end process;

    -- ---------------------------------------------------------- decoder
    process (clk)
        variable a  : std_logic_vector(7 downto 0);
        variable up : boolean;
    begin
        if rising_edge(clk) then
            stb <= '0';
            if reset = '1' then
                is_break <= '0'; is_ext <= '0';
                shift_l <= '0'; shift_r <= '0'; ctrl <= '0'; caps <= '0';
            elsif code_stb = '1' then
                if code = x"F0" then
                    is_break <= '1';
                elsif code = x"E0" then
                    is_ext <= '1';
                else
                    -- E0-prefixed keys. These were consumed and discarded,
                    -- which is why the file manager could not be navigated:
                    -- every arrow key is E0-prefixed. The OS expects its own
                    -- high-range codes (sys/lib/Keyboard.d), NOT ASCII:
                    --   up 200c dw 201c right 202c left 203c
                    --   pgup 204c pgdw 205c home 206c end 207c del 210c ins 211c
                    -- A release arrives as E0 F0 xx and must emit nothing.
                    if is_ext = '1' then
                        is_ext <= '0'; is_break <= '0';
                        if is_break = '0' then
                            case code is
                                when x"75" => ascii <= x"80"; stb <= '1';  -- up
                                when x"72" => ascii <= x"81"; stb <= '1';  -- down
                                when x"74" => ascii <= x"82"; stb <= '1';  -- right
                                when x"6B" => ascii <= x"83"; stb <= '1';  -- left
                                when x"7D" => ascii <= x"84"; stb <= '1';  -- pgup
                                when x"7A" => ascii <= x"85"; stb <= '1';  -- pgdn
                                when x"6C" => ascii <= x"86"; stb <= '1';  -- home
                                when x"69" => ascii <= x"87"; stb <= '1';  -- end
                                when x"71" => ascii <= x"88"; stb <= '1';  -- delete
                                when x"70" => ascii <= x"89"; stb <= '1';  -- insert
                                when x"5A" => ascii <= x"0D"; stb <= '1';  -- keypad Enter
                                when others => null;
                            end case;
                        end if;
                    elsif is_break = '1' then
                        is_break <= '0';
                        case code is
                            when x"12" => shift_l <= '0';
                            when x"59" => shift_r <= '0';
                            when x"14" => ctrl    <= '0';
                            when others => null;
                        end case;
                    else
                        case code is
                            when x"12" => shift_l <= '1';
                            when x"59" => shift_r <= '1';
                            when x"14" => ctrl    <= '1';
                            when x"58" => caps    <= not caps;
                            when x"5A" => ascii <= x"0D"; stb <= '1';  -- Enter
                            when x"66" => ascii <= x"08"; stb <= '1';  -- Backspace
                            when x"0D" => ascii <= x"09"; stb <= '1';  -- Tab
                            when x"76" => ascii <= x"1B"; stb <= '1';  -- Esc
                            -- F1..F10 -> the OS's 220c.. codes; the file
                            -- manager labels its menu with them
                            when x"05" => ascii <= x"90"; stb <= '1';  -- F1
                            when x"06" => ascii <= x"91"; stb <= '1';  -- F2
                            when x"04" => ascii <= x"92"; stb <= '1';  -- F3
                            when x"0C" => ascii <= x"93"; stb <= '1';  -- F4
                            when x"03" => ascii <= x"94"; stb <= '1';  -- F5
                            when x"0B" => ascii <= x"95"; stb <= '1';  -- F6
                            when x"83" => ascii <= x"96"; stb <= '1';  -- F7
                            when x"0A" => ascii <= x"97"; stb <= '1';  -- F8
                            when x"01" => ascii <= x"98"; stb <= '1';  -- F9
                            when x"09" => ascii <= x"99"; stb <= '1';  -- F10
                            when x"29" => ascii <= x"20"; stb <= '1';  -- Space
                            when others =>
                                -- Caps Lock applies to letters only, and XORs
                                -- with Shift so Shift+letter is lower case
                                up := (shift_l or shift_r) = '1';
                                if caps = '1' and is_letter(code) then
                                    up := not up;
                                end if;
                                a := to_ascii(code, up);
                                if ctrl = '1' and is_letter(code) then
                                    -- Ctrl-A..Ctrl-Z -> 0x01..0x1A, so a shell
                                    -- gets Ctrl-C / Ctrl-D.
                                    -- Assign through `a` first: to_ascii returns
                                    -- a hex literal, which is an ASCENDING
                                    -- vector, and slicing that (4 downto 0)
                                    -- is a direction mismatch.
                                    a := to_ascii(code, false);
                                    a := "000" & a(4 downto 0);
                                end if;
                                if a /= x"00" then
                                    ascii <= a;
                                    stb   <= '1';
                                end if;
                        end case;
                    end if;
                end if;
            end if;
        end if;
    end process;

end architecture rtl;

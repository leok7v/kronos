library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

-- Drives real PS/2 frames at a realistic ~15 kHz and checks the ASCII that
-- comes out. Covers the cases that actually break keyboards: shift, the 0xF0
-- release prefix (a released Shift must stop shifting), Caps Lock XORing with
-- Shift, Ctrl folding to a control code, the 0xE0 extended prefix being
-- swallowed, and a bad-parity frame being DROPPED rather than delivered.

entity tb_ps2_keyboard is
end tb_ps2_keyboard;

architecture sim of tb_ps2_keyboard is
    constant PERIOD : time := 46561 ps;          -- 21.47727 MHz

    signal clk      : std_logic := '0';
    signal reset    : std_logic := '1';
    signal ps2_clk  : std_logic := '1';
    signal ps2_data : std_logic := '1';
    signal ascii    : std_logic_vector(7 downto 0);
    signal stb      : std_logic;
    signal done     : boolean := false;

    -- last key delivered, latched for the checker
    signal got      : std_logic_vector(7 downto 0) := (others => '0');
    signal got_n    : integer := 0;
begin
    clk <= not clk after PERIOD / 2 when not done else '0';

    dut : entity work.ps2_keyboard
        port map (clk => clk, reset => reset, ps2_clk => ps2_clk,
                  ps2_data => ps2_data, ascii => ascii, stb => stb);

    catch : process (clk)
    begin
        if rising_edge(clk) then
            if stb = '1' then
                got   <= ascii;
                got_n <= got_n + 1;
            end if;
        end if;
    end process;

    stim : process
        variable n : integer := 0;

        -- one PS/2 frame: start, 8 data LSB first, odd parity, stop.
        -- `bad` corrupts the parity bit so the receiver must reject the frame.
        procedure frame(b : std_logic_vector(7 downto 0); bad : boolean) is
            variable par : std_logic;
            variable bits : std_logic_vector(0 to 10);
        begin
            par := not (b(0) xor b(1) xor b(2) xor b(3) xor
                        b(4) xor b(5) xor b(6) xor b(7));
            if bad then par := not par; end if;
            bits := '0' & b(0) & b(1) & b(2) & b(3) & b(4) & b(5) & b(6) & b(7)
                    & par & '1';
            for i in 0 to 10 loop
                ps2_data <= bits(i);
                wait for 20 us;                 -- data set up before the edge
                ps2_clk  <= '0';                -- host samples on this edge
                wait for 30 us;
                ps2_clk  <= '1';
                wait for 16 us;
            end loop;
            ps2_data <= '1';
            wait for 60 us;
        end procedure;

        procedure send(b : std_logic_vector(7 downto 0)) is
        begin
            frame(b, false);
        end procedure;

        procedure expect(c : std_logic_vector(7 downto 0); what : string) is
        begin
            assert got_n = n + 1
                report "FAIL: " & what & " produced " &
                       integer'image(got_n - n) & " keys, expected 1"
                severity failure;
            assert got = c
                report "FAIL: " & what & " gave " &
                       integer'image(conv_integer(got)) & " (dec), expected " &
                       integer'image(conv_integer(c))
                severity failure;
            report "ok: " & what & " -> " & integer'image(conv_integer(got)) & " dec" &
                   " '" & character'val(conv_integer(got)) & "'" severity note;
            n := got_n;
        end procedure;

        procedure expect_none(what : string) is
        begin
            assert got_n = n
                report "FAIL: " & what & " should have produced no key, got " &
                       integer'image(got_n - n) severity failure;
            report "ok: " & what & " -> no key (correctly ignored)" severity note;
        end procedure;
    begin
        wait for PERIOD * 20;
        reset <= '0';
        wait for 100 us;

        send(x"1C");                        expect(x"61", "'a'");
        send(x"12"); send(x"1C");           expect(x"41", "Shift+a");
        send(x"F0"); send(x"12");           -- Shift released
        send(x"1C");                        expect(x"61", "'a' after Shift release");
        send(x"5A");                        expect(x"0D", "Enter");
        send(x"66");                        expect(x"08", "Backspace");
        send(x"29");                        expect(x"20", "Space");
        send(x"14"); send(x"21");           expect(x"03", "Ctrl+c");
        send(x"F0"); send(x"14");           -- Ctrl released
        send(x"58"); send(x"1C");           expect(x"41", "CapsLock then 'a'");
        send(x"12"); send(x"1C");           expect(x"61", "Caps+Shift+a (XOR)");
        send(x"F0"); send(x"12");
        send(x"58");                        -- Caps off again
        send(x"16");                        expect(x"31", "'1'");
        send(x"12"); send(x"16");           expect(x"21", "Shift+1 = '!'");
        send(x"F0"); send(x"12");

        -- a released key must not emit anything
        n := got_n;
        send(x"F0"); send(x"1C");           expect_none("key release");

        -- ARROWS: E0-prefixed, and the OS wants its own codes, not ASCII
        send(x"E0"); send(x"75");           expect(x"80", "Up arrow");
        send(x"E0"); send(x"72");           expect(x"81", "Down arrow");
        send(x"E0"); send(x"74");           expect(x"82", "Right arrow");
        send(x"E0"); send(x"6B");           expect(x"83", "Left arrow");
        send(x"E0"); send(x"7D");           expect(x"84", "PgUp");
        -- a released arrow (E0 F0 75) must emit nothing
        n := got_n;
        send(x"E0"); send(x"F0"); send(x"75"); expect_none("arrow release");
        send(x"05");                        expect(x"90", "F1");

        -- a corrupted frame must be dropped, not delivered as a keystroke
        frame(x"1C", true);                 expect_none("bad-parity frame");

        -- and the receiver must still work afterwards
        send(x"1C");                        expect(x"61", "'a' after bad frame");

        report "*** PASS: PS/2 keyboard decode correct ***" severity note;
        done <= true;
        wait;
    end process;
end architecture sim;

library ieee;
use ieee.std_logic_1164.all;
use std.textio.all;

package Kronos_Types is

    attribute clock_signal : string;

    procedure write_hex (ln : inout line; bits : in std_logic_vector);
    function conv_std_logic_vector_fix(arg: integer; size: integer) return std_logic_vector;

    -- synthesis translate_off
    constant trace : boolean := false;  -- OneChipBook synthesis build: trace off (no textio in synth)
    -- synthesis translate_on

end Kronos_Types;

package body Kronos_Types is

    function conv_std_logic_vector_fix(arg: integer; size: integer) return std_logic_vector is
	variable result: std_logic_vector(size-1 downto 0);
	variable temp: integer;
    begin
	temp := arg;
	for i in 0 to size-1 loop
	    if (temp mod 2) = 1 then
		result(i) := '1';
	    else 
		result(i) := '0';
	    end if;
	    if temp > 0 then
		temp := temp / 2;
	    elsif temp = integer'low then
		temp := -16#40000000#;
	    else
		temp := (temp - 1) / 2; -- simulate ASR
	    end if;
	end loop;
	return result;
    end;
    
    procedure write_hex(ln : inout line; bits : in std_logic_vector) is
	variable i, n : natural;
    begin
	n := 0;
	for i in bits'range loop
	    n := n * 2;
	    if bits(i) = '1' then
		n := n + 1;
	    end if;
	    if (i - bits'right) mod 4 = 0 then
		if n <= 9 then write(ln, n);
		else write(ln, character'val(character'pos('A') + n - 10));
		end if;
		n := 0;
	    end if;
	end loop;
   end write_hex;

end Kronos_Types;


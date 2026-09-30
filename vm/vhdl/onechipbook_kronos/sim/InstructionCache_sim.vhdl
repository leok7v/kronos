library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
use ieee.std_logic_textio.all;
use std.textio.all;
use work.Kronos_Types.all;

entity InstructionFetch is
    port (
	pc_inp	: in  std_logic_vector(31 downto 0);
	pc_out	: out std_logic_vector(23 downto 0);
	write_pc: in  std_logic;	    
	write_f	: in  std_logic;
	dec_pc	: in  std_logic;
	inc_pc	: in  std_logic;
	reset_pc: in  std_logic;
        cmd_end : in  std_logic;
	i_bus	: out std_logic_vector(7 downto 0);
	i_read	: in  std_logic;
	i_error : out std_logic;
	a_bus	: out std_logic_vector(31 downto 0);
	a_read	: out std_logic;
	d_bus	: in  std_logic_vector(31 downto 0);
	d_error	: in  std_logic;
        stall   : in  std_logic;
        trap    : in  std_logic;
	clock	: in  std_logic;
	reset	: in  std_logic);
end InstructionFetch;

architecture Behaviour of InstructionFetch is
    signal f        : std_logic_vector(31 downto 0) := (31 downto 0 => '0');
    signal pc       : std_logic_vector(23 downto 0) := (23 downto 0 => '0');
    signal pc_r     : std_logic_vector(23 downto 0);
    signal pc_e     : std_logic_vector(23 downto 0);
    signal pc_w     : std_logic_vector(23 downto 0);
    signal pc_new   : std_logic_vector(23 downto 0);
    signal pc_jmp   : std_logic_vector(23 downto 0);
    signal pc_ok    : std_logic;
    signal rd_next  : std_logic;
    signal write_pc1: std_logic;
    signal write_f1 : std_logic;
    signal pc_jmp_ok: std_logic;
    signal i_bus0   : std_logic_vector(7 downto 0);
begin

    -- synthesis translate_off
    print_trace : process (clock)
	variable ln : line;
    begin
       	if trace and clock'event and clock = '1' then
            -- COMMENTED FOR GHDL: write(ln, "InstructionFetch:");
            -- COMMENTED FOR GHDL: write(ln, " d_error ");
            -- COMMENTED FOR GHDL: write(ln, d_error);
            -- COMMENTED FOR GHDL: write(ln, ", pc_ok ");
            -- COMMENTED FOR GHDL: write(ln, pc_ok);
            -- COMMENTED FOR GHDL: write(ln, ", i_read ");
            -- COMMENTED FOR GHDL: write(ln, i_read);
            -- COMMENTED FOR GHDL: write(ln, ", wr_pc ");
            -- COMMENTED FOR GHDL: write(ln, write_pc);
            -- COMMENTED FOR GHDL: write(ln, ", dec_pc ");
            -- COMMENTED FOR GHDL: write(ln, dec_pc);
            -- COMMENTED FOR GHDL: write(ln, ", inc_pc ");
            -- COMMENTED FOR GHDL: write(ln, inc_pc);
            -- COMMENTED FOR GHDL: write(ln, ", f ");
            -- COMMENTED FOR GHDL: write_hex(ln, f);
            -- COMMENTED FOR GHDL: write(ln, ", pc ");
            -- COMMENTED FOR GHDL: write_hex(ln, pc);
            -- COMMENTED FOR GHDL: write(ln, " -> ");
            -- COMMENTED FOR GHDL: write_hex(ln, pc_new);
            -- COMMENTED FOR GHDL: write(ln, ", a_bus ");
            -- COMMENTED FOR GHDL: write_hex(ln, a_bus);
            writeline(std.textio.output, ln);
	end if;
    end process print_trace;
    -- synthesis translate_on

    i_bus <= i_bus0;
    pc_out <= pc;
    pc_new <=
        pc_inp(23 downto 0) when write_pc1 = '1' else
        pc_jmp when pc_jmp_ok = '1' else
        pc + 1;
    a_bus <= f + pc_new(23 downto 2);

    rd_next <= i_read and not reset_pc when pc(1 downto 0) = "11" else '0';
    a_read <= write_pc1 or pc_jmp_ok or rd_next;
    i_error <= d_error or not pc_ok;

    with pc(1 downto 0) select i_bus0 <=
        d_bus( 7 downto  0) when "00",
        d_bus(15 downto  8) when "01",
        d_bus(23 downto 16) when "10",
        d_bus(31 downto 24) when others;

    process (clock)
    begin
	if clock'event and clock = '1' and stall = '0' then
            write_pc1 <= write_pc;
            write_f1 <= write_f;
            if cmd_end = '1' then
                pc_r <= pc;
            end if;
            pc_e <= pc_r;
            pc_w <= pc_e;
            if reset = '1' or write_f = '1' or write_pc = '1' or reset_pc = '1' or trap = '1' then
                pc_ok <= '0';
            elsif write_pc1 = '1' then
                pc_ok <= '1';
            end if;
            if write_f1 = '1' and trap = '0' then
                f <= pc_inp;
            end if;
            if trap = '1' then
                pc <= pc_w;
            elsif write_pc1 = '1' or pc_jmp_ok = '1' or i_read = '1' then
                pc <= pc_new;
            end if;
            if dec_pc = '1' then
                pc_jmp <= pc_new - i_bus0;
            elsif inc_pc = '1' then
                pc_jmp <= pc_new + i_bus0;
            end if;
            pc_jmp_ok <= dec_pc or inc_pc;
	end if;
    end process;

end Behaviour;


library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
use ieee.std_logic_textio.all;
use std.textio.all;
use work.uCmdBits.all;
use work.Kronos_Types.all;

entity Decode is
    port (
	i_bus	: in std_logic_vector(7 downto 0);
	i_read	: out std_logic;
	i_dec_pc: out std_logic;
	i_inc_pc: out std_logic;
	i_rst_pc: out std_logic;
	i_error : in std_logic;
        cmd_end : out std_logic;
	u_cmd	: out std_logic_vector(ucmd_bits0);
        c_bus   : out std_logic_vector(7 downto 0);
        flag    : in std_logic;
        ready   : out std_logic;
        idle    : out std_logic;
        halt    : in std_logic;
        halted  : out std_logic;
        trap_stk: in std_logic;
        trap_mem: in std_logic;
        trap_sh : in std_logic;
        int_en  : in std_logic_vector(3 downto 0);
        int_rq  : in std_logic_vector(3 downto 0);
        rs_mode : in std_logic_vector(1 downto 0);
	clock   : in std_logic;
        stall   : in std_logic;
        trap    : in std_logic;
	reset   : in std_logic);
end Decode;

architecture Behavioral of Decode is

    constant ChipScope : boolean := false;
    constant trace : boolean := false;

    type stack_array is array(2 downto 0) of std_logic_vector(uadr_bits);

    signal u_cmd_i     : std_logic_vector(ucmd_bits);
    signal u_adr       : std_logic_vector(uadr_bits);
    signal addr_next   : std_logic_vector(uadr_bits);
    signal addr_jump   : std_logic_vector(uadr_bits);
    signal speculation : std_logic;
    signal fall_back   : std_logic;
    signal trap_rg     : std_logic_vector(3 downto 0);
    signal trapped     : std_logic;
    signal stk_size    : integer range stack_array'high + 1 downto 0 := 0;
    signal stack       : stack_array;
    signal en_rom      : std_logic;
    signal cmd_end_i   : std_logic;
    signal int_time_rs : std_logic;
    signal int_ck      : std_logic_vector(3 downto 0);
    signal int_vec     : std_logic_vector(1 downto 0);
    signal int_act     : std_logic;
    signal idle0       : std_logic;
    signal idle1       : std_logic;
    signal cmd_imm     : std_logic_vector(3 downto 0);
    signal trap_vec    : std_logic_vector(3 downto 0);
    signal trap_vec_r  : std_logic_vector(3 downto 0);
    signal trap_vec_e  : std_logic_vector(3 downto 0);
    signal i_read_i    : std_logic;

begin

    -- synthesis translate_off
    print_trace : process (clock)
	variable ln : line;
    begin
       	if clock'event and clock = '1' and en_rom = '1' and cmd_end_i = '1'
           and now >= 8 ms and now <= 8200 us then
            write(ln, string'("OP i_bus="));
            write_hex(ln, i_bus);
            write(ln, string'(" uadr="));
            write_hex(ln, u_adr);
            write(ln, string'(" trap="));
            write(ln, trap);
            writeline(std.textio.output, ln);
        end if;
    end process print_trace;
    -- synthesis translate_on

    chipscope_cores: if ChipScope generate
        component icon port (
            control0   : inout std_logic_vector(35 downto 0));
        end component;
        component ila port (
            control    : inout std_logic_vector(35 downto 0);
            clk        : in std_logic;
            data       : in std_logic_vector(31 downto 0);
            trig0      : in std_logic_vector(7 downto 0));
        end component;
        signal control : std_logic_vector(35 downto 0);
        signal data    : std_logic_vector(31 downto 0);
        signal trig    : std_logic_vector(7 downto 0);
    begin
        i_icon : icon port map (
            control0    => control);
        i_ila : ila port map (
            control   => control,
            clk       => clock,
            data      => data,
            trig0     => trig);
        data <= "00000" & flag & stall & reset & u_cmd_i(ub_goto) & addr_next & u_adr;
        trig <= "00" & stall & u_cmd_i(ub_reg_z);
    end generate;

    rom : entity work.Microcode
        port map (
            clock => clock,
            en    => en_rom,
            addr  => u_adr,
	    data  => u_cmd_i);

    en_rom <= not stall or fall_back;
    ready <= not fall_back and (not idle1 or int_act);
    idle <= idle1;
    fall_back <= (speculation and not flag) or (trap and not trapped);
    i_read <= i_read_i;
    u_cmd <= u_cmd_i(ucmd_bits0);
    cmd_end <= cmd_end_i;

    int_logic : process (clock) 
    begin
        if clock'event and clock = '1' then
            int_ck(3) <= int_rq(3);
            int_ck(2) <= int_rq(2);
            if int_rq(1) = '1' then
                int_ck(1) <= '1';
            elsif int_time_rs = '1' and stall = '0' then
                int_ck(1) <= '0';
            end if;
            int_ck(0) <= int_rq(0);
            if stall = '0' then
                idle1 <= idle0;
            end if;
            if stall = '0' or idle1 = '1' then
                if int_ck(3) = '1' and int_en(3) = '1' then
                    int_vec <= "11";
                    int_act <= '1';
                elsif int_ck(2) = '1' and int_en(2) = '1' then
                    int_vec <= "10";
                    int_act <= '1';
                elsif int_ck(1) = '1' and int_en(1) = '1' then
                    int_vec <= "01";
                    int_act <= '1';
                elsif int_ck(0) = '1' and int_en(0) = '1' then
                    int_vec <= "00";
                    int_act <= '1';
                else
                    int_act <= '0';
                end if;
            end if;
        end if;        
    end process int_logic;

    addr_mx : process (clock, reset, addr_next, addr_jump, fall_back, u_cmd_i, i_bus,
            trap, trap_vec_e, stack, stk_size, int_vec, int_act, i_error, rs_mode)
        variable jump_time : boolean;
        variable jump_done : boolean;
        variable jump_ibus : boolean;
    begin
        jump_time := false;
        jump_done := false;
        jump_ibus := false;
        if reset = '1' then
            u_adr <= (u_adr'high downto 2 => '0') & rs_mode;
        elsif fall_back = '1' then
            if trap = '1' then
                u_adr <= "1101111" & trap_vec_e;
            else
                u_adr <= addr_jump;
            end if;
        elsif u_cmd_i(ub_goto) = "00" then
            if stk_size /= 0 then
                u_adr <= stack(stk_size - 1);
            elsif int_act = '1' then
                jump_done := true;
                jump_time := int_vec = "01";
                u_adr <= "11011100" & int_vec & "0";
            else
                jump_done := true;
                jump_ibus := true;
                if i_error = '1' then
                    u_adr <= "11011110000";
                else
                    u_adr <= "111" & i_bus;
                end if;
            end if;
        elsif u_cmd_i(ub_goto) = "01" or u_cmd_i(ub_goto) = "10" then
            u_adr <= u_cmd_i(ub_addr);
        else
            u_adr <= addr_next;
        end if; 
        if jump_time then
            int_time_rs <= '1';
        else
            int_time_rs <= '0';
        end if;
        if jump_ibus or u_cmd_i(ub_reg_y) = rg_imm then
            i_read_i <= '1';
        else
            i_read_i <= '0';
        end if;
        if jump_done then
            cmd_end_i <= '1';
        else
            cmd_end_i <= '0';
        end if;
    end process addr_mx;

    u_cmd_mx : process (u_cmd_i, i_bus, cmd_imm, trap_rg)
    begin
        if u_cmd_i(ub_reg_y) = rg_imm then
            -- TODO: need to handle i_error here
            c_bus <= i_bus;
        elsif u_cmd_i(ub_reg_x) = rg_imm then
            c_bus <= "0000" & cmd_imm;
        elsif u_cmd_i(ub_reg_x) = rg_trap then
            c_bus <= "0000" & trap_rg;
        elsif u_cmd_i(ub_goto) = "10" or u_cmd_i(ub_goto) = "01" then
            c_bus <= "00000000";    
        else
            c_bus <= u_cmd_i(ub_const);
        end if;
    end process u_cmd_mx;

    trap_register : process (clock)
    begin
       	if clock'event and clock = '1' and stall = '0' then
            if reset = '1' then
                trap_rg <= "0000";
            elsif trap = '1' then
                trap_rg <= '0' & trap_sh & trap_mem & trap_stk;
            elsif i_read_i = '1' and i_error = '1' then
                trap_rg <= "1000";
            end if;
        end if;
    end process trap_register;

    addr_next_register : process (clock)
    begin
       	if clock'event and clock = '1' and (stall = '0' or fall_back = '1') then
            addr_next <= u_adr + 1;
	end if;
    end process addr_next_register;

    addr_jump_register : process (clock)
    begin
       	if clock'event and clock = '1' and stall = '0' then
            if reset = '1' then
                stk_size <= 0;
                addr_jump <= (addr_jump'range => '0');
            else
                case u_cmd_i(ub_goto) is
                when "00" =>
                    -- return
                    if stk_size /= 0 then
                        stk_size <= stk_size - 1;
                    end if;
                when "01" =>
                    -- call
                    if stk_size /= (stack_array'high + 1) then
                        stack(stk_size) <= addr_next;
                        stk_size <= stk_size + 1;
                    end if;
                when "10" =>
                    -- conditional goto
                    addr_jump <= addr_next;
                when others =>
                    null;
                end case;
                if u_cmd_i(ub_reg_z) = rg_drop_stk then
                    stk_size <= 0;
                end if;
            end if;
	end if;
    end process addr_jump_register;

    process (clock)
    begin
       	if clock'event and clock = '1' then
            if stall = '0' then
                if u_cmd_i(ub_goto) = "10" then
	            speculation <= '1'; -- conditional jump
                else
	            speculation <= '0';
                end if;
                trapped <= '0';
                if reset = '1' or cmd_end_i = '1' then
                    trap_vec <= "0000";
                elsif u_cmd_i(ub_trap) /= "0000" then
                    trap_vec <= u_cmd_i(ub_trap);
                end if;
                trap_vec_r <= trap_vec;
                trap_vec_e <= trap_vec_r;
            elsif fall_back = '1' then
                speculation <= '0';
                trapped <= trap;
            end if;
	end if;
    end process;

    process (clock)
    begin
       	if clock'event and clock = '1' and en_rom = '1' then
            if u_adr = "11100011011" then
                i_inc_pc <= '1';
            else
                i_inc_pc <= '0';
            end if;
            if u_adr = "11100011111" then
                i_dec_pc <= '1';
            else
                i_dec_pc <= '0';
            end if;
            if u_adr = "11111001010" or u_adr(10 downto 4) = "1111101" then
                i_rst_pc <= '1';
            else
                i_rst_pc <= '0';
            end if;
            if u_adr = "11110000111" then
                idle0 <= '1';
            else
                idle0 <= '0';
            end if;
            if u_adr(10 downto 8) = "111" then
                cmd_imm <= u_adr(3 downto 0);
                if u_adr(7 downto 0) = "10000001" then
                    halted <= '1';
                else
                    halted <= '0';
                end if;
            else
                -- Fixed: Always assign halted to prevent latching uninitialized value
                halted <= '0';
            end if;
	end if;
    end process;

end Behavioral;

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
use ieee.std_logic_textio.all;
use std.textio.all;
use work.uCmdBits.all;
use work.Kronos_Types.all;

-- Stage 1, Decode: micro address selection, micro-command reading
-- Stage 2, Read  : register addresses decoding and reading registers
-- Stage 3, Exec  : ALU operation or memory access
-- Stage 4, Write : writing result into a register

entity Kronos is
        generic (
            address_size : in integer := 32);
        port (
            rs_mode  : in std_logic_vector(1 downto 0);
            halt     : in std_logic;
	    halted   : out std_logic;
            idle     : out std_logic;
            ireq     : in std_logic_vector(3 downto 0);
            -- Wishbone master interface
            clk_i    : in std_logic;
	    rst_i    : in std_logic;
            adr_o    : out std_logic_vector(address_size - 1 downto 0);
	    dat_i    : in std_logic_vector(31 downto 0);
	    dat_o    : out std_logic_vector(31 downto 0);
            we_o     : out std_logic;
            stb_o    : out std_logic;
            cyc_o    : out std_logic;
            ack_i    : in std_logic;
            err_i    : in std_logic;
            lock_o   : out std_logic);

end Kronos;

architecture Behaviour of Kronos is

    signal clock    : std_logic;
    signal reset    : std_logic;
    signal stall    : std_logic;
    signal trap     : std_logic;
    signal i_bus    : std_logic_vector(7 downto 0);
    signal i_read   : std_logic;
    signal i_dec_pc : std_logic;
    signal i_inc_pc : std_logic;
    signal i_rst_pc : std_logic;
    signal i_error  : std_logic;
    signal u_cmd    : std_logic_vector(ucmd_bits0);
    signal c_bus    : std_logic_vector(7 downto 0);
    signal x_bus    : std_logic_vector(31 downto 0);
    signal x_ofs    : std_logic_vector(4 downto 0);
    signal y_bus    : std_logic_vector(31 downto 0);
    signal z_bus    : std_logic_vector(31 downto 0);
    signal z_mem    : std_logic_vector(31 downto 0);
    signal z_alu    : std_logic_vector(31 downto 0);
    signal z_mem_en : std_logic;
    signal alu_op   : std_logic_vector(4 downto 0);
    signal flag_op0 : std_logic_vector(2 downto 0);
    signal flag_op1 : std_logic_vector(2 downto 0);
    signal x_addr   : std_logic_vector(4 downto 0);
    signal y_addr   : std_logic_vector(4 downto 0);
    signal z_addr   : std_logic_vector(4 downto 0);
    signal flag     : std_logic;
    signal flag_z   : std_logic;
    signal flag_c   : std_logic;
    signal flag_sp  : std_logic;
    signal flag_p   : std_logic;
    signal trap_stk : std_logic;
    signal trap_mem : std_logic;
    signal trap_sh  : std_logic;
    signal ready_d  : std_logic;
    signal ready_s  : std_logic;
    signal stk      : std_logic_vector(4 downto 0);
    signal stk_e    : std_logic_vector(4 downto 0);
    signal stk_w    : std_logic_vector(4 downto 0);
    signal int_en   : std_logic_vector(3 downto 0);
    signal x_sreg   : std_logic;
    signal y_sreg   : std_logic;
    signal z_sreg   : std_logic;
    signal x_stk    : boolean;
    signal y_stk    : boolean;
    signal z_stk    : boolean;
    signal copt     : boolean;
    signal stk_p1   : std_logic_vector(4 downto 0);
    signal stk_m1   : std_logic_vector(4 downto 0);
    signal stk_m2   : std_logic_vector(4 downto 0);
    signal stk_err  : std_logic;
    signal cmd_end  : std_logic;
    signal cmd_end1 : std_logic;

begin

    clock <= clk_i;
    reset <= rst_i;
    stall <= (not ready_d or not ready_s) and not reset;
    trap  <= trap_mem or trap_sh or trap_stk;

    z_bus <= z_mem when z_mem_en = '1' else z_alu;
    flag_z <= '1' when z_alu = (z_alu'range => '0') else '0';
    flag_sp <= '0' when stk = "00000" else '1';
    flag_p <= not z_alu(z_alu'high);

    x_stk <= u_cmd(ub_reg_x) = rg_stk;
    y_stk <= u_cmd(ub_reg_y) = rg_stk;
    z_stk <= u_cmd(ub_reg_z) = rg_stk;
    copt  <= u_cmd(ub_reg_y) = rg_copt;

    stk_p1 <= stk + 1;
    stk_m1 <= stk - 1;
    stk_m2 <= stk - 2;

    x_sreg <= '0' when x_stk else u_cmd(ub_reg_x'high);
    y_sreg <= '0' when y_stk else u_cmd(ub_reg_y'high);
    z_sreg <= '0' when z_stk else u_cmd(ub_reg_z'high);

    x_addr <=
        u_cmd(ub_reg_x) when not x_stk else
        '1' & stk_m2(3 downto 0) when y_stk else
        '1' & stk_m1(3 downto 0);

    y_addr <=
        u_cmd(ub_reg_y) when not y_stk else
        '1' & stk_m1(3 downto 0);

    z_addr <=
        u_cmd(ub_reg_z) when not z_stk else
        '1' & stk(3 downto 0) when copt or not (x_stk or y_stk) else
        '1' & stk_m2(3 downto 0) when x_stk and y_stk else
        '1' & stk_m1(3 downto 0);

    with flag_op1 select flag <=
        '1'         when "000",
        flag_z      when "001",
        not flag_z  when "010",
        flag_c      when "011",
        not flag_c  when "100",
        flag_sp     when "101",
        flag_p      when "110",
        '1'         when others;

    cache : entity work.DataStorage
        generic map(
            address_size => address_size)
    	port map (
	    adr_o   => adr_o,
	    dat_i   => dat_i,
            dat_o   => dat_o,
	    we_o    => we_o,
            cyc_o   => cyc_o,
            stb_o   => stb_o,
            lock_o  => lock_o,
	    ack_i   => ack_i,
	    err_i   => err_i,
            x_addr  => x_addr,
            x_bus   => x_bus,
            x_ofs   => x_ofs,
            x_sreg  => x_sreg,
            y_addr  => y_addr,
            y_bus   => y_bus,
            y_sreg  => y_sreg,
            z_addr  => z_addr,
            z_bus   => z_bus,
            z_mem   => z_mem,
            z_mem_en=> z_mem_en,
            z_sreg  => z_sreg,
            c_bus   => c_bus,
	    i_bus   => i_bus,
	    i_read  => i_read,
	    i_dec_pc=> i_dec_pc,
	    i_inc_pc=> i_inc_pc,
	    i_rst_pc=> i_rst_pc,
	    i_error => i_error,
            cmd_end => cmd_end,
            trap_mem=> trap_mem,
            trap_sh => trap_sh,
            int_en  => int_en,
            ready   => ready_s,
            stall   => stall,
            trap    => trap,
	    clock   => clock,
	    reset   => reset);

    command_decoder : entity work.Decode
    	port map (
	    i_bus   => i_bus,
	    i_read  => i_read,
	    i_dec_pc=> i_dec_pc,
	    i_inc_pc=> i_inc_pc,
	    i_rst_pc=> i_rst_pc,
	    i_error => i_error,
            cmd_end => cmd_end,
	    u_cmd   => u_cmd,
            c_bus   => c_bus,
            flag    => flag,
            ready   => ready_d,
            idle    => idle,
            halt    => halt,
            halted  => halted,
            trap_stk=> trap_stk,
            trap_mem=> trap_mem,
            trap_sh => trap_sh,
            int_en  => int_en,
            int_rq  => ireq,
            rs_mode => rs_mode,
	    clock   => clock,
            stall   => stall,
            trap    => trap,
	    reset   => reset);

    alu_comp : entity work.ALU
        port map (
    	    op      => alu_op,
    	    x_bus   => x_bus,
    	    y_bus   => y_bus,
    	    z_bus   => z_alu,
            flag_c  => flag_c,
            clock   => clock,
            stall   => stall);

    -- [trace stripped for GHDL]

    stk_control : process (clock)
        variable error : boolean;
    begin
        if clock'event and clock = '1' and stall = '0' then
            cmd_end1 <= cmd_end;
            if cmd_end1 = '1' then
                stk_e <= stk;
            end if;
            stk_w <= stk_e;
            trap_stk <= stk_err and not trap;
            error := false;
            if reset = '1' then
                stk <= "00000";
            elsif trap = '1' then
                stk <= stk_w;
            elsif copt or (not x_stk and not y_stk and z_stk) then
                if stk /= "10000" then
                    stk <= stk_p1;
                else
                    error := true;
                end if;
            elsif x_stk and y_stk and not z_stk then
                if stk(4 downto 1) /= "0000" then
                    stk <= stk_m2;
                else
                    error := true;
                end if;
            elsif (x_stk and y_stk) or ((x_stk or y_stk) and not z_stk) then
                if stk /= "00000" then
                    stk <= stk_m1;
                else
                    error := true;
                end if;
            elsif (x_stk or y_stk) and stk = "00000" then
                error := true;
            end if;
            if error then
                stk_err <= '1';
            else
                stk_err <= '0';
            end if;
        end if;
    end process stk_control;

    flag_control : process(clock)
    begin
        if clock'event and clock = '1' and stall = '0' then
	    flag_op0 <= u_cmd(ub_flag);
	    flag_op1 <= flag_op0;
	end if;
    end process flag_control;

    alu_control : process(clock)
    begin
        if clock'event and clock = '1' and stall = '0' then
            if u_cmd(ub_reg_y) = rg_mrd or u_cmd(ub_reg_z) = rg_mwr then
                alu_op <= alu_x;
                x_ofs <= u_cmd(ub_alu);
            else
	        alu_op <= u_cmd(ub_alu);
                x_ofs <= "00000";
            end if;
	end if;
    end process alu_control;

end Behaviour;


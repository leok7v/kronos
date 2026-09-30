library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;
use ieee.std_logic_textio.all;
use std.textio.all;
use work.Kronos_Types.all;
use work.uCmdBits.all;

-- Note: data on x_bus and y_bus is delayed 1 clock cycle relative to addresses.
-- Data and address transitions are synchronized by rasing edge of clock.

entity DataStorage is
    generic (
        address_size : in integer := 32);
    port (
        -- Wishbone interface
	adr_o   : out std_logic_vector(address_size - 1 downto 0);
	dat_i   : in std_logic_vector(31 downto 0);
	dat_o   : out std_logic_vector(31 downto 0);
	we_o    : out std_logic;
	cyc_o   : out std_logic;
	stb_o   : out std_logic;
	lock_o  : out std_logic;
	ack_i   : in std_logic;
	err_i   : in std_logic;
        -- Staged signals                                       to stage
        x_addr  : in std_logic_vector(4 downto 0);              -- R
	x_bus	: inout std_logic_vector(31 downto 0);          -- E
	x_ofs	: in std_logic_vector(4 downto 0);              -- E
        x_sreg  : in std_logic;                                 -- R
        y_addr  : in std_logic_vector(4 downto 0);              -- R
	y_bus	: inout std_logic_vector(31 downto 0);          -- E
        y_sreg  : in std_logic;                                 -- R
        z_addr  : in std_logic_vector(4 downto 0);              -- R
	z_bus	: in std_logic_vector(31 downto 0);             -- W
	z_mem	: out std_logic_vector(31 downto 0);            -- W
	z_mem_en: out std_logic;                                -- W
        z_sreg  : in std_logic;                                 -- R
        c_bus   : in std_logic_vector(7 downto 0);              -- R
        trap_mem: out std_logic;                                -- W
        trap_sh : out std_logic;                                -- W
        int_en  : out std_logic_vector(3 downto 0);             -- W+1
        -- Instructions bus
        i_bus   : out std_logic_vector(7 downto 0);             
        i_read  : in std_logic;
        i_error : out std_logic;
        i_dec_pc: in std_logic;
        i_inc_pc: in std_logic;
        i_rst_pc: in std_logic;
        cmd_end : in std_logic;
        -- Misc
        ready   : out std_logic;
        stall   : in std_logic;
        trap    : in std_logic;
	clock	: in std_logic;
	reset	: in std_logic);

end DataStorage;

architecture Behaviour of DataStorage is

    signal a0_bus   : std_logic_vector(31 downto 0);
    signal a0_read  : std_logic;
    signal a0_write : std_logic;
    signal d0_error : std_logic;
    signal d0_ready : std_logic;
    signal mem_dat  : std_logic;
    signal wr_pc    : std_logic;
    signal wr_f     : std_logic;
    signal wr_tlb   : std_logic;
    signal wr_base  : std_logic;
    signal pc_out   : std_logic_vector(23 downto 0);
    signal y_addr1  : std_logic_vector(4 downto 0);
    signal y_sreg1  : std_logic;
    signal z_addr1  : std_logic_vector(4 downto 0);
    signal z_addr2  : std_logic_vector(4 downto 0);
    signal z_sreg1  : std_logic;
    signal z_sreg2  : std_logic;
    signal h_reg    : std_logic_vector(31 downto 0);
    signal trap1    : std_logic;
    signal rg_s_chk : std_logic;
    signal rg_s_err : std_logic;

    signal x_regs0  : std_logic_vector(31 downto 0);
    signal y_regs0  : std_logic_vector(31 downto 0);
    signal x_regs1  : std_logic_vector(31 downto 0);
    signal y_regs1  : std_logic_vector(31 downto 0);
    signal x_bypass : boolean;
    signal y_bypass : boolean;
    signal regs_we0 : std_logic;
    signal regs_we1 : std_logic;

    signal a1_bus   : std_logic_vector(31 downto 0);
    signal a1_read  : std_logic;
    signal d1_out   : std_logic_vector(31 downto 0);
    signal d1_error : std_logic;
    signal d1_ready : std_logic;

    signal x_bus0   : std_logic_vector(31 downto 0);
    signal y_bus0   : std_logic_vector(31 downto 0);

    -- synthesis translate_off
    signal x_addr1  : std_logic_vector(4 downto 0);
    signal x_sreg1  : std_logic;
    -- synthesis translate_on

begin

    -- synthesis translate_off
    print_trace : process (clock) 
	variable ln : line;
    begin
	if trace and clock'event and clock = '1' then
            x_addr1 <= x_addr;
            x_sreg1 <= x_sreg;
	    -- COMMENTED FOR GHDL: write(ln, "X[");
	    write_hex(ln, x_addr1);
	    -- COMMENTED FOR GHDL: write(ln, "]=");
	    write_hex(ln, x_bus);
	    -- COMMENTED FOR GHDL: write(ln, ", Y[");
	    write_hex(ln, y_addr1);
	    -- COMMENTED FOR GHDL: write(ln, "]=");
	    write_hex(ln, y_bus);
	    -- COMMENTED FOR GHDL: write(ln, ", Z[");
	    write_hex(ln, z_addr2);
	    -- COMMENTED FOR GHDL: write(ln, "]=");
	    write_hex(ln, z_bus);
	    -- COMMENTED FOR GHDL: write(ln, ", c_bus ");
	    write_hex(ln, c_bus);
	    writeline(std.textio.output, ln);
	end if;
    end process print_trace;
    -- synthesis translate_on

    d_cache : entity work.DataCache
        generic map(
            address_size => address_size)
	port map (
	    adr_o    => adr_o,
	    dat_i    => dat_i,
            dat_o    => dat_o,
	    we_o     => we_o,
            cyc_o    => cyc_o,
            stb_o    => stb_o,
            lock_o   => lock_o,
	    ack_i    => ack_i,
	    err_i    => err_i,
	    a0_bus   => a0_bus,
	    a0_read  => a0_read,
	    a0_write => a0_write,
	    a0_wtlb  => wr_tlb,
	    a0_wbase => wr_base,
	    d0_in    => y_bus,
	    d0_out   => z_mem,
	    d0_error => d0_error,
            d0_ready => d0_ready,
	    a1_bus   => a1_bus,
	    a1_read  => a1_read,
	    d1_out   => d1_out,
	    d1_error => d1_error,
            d1_ready => d1_ready,
            stall    => stall,
	    clock    => clock,
	    reset    => reset);

    i_cache : entity work.InstructionFetch
        port map (
	    pc_inp  => z_bus,
            pc_out  => pc_out,
	    write_pc=> wr_pc,
	    write_f => wr_f,
            dec_pc  => i_dec_pc,
            inc_pc  => i_inc_pc,
            reset_pc=> i_rst_pc,
            cmd_end => cmd_end,
	    i_bus   => i_bus,
	    i_read  => i_read,
	    i_error => i_error,
	    a_bus   => a1_bus,
	    a_read  => a1_read,
	    d_bus   => d1_out,
	    d_error => d1_error,
            stall   => stall,
            trap    => trap,
	    clock   => clock,
	    reset   => reset);

    regs_block: for i in z_bus'range generate
    begin
         regs_block_a0: entity work.DistrRam
         port map (       D => z_bus(i),
                         WE => regs_we0,
                       WCLK => clock,
                          A => z_addr2(3 downto 0),
                       DPRA => x_addr(3 downto 0),
                        DPO => x_regs0(i));

         regs_block_a1: entity work.DistrRam
         port map (       D => z_bus(i),
                         WE => regs_we1,
                       WCLK => clock,
                          A => z_addr2(3 downto 0),
                       DPRA => x_addr(3 downto 0),
                        DPO => x_regs1(i));

         regs_block_b0: entity work.DistrRam
         port map (       D => z_bus(i),
                         WE => regs_we0,
                       WCLK => clock,
                          A => z_addr2(3 downto 0),
                       DPRA => y_addr(3 downto 0),
                        DPO => y_regs0(i));

         regs_block_b1: entity work.DistrRam
         port map (       D => z_bus(i),
                         WE => regs_we1,
                       WCLK => clock,
                          A => z_addr2(3 downto 0),
                       DPRA => y_addr(3 downto 0),
                        DPO => y_regs1(i));
    end generate regs_block;

    ready <= d0_ready and d1_ready and rg_s_chk;

    regs_we0 <= not trap and not trap1 and not z_sreg2 and not z_addr2(4);
    regs_we1 <= not trap and not trap1 and not z_sreg2 and z_addr2(4);

    z_mem_en <= mem_dat;
    a0_bus <= x_bus + x_ofs;
    trap_mem <= mem_dat and d0_error;
    trap_sh <= rg_s_err;

    rg_s_chk <= rg_s_err when z_sreg2 = '0' and z_addr2 = rg_s and z_bus > h_reg else '1';

    wr_f    <= not trap when z_sreg1 = '0' and z_addr1 = rg_f    else '0';
    wr_pc   <= not trap when z_sreg1 = '1' and z_addr1 = rg_pc   else '0';
    wr_tlb  <= not trap when z_sreg1 = '1' and z_addr1 = rg_tlb  else '0';
    wr_base <= not trap when z_sreg1 = '1' and z_addr1 = rg_base else '0';
    a0_read <= not trap when y_sreg1 = '1' and y_addr1 = rg_mrd  else '0';
    a0_write<= not trap when z_sreg1 = '1' and z_addr1 = rg_mwr  else '0';

    x_bus <= z_bus when x_bypass else x_bus0;
    y_bus <= z_bus when y_bypass else y_bus0;

    process (clock)
    begin
        if clock'event and clock = '1' then
            if stall = '0' then
                rg_s_err <= '0';
            elsif d0_ready = '1' and rg_s_chk = '0' then
                rg_s_err <= '1';
            end if;
        end if;
    end process;

    addr_regs : process (clock)
    begin
	if clock'event and clock = '1' and stall = '0' then
            trap1 <= trap;
            mem_dat <= a0_read or a0_write;
	    z_addr1 <= z_addr;
	    z_addr2 <= z_addr1;
            z_sreg1 <= z_sreg;
            z_sreg2 <= z_sreg1;
            y_sreg1 <= y_sreg;
            y_addr1 <= y_addr;
	    x_bypass <= x_sreg = '0' and z_sreg1 = '0' and x_addr = z_addr1;
	    y_bypass <= y_sreg = '0' and z_sreg1 = '0' and y_addr = z_addr1;
            if trap = '0' and trap1 = '0' then
                if z_sreg2 = '1' and z_addr2 = rg_h then
                    h_reg <= z_bus;
                end if;
                if z_sreg2 = '0' and z_addr2 = rg_m then
                    -- OneChipBook: take the interrupt-enable mask from the LOW
                    -- bits of M, not 19..16.
                    --
                    -- This disk image's OS is built for the Kronos-3 VM, and
                    -- that VM gates interrupts on the low bits:
                    --     device/SIO traps 0Ch..3Eh   if (M & 0x1)
                    --     timer trap 1                if (M & 0x2)
                    -- Traced live from the running OS, the only M values it
                    -- ever writes are 800000F8h / 800000FBh / 00000003h -- it
                    -- toggles exactly bits 0 and 1 (the driver's di()/ei()),
                    -- and bits 19..16 are ZERO in every one of them. With the
                    -- stock mapping int_en is therefore permanently "0000", no
                    -- interrupt line can ever fire, and console input dies at
                    -- the login prompt: the OS enables the DL11 receive
                    -- interrupt, we assert cpu_ireq(0), and the CPU never takes
                    -- it -- so the OS never reads RBUF.
                    --
                    -- Lines 2 and 3 are device lines too (ATA / undef), so they
                    -- share bit 0; they are tied off at the top level anyway.
                    -- The 5.0 tree keeps the original mapping for Spartan-3,
                    -- which is why this is an OneChipBook-local copy.
                    int_en(0) <= z_bus(0);
                    int_en(1) <= z_bus(1);
                    int_en(2) <= z_bus(0);
                    int_en(3) <= z_bus(0);
                end if;
            end if;
            if x_sreg = '1' and x_addr = rg_io then
                x_bus0 <= "11111111111111111111000000000000";
            elsif x_sreg = '1' and x_addr = rg_pc then
                x_bus0 <= "00000000" & pc_out;
            elsif x_sreg = '1' and x_addr = rg_swap then
                x_bus0 <= x_bus;
            elsif x_sreg = '1' then
                x_bus0 <= "000000000000000000000000" & c_bus;
            elsif z_sreg2 = '0' and x_addr = z_addr2 then
                x_bus0 <= z_bus;
            elsif x_addr(4) = '0' then
                x_bus0 <= x_regs0;
            else
                x_bus0 <= x_regs1;
            end if;
            if y_sreg = '1' and y_addr = rg_nil then
                y_bus0 <= "01111111111111111111111110000000";
            elsif y_sreg = '1' and y_addr = rg_pc then
                y_bus0 <= "00000000" & pc_out;
            elsif y_sreg = '1' then
                y_bus0 <= "000000000000000000000000" & c_bus;
            elsif z_sreg2 = '0' and y_addr = z_addr2 then
                y_bus0 <= z_bus;
            elsif y_addr(4) = '0' then
                y_bus0 <= y_regs0;
            else
                y_bus0 <= y_regs1;
            end if;
        end if;
    end process addr_regs;

end Behaviour;


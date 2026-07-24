library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Verify sdram_controller against the behavioural SDRAM model: write a set of
-- 32-bit words to assorted addresses, read them back, check they match.

entity tb_sdram is
end tb_sdram;

architecture sim of tb_sdram is
    constant ADDR_BITS : integer := 19;
    signal clk   : std_logic := '0';
    signal reset : std_logic := '1';
    signal done  : boolean := false;

    signal wb_adr : std_logic_vector(ADDR_BITS-1 downto 0) := (others => '0');
    signal wb_di  : std_logic_vector(31 downto 0) := (others => '0');
    signal wb_do  : std_logic_vector(31 downto 0);
    signal wb_we, wb_stb, wb_cyc, wb_ack, rdy : std_logic := '0';

    signal sd_a   : std_logic_vector(12 downto 0);
    signal sd_ba  : std_logic_vector(1 downto 0);
    signal sd_dq  : std_logic_vector(15 downto 0);
    signal sd_cke, sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n, sd_dqml, sd_dqmh : std_logic;

    signal errors : integer := 0;

    -- test vectors: (word address, data)
    type vec_t is record a : integer; d : std_logic_vector(31 downto 0); end record;
    type vlist_t is array (natural range <>) of vec_t;
    constant V : vlist_t := (
        (0,    x"DEADBEEF"), (1,    x"00000090"), (5,    x"07F00012"),
        (100,  x"12345678"), (144,  x"000001B1"), (433,  x"00000006"),
        (511,  x"CAFEF00D"), (512,  x"A5A5A5A5"), (1000, x"0000FFFF"),
        (2047, x"FFFF0000"), (5000, x"11223344"));
begin
    clk <= not clk after 18.5 ns;   -- ~27 MHz

    dut : entity work.sdram_controller
        generic map (ADDR_BITS => ADDR_BITS, T_INIT => 20, T_REFI => 40)
        port map (clk=>clk, reset=>reset,
                  wb_adr=>wb_adr, wb_dat_i=>wb_di, wb_dat_o=>wb_do,
                  wb_we=>wb_we, wb_stb=>wb_stb, wb_cyc=>wb_cyc, wb_ack=>wb_ack, ready=>rdy,
                  sd_a=>sd_a, sd_ba=>sd_ba, sd_dq=>sd_dq, sd_cke=>sd_cke,
                  sd_cs_n=>sd_cs_n, sd_ras_n=>sd_ras_n, sd_cas_n=>sd_cas_n, sd_we_n=>sd_we_n,
                  sd_dqml=>sd_dqml, sd_dqmh=>sd_dqmh);

    model : entity work.sdram_model
        port map (clk=>clk, a=>sd_a, ba=>sd_ba, dq=>sd_dq, cke=>sd_cke,
                  cs_n=>sd_cs_n, ras_n=>sd_ras_n, cas_n=>sd_cas_n, we_n=>sd_we_n,
                  dqml=>sd_dqml, dqmh=>sd_dqmh);

    stim : process
        procedure wb_write(addr : integer; data : std_logic_vector(31 downto 0)) is
        begin
            wb_adr <= std_logic_vector(to_unsigned(addr, ADDR_BITS));
            wb_di  <= data; wb_we <= '1'; wb_stb <= '1'; wb_cyc <= '1';
            loop wait until rising_edge(clk); exit when wb_ack = '1'; end loop;
            wb_stb <= '0'; wb_cyc <= '0'; wb_we <= '0';
            wait until rising_edge(clk);
        end procedure;

        procedure wb_read(addr : integer; expect : std_logic_vector(31 downto 0)) is
        begin
            wb_adr <= std_logic_vector(to_unsigned(addr, ADDR_BITS));
            wb_we <= '0'; wb_stb <= '1'; wb_cyc <= '1';
            loop wait until rising_edge(clk); exit when wb_ack = '1'; end loop;
            if wb_do /= expect then
                report "MISMATCH @word " & integer'image(addr) &
                       " got " & to_hstring(wb_do) & " expected " & to_hstring(expect) severity warning;
                errors <= errors + 1;
            end if;
            wb_stb <= '0'; wb_cyc <= '0';
            wait until rising_edge(clk);
        end procedure;
    begin
        reset <= '1'; wait for 1 us; reset <= '0';
        wait until rdy = '1' for 200 us;
        assert rdy = '1' report "FAIL: controller never became ready (init)" severity failure;
        report "*** SDRAM init complete ***" severity note;

        for i in V'range loop wb_write(V(i).a, V(i).d); end loop;
        for i in V'range loop wb_read(V(i).a, V(i).d); end loop;

        if errors = 0 then
            report "*** PASS: all " & integer'image(V'length) & " words written and read back correctly ***" severity note;
        else
            report "*** FAIL: " & integer'image(errors) & " mismatches ***" severity failure;
        end if;
        done <= true; wait;
    end process;
end sim;

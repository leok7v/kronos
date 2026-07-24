library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;

-- OneChipBook sim: boot the REAL Excelsior boot chain from the (modelled) SD card.
-- sd_model_xd0 serves the genuine xd0.dsk; the disk controller auto-loads
-- sectors 0-3 and releases the CPU; the real stage-1/booter then reads the
-- rest of the disk through io2 and talks on the DL11 console. The testbench
-- decodes the UART TX line and prints every console line; it passes when the
-- booter reports "Loaded OK" (with the VM-compatible microcode in place).
--
-- LED map: 2(idx1)=SDRAM ready, 3(idx2)=boot loaded, 4(idx3)=disk busy,
--          5(idx4)=console activity, 7(idx6)=cpu idle, 8(idx7)=boot fail.

entity tb_onechip is
    generic (
        SIM_BAUD_DIV : integer := 32;      -- fast UART for sim
        -- memory_top now fills 1792 KB, not 512 KB, so the first console line
        -- alone lands near 540 ms; allow room for the OS load after it
        RUN_FOR      : time    := 1100 ms  -- hard stop
    );
end tb_onechip;

architecture sim of tb_onechip is
    signal clk   : std_logic := '0';
    signal rst_n : std_logic := '1';
    signal led   : std_logic_vector(8 downto 0);

    signal sd_sclk, sd_mosi, sd_cs, sd_miso : std_logic;
    signal uart_txd : std_logic;
    signal uart_rxd : std_logic := '1';
    signal ps2_clk  : std_logic := '1';   -- idle high (open collector)
    signal ps2_data : std_logic := '1';

    signal a  : std_logic_vector(12 downto 0);
    signal ba : std_logic_vector(1 downto 0);
    signal dq : std_logic_vector(15 downto 0);
    signal cke, csn, rasn, casn, wen, dqml, dqmh, sclk : std_logic;

    signal dbg_adr : std_logic_vector(22 downto 0);
    signal dbg_stb, dbg_we, dbg_ack : std_logic;
    signal dbg_dat, dbg_dati : std_logic_vector(31 downto 0);

    constant BIT_TIME : time := 18.5 ns * 2 * SIM_BAUD_DIV;

    signal loaded_ok : boolean := false;
    signal done      : boolean := false;
begin
    -- stopping the clock quiesces every process and ends the simulation (VHDL-93)
    clk <= not clk after 18.5 ns when not done else '0';

    dut : entity work.OneChipBook
        generic map (RESET_CYCLES => 20, SD_T_INIT => 20, SD_T_REFI => 150,
                     USE_PLL => false,     -- GHDL cannot elaborate altpll
                     SD_READ_LAT => 4, SD_CLK_DIV => 16,
                     BAUD_DIV => SIM_BAUD_DIV, DISK_4KB => 197)
        port map (clk => clk, clk_ref => clk, rst_n => rst_n, led => led,
                  sw => "00000000",     -- debug readout: view 00 = live status
                  sd_sclk => sd_sclk, sd_mosi => sd_mosi, sd_cs => sd_cs, sd_miso => sd_miso,
                  uart_txd => uart_txd, uart_rxd => uart_rxd,
                  sd_a => a, sd_ba => ba, sd_dq => dq, sd_cke => cke,
                  sd_cs_n => csn, sd_ras_n => rasn, sd_cas_n => casn, sd_we_n => wen,
                  sd_dqml => dqml, sd_dqmh => dqmh, sd_clk => sclk,
                  ps2_clk => ps2_clk, ps2_data => ps2_data,
                  vga_hsync => open, vga_vsync => open,
                  vga_r => open, vga_g => open, vga_b => open,
                  dbg_adr => dbg_adr, dbg_stb => dbg_stb, dbg_we => dbg_we, dbg_dat => dbg_dat,
                  dbg_dati => dbg_dati, dbg_ack => dbg_ack);

    card : entity work.sd_model_xd0
        generic map (FNAME => "xd0.dec", SECTORS => 1580)
        port map (sclk => sd_sclk, cs => sd_cs, mosi => sd_mosi, miso => sd_miso);

    ram : entity work.sdram_model
        generic map (MEM_HW => 1048576)    -- back the full 19-bit word space
        port map (clk => clk, a => a, ba => ba, dq => dq, cke => cke,
                  cs_n => csn, ras_n => rasn, cas_n => casn, we_n => wen, dqml => dqml, dqmh => dqmh);

    -- ------------------------------------------------ UART decoder
    console : process
        variable byte  : std_logic_vector(7 downto 0);
        variable l     : line;
        variable ch    : character;
        variable nbytes: integer := 0;
    begin
        wait until uart_txd = '0';                 -- start bit
        wait for BIT_TIME + BIT_TIME/2;            -- centre of data bit 0
        for i in 0 to 7 loop
            byte(i) := uart_txd;                   -- LSB first
            wait for BIT_TIME;
        end loop;
        nbytes := nbytes + 1;
        if nbytes = 1 then
            report "FIRST CONSOLE BYTE: 0x" & integer'image(to_integer(unsigned(byte))) severity note;
        end if;
        -- accumulate into a line; print on CR or LF
        if byte = x"0D" or byte = x"0A" then
            if l /= null then
                if l'length > 0 then
                    report "CONSOLE: " & l.all severity note;
                    if l'length >= 9 then
                        if l.all(l'length-8 to l'length) = "Loaded OK" then
                            loaded_ok <= true;
                        end if;
                    end if;
                end if;
                deallocate(l); l := null;
            end if;
        else
            ch := character'val(to_integer(unsigned(byte)));
            write(l, ch);
        end if;
    end process;

    -- Once the OS is up, TYPE at it. This is the end-to-end check that console
    -- input works: the keystroke has to be decoded from PS/2, injected into the
    -- DL11, raise interrupt line 0, vector through the retargeted microcode to
    -- trap 0x0C, and be read back out by the OS -- which then echoes it, so the
    -- echo appearing on the console proves the whole chain.
    typist : process
        procedure key(b : std_logic_vector(7 downto 0)) is
            variable par  : std_logic;
            variable bits : std_logic_vector(0 to 10);
        begin
            par := not (b(0) xor b(1) xor b(2) xor b(3) xor
                        b(4) xor b(5) xor b(6) xor b(7));
            bits := '0' & b(0) & b(1) & b(2) & b(3) & b(4) & b(5) & b(6) & b(7)
                    & par & '1';
            for i in 0 to 10 loop
                ps2_data <= bits(i);
                wait for 20 us;
                ps2_clk  <= '0';
                wait for 30 us;
                ps2_clk  <= '1';
                wait for 16 us;
            end loop;
            ps2_data <= '1';
            wait for 2 ms;             -- let the OS consume it
        end procedure;
    begin
        wait until loaded_ok for 1100 ms;
        if not loaded_ok then wait; end if;
        wait for 20 ms;                -- let the login prompt be drawn
        report "*** TYPING 'root' at the console ***" severity note;
        key(x"2D");                    -- r
        key(x"44");                    -- o
        key(x"44");                    -- o
        key(x"2C");                    -- t
        key(x"5A");                    -- Enter
        report "*** TYPING done ***" severity note;
        wait;
    end process;

    -- periodic heartbeat: shows progress (or a stuck CPU) while the booter
    -- churns through its ~1 MB memory probe before the first console write
    -- track the CPU address bus and emit a windowed heartbeat (one process, so
    -- min/max/acc are plain variables with a single writer). ~270270 clocks = 5 ms.
    track : process
        type iarr is array(0 to 47) of integer;
        variable h_adr, h_we, h_dat, h_dati : iarr := (others => 0);
        variable hp : integer := 0;
        variable dumped : boolean := false;
        variable prev_hi : integer := 0;         -- last high address seen (memory_top)
        variable prev_stb : std_logic := '0';
        variable ad, wmin, wmax, wacc, cyc, t, k, idx : integer := 0;
        variable nfirst : integer := 0;          -- TEMP: trace the first accesses
    begin
        wmin := 524287; wmax := 0; wacc := 0; cyc := 0; t := 0;
        loop
            wait until rising_edge(clk);
            exit when done;
            -- ring buffer of completed accesses; dump when the CPU first jumps
            -- to a very low address (the fault -> execute-from-0) after startup
            if dbg_stb = '1' and dbg_ack = '1' then
                ad := to_integer(unsigned(dbg_adr));
                -- Trace the first 40 completed CPU accesses after release. This
                -- is the fastest "is the machine even alive" check there is: a
                -- healthy boot reads mem[0]=144 and walks the descriptor chain
                -- 0 -> 144 -> 433 -> ...; all-zero rdat means the CPU is executing
                -- zeros (e.g. a wrong SDRAM read_lat), which otherwise just looks
                -- like a mysterious hang tens of ms later.
                if nfirst < 40 then
                    nfirst := nfirst + 1;
                    report "FIRST#" & integer'image(nfirst) & " adr=" & integer'image(ad) &
                           " we=" & std_logic'image(dbg_we) &
                           " wdat=" & integer'image(to_integer(signed(dbg_dat))) &
                           " rdat=" & integer'image(to_integer(signed(dbg_dati))) severity note;
                end if;
                -- trigger: the CPU was running high in memory (memory_top) and
                -- suddenly drops to a very low address = the fault/jump-to-0.
                -- Dump the ring buffer (captured the transition) BEFORE storing
                -- this access, then store it.
                if ad < 16 and prev_hi > 4096 and not dumped and t >= 5 then
                    dumped := true;
                    report "=== JUMP low (adr=" & integer'image(ad) & ") after high=" &
                           integer'image(prev_hi) & " at t~=" & integer'image(t) &
                           "ms; last 48 accesses ===" severity note;
                    for k in 0 to 47 loop
                        idx := (hp + k) mod 48;
                        report "  adr=" & integer'image(h_adr(idx)) & " we=" & integer'image(h_we(idx)) &
                               " wdat=" & integer'image(h_dat(idx)) & " rdat=" & integer'image(h_dati(idx)) severity note;
                    end loop;
                end if;
                if ad > 64 then prev_hi := ad; end if;
                h_adr(hp) := ad;
                h_we(hp)  := 0; if dbg_we = '1' then h_we(hp) := 1; end if;
                h_dat(hp) := to_integer(signed(dbg_dat));
                h_dati(hp):= to_integer(signed(dbg_dati));
                hp := (hp + 1) mod 48;
            end if;
            -- probe every completed access to word 233 (mem[F+6], the cl6 target)
            if dbg_stb = '1' and dbg_ack = '1' and to_integer(unsigned(dbg_adr)) = 233 then
                report "MEM233 we=" & std_logic'image(dbg_we) &
                       " wdata=" & integer'image(to_integer(signed(dbg_dat))) &
                       " rdata=" & integer'image(to_integer(signed(dbg_dati))) severity note;
            end if;
            if dbg_stb = '1' and prev_stb = '0' then
                ad := to_integer(unsigned(dbg_adr));
                -- ignore I/O-page addresses (>=0x7F000) for the RAM-range window
                if ad < 520192 then
                    if ad < wmin then wmin := ad; end if;
                    if ad > wmax then wmax := ad; end if;
                elsif ad = 520192 and dbg_we = '1' then
                    report "MMCON byte=0x" & integer'image(to_integer(unsigned(dbg_dat(7 downto 0)))) &
                           " '" & character'val(to_integer(unsigned(dbg_dat(7 downto 0)))) & "'" severity note;
                elsif ad >= 520208 and ad <= 520215 and dbg_we = '1' then
                    report "DISKW off=" & integer'image(ad - 520208) &
                           " data=" & integer'image(to_integer(unsigned(dbg_dat))) severity note;
                else
                    report "IOACC adr=0x" & integer'image(ad) & " we=" & std_logic'image(dbg_we) severity note;
                end if;
                wacc := wacc + 1;
            end if;
            prev_stb := dbg_stb;
            cyc := cyc + 1;
            if cyc = 270270 then
                t := t + 5;
                report "HB " & integer'image(t) & "ms: adr[" & integer'image(wmin) &
                       ".." & integer'image(wmax) & "] acc=" & integer'image(wacc) &
                       " boot=" & std_logic'image(led(2)) & " disk=" & std_logic'image(led(3)) &
                       " con=" & std_logic'image(led(4)) & " idle=" & std_logic'image(led(6)) severity note;
                wmin := 524287; wmax := 0; wacc := 0; cyc := 0;
            end if;
        end loop;
        wait;
    end process;

    check : process
    begin
        wait until led(2) = '1' for 400 ms;        -- boot image loaded (slow at CLK_DIV=64)
        assert led(2) = '1'
            report "FAIL: boot sectors never loaded from SD (fail=" & std_logic'image(led(6)) & ")"
            severity failure;
        report "*** boot code loaded from xd0.dsk, CPU released ***" severity note;

        wait until loaded_ok for RUN_FOR;
        if loaded_ok then
            report "*** PASS: real booter loaded SYSTEM.BOOT (Loaded OK on console) ***" severity note;
            report "lok_seen(led7/LED8)=" & std_logic'image(led(7)) &
                   " panic(led8/LED9)=" & std_logic'image(led(8)) & " at Loaded OK" severity note;
            wait for 120 ms;    -- keep running to observe POST-"Loaded OK" behaviour
            report "POST: panic(led8/LED9)=" & std_logic'image(led(8)) &
                   " cpu_idle(led5)=" & std_logic'image(led(5)) severity note;
        else
            report "*** did not reach 'Loaded OK' (cpu_idle=" & std_logic'image(led(5)) &
                   " diskbusy=" & std_logic'image(led(3)) & ") ***" severity note;
        end if;
        done <= true;
        wait;
    end process;
end sim;

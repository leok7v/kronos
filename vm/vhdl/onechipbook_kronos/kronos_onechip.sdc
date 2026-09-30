# 21.47727 MHz oscillator on pin 28 (net CLK21M). Period = 46.560 ns.
create_clock -name clk -period 46.560 [get_ports clk]
derive_pll_clocks
derive_clock_uncertainty
set_false_path -from [get_ports rst_n] -to *
set_false_path -from * -to [get_ports {led[*]}]
# SDRAM I/O timing is tuned on hardware (PLL c1 phase + the controller's READ_LAT).
set_false_path -from * -to [get_ports uart_txd]
set_false_path -from [get_ports uart_rxd] -to *

# ---------------------------------------------------------------- CDC
# The system clock and the pixel clock are separate PLL outputs, and the ONLY
# paths between them are the console byte stream. That crossing is built as an
# asynchronous one and is correct by construction, not by static timing:
#
#   OneChipBook : cdc_byte <= vga_ch; cdc_tog <= not cdc_tog;   (system clk)
#   vga_console    : tog_s <= tog_s(1 downto 0) & ch_tog;          (pixel clk)
#                    ch_stb <= tog_s(2) xor tog_s(1);
#
# i.e. a toggle through a two-flop synchroniser plus an edge detect, with the
# DATA held stable either side for ~174us -- console bytes are that far apart at
# 57600 baud, thousands of pixel clocks. The video domain samples cdc_byte only
# after the synchronised toggle fires, long after it has settled.
#
# Analysing cdc_byte -> vga_console as a synchronous transfer is therefore
# meaningless, and it only ever "passed" by luck of the old 21.5/64.4 ratio.
# Raising the system clock to 26.85 MHz retimed that accidental alignment and it
# reported -4.690ns across ~100 paths, all of them this one crossing.
#
# Declared asynchronous rather than false-pathed per-register so that any future
# path between these domains is flagged by the CDC review it deserves, instead of
# silently inheriting an exception aimed at today's register names.
set_clock_groups -asynchronous \
    -group [get_clocks {syspll|*|pll|clk[0]}] \
    -group [get_clocks {syspll|*|pll|clk[1]}]

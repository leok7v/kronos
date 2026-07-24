# OneChipBook-12 pin assignments (authoritative)

FPGA: **Altera Cyclone EP1C12Q240** (12,060 LE, 239,616 bits M4K RAM), 240-pin PQFP.
Speed grade **C8N CONFIRMED** — the TR Rev1.01 schematic names U1 as EP1C12Q240C8N.
Source: *OneChipBook Series-12 Technical Reference, Rev 1.0* (8086YES!). All numbers below are FPGA pin numbers.

## Clock (confirmed from schematic p.17)
- **Master oscillator: 21.47727 MHz → pin 28** (`CLK0/LVDSCLK1p`, net `CLK21M`). This is the CPU clock input.
- Pin 29 (`CLK1/LVDSCLK1n`, net `EXTCLK`) = alternate external clock input.
- Pin 38 (`IO38/PLL1_OUTp`, net `MCLK`, via R8 33Ω) = **SDRAM clock, a PLL1 output** — the SDRAM is clocked by the FPGA's PLL, not the raw oscillator. Pin 39 (`PLL1_OUTn`) = `MCKE`.
- So the real design feeds pin 28 → altpll (PLL1) → CPU clock + SDRAM clock (out pin 38). The bring-up just clocks the core directly from pin 28.
- Active Serial config pins (from schematic): 24 nCSO/AS_CS, 25 DATA0/AS_DI, 36 DCLK/AS_CK, 37 ASDO/AS_DO, 26 nCONFIG.

## Configuration / programming
- **Active Serial (AS)** to an EPCS config flash via a USB Blaster (a "simplified"
  one is included as a spare; recommended cable: Intel PL-USB-BLASTER-RCN).
- Flow: Quartus `.sof` → convert to `.pof` (EPCS device) → program EPCS in **AS mode**
  → power-cycle. (No volatile JTAG SRAM port is exposed on the front panel.)
- Front-panel ASP header: 1 DCLK, 2 GND, 3 nCONFIG(? "C_DONE"), 4 NC, 5 nCONFIG,
  6 nCE, 7 DATA_in, 8 nCS, 9 DATA_out, 10 GND.

## Status LEDs (×9)  — used for the bring-up test
| LED | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|-----|---|---|---|---|---|---|---|---|---|
| pin |43 |44 |45 |46 |47 |48 |49 |50 |240|

## Reset
- **RESET, active-low → pin 153** (also on expansion-slot pin 15; asserted by FN+R on the keyboard).

## SDRAM — 32 MB, 16-bit SDR (13 addr, 2 bank; 256 Mbit)
| Data | pin | Data | pin | Addr | pin | Ctrl | pin |
|------|-----|------|-----|------|-----|------|-----|
| D0 |181| D8 |222| A0 |203| RAS# |196|
| D1 |182| D9 |219| A1 |206| CAS# |195|
| D2 |183| D10|218| A2 |207| WE#  |194|
| D3 |184| D11|217| A3 |208| CS#  |197|
| D4 |185| D12|216| A4 |235| CKE  | 39|
| D5 |186| D13|215| A5 |234| CLK  | 38|
| D6 |187| D14|214| A6 |233| LDQM |193|
| D7 |188| D15|213| A7 |228| UDQM |223|
|    |   |    |   | A8 |227| BA0  |200|
|    |   |    |   | A9 |226| BA1  |201|
|    |   |    |   | A10|202|      |   |
|    |   |    |   | A11|225|      |   |
|    |   |    |   | A12|224|      |   |

## VGA — 18-bit colour (6:6:6) via resistor DAC (shared with S-Video/CVBS by mux)
- HSYNC 75, VSYNC 74
- R[5:0] = 104,101,100,99,98,95
- G[5:0] = 94,93,88,87,86,85
- B[5:0] = 84,83,82,79,78,77

## PS/2 keyboard
- CLK 68, DATA 67  (built-in keyboard MCU is in parallel with the external PS/2 port; FN+4 toggles)

## SD card slot  (SPI-mode mapping in parentheses)
- CLK 63 (SCLK), CMD 64 (MOSI), DAT0 62 (MISO), DAT3 65 (CS), DAT1 61, DAT2 66

## Serial — two DB9 ports.  **MEASURED, 2026-07-19.  3.3 V TTL, NOT RS-232.**

The vendor Technical Reference's DB9 table is WRONG for this hardware. It says
`conn pin N → FPGA pin N, pin8 GND, pin9 VCC`; the board does not do that, and
following it cost a long debugging detour. These numbers were measured on the
real machine with a meter (black lead on the VGA connector shell) and then
confirmed by receiving actual console text:

  1# DB9 = the LEFT of the two (external rear view; the console port)
    - **pin 9 = GND**
    - **pin 2 = board TX**  (this is FPGA pin 3 = uart_txd, 57600 8N1)
    - pin 5 = +5 V VCC — KEEP A TTL ADAPTER OFF THIS PIN
    - the remaining pins read +3.3 V (unused FPGA pins are reserved
      AS INPUT TRI-STATED, so weak pull-ups hold them high, and an idle
      uart_txd sits at 3.3 V too — which is why voltage alone cannot pick
      TX out of the group; find it by connecting and power-cycling)

TX on pin 2 is **DCE** convention (the board transmits on 2, receives on 3), so
a straight-through cable to a PC is correct and a null-modem is not.

**LEVELS: the DB9 pins go straight to FPGA I/O through 100 Ω series resistors
(R9–R22 in the schematic). There is no MAX232 or any transceiver on this path.**
Use a 3.3 V USB-TTL adapter. Connecting a real ±12 V RS-232 port would destroy
the FPGA.

To receive INTO the board, set `UART_RX_EN => true` in OneChipBook and
rebuild; uart_rxd is FPGA pin 2, whose DB9 position is untested (likely pin 1 or
3 — probe it the same way).

FPGA-side pin groups, for reference: 1#DB9 uses FPGA 1..7,
2#DB9 uses FPGA 8,11,12,13,14,15,16.

## Audio — 6-bit R/L resistor DACs
- R[5:0] = 120,119,118,117,116,115 ; L[5:0] = 114,113,108,107,106,105

## USB (device)
- DP 239, DN 238

## DIP switches (×8)
- SW1..8 = 53,54,55,56,57,58,59,60. SW1 & SW2 also drive the VGA line-scan generator.

## Expansion slots (50-pin, 42 GPIO + RESET + power) — full contact map, TR Rev1.01 p.9/p.11

The INTERNAL card slot (item 17) and the EXTERNAL cartridge slot (item 4) carry
THE SAME FPGA pins in parallel (TR p.5 states it; the two tables agree) with
**ONE exception: contact 4 = FPGA 125 on the external slot but FPGA 121 on the
internal slot** (both IOs exist with their own series resistors in the
schematic, so this is a real difference, not a typo — treat contact 4 as
slot-dependent). Only one slot usable at a time; the idle slot is a stub on
every line. **Every slot IO passes through a 100Ω series resistor on the
motherboard (R51–R94).** Only TWO ground contacts (41/43) serve all 42
signals — mind simultaneous-switching noise on wide fast buses (slow slew,
consider halving the link clock if flaky).

| contact | FPGA | contact | FPGA |
|---|------|---|------|
| 1 | 122 | 2 | 123 |
| 3 | 124 | 4 | **125 ext / 121 int** |
| 5 | 126 | 6 | 127 |
| 7 | 128 | 8 | 131 (DPCLK5) |
| 9 | 132 | 10 | 133 |
| 11 | 134 | 12 | 135 |
| 13 | 136 | 14 | 137 |
| 15 | RESET (153) | 16 | 138 |
| 17 | 139 | 18 | 140 |
| 19 | 141 | 20 | 143 (PLL2_OUTn) |
| 21 | 156 | 22 | 158 |
| 23 | 159 | 24 | 160 |
| 25 | 161 | 26 | 162 |
| 27 | 163 | 28 | 164 |
| 29 | 165 | 30 | 166 |
| 31 | 167 | 32 | 168 |
| 33 | 169 | 34 | 170 (DPCLK4) |
| 35 | 173 | 36 | 174 |
| 37 | 175 | 38 | 176 |
| 39 | 177 | 40 | 178 |
| 41 | GND | 42 | 144 (PLL2_OUTp) |
| 43 | GND | 44 | 179 |
| 45 | +5V | 46 | 180 |
| 47 | +5V | 48 | **+12V** |
| 49 | Audio-L | 50 | **−12V** |

Notes:
- **PLL2_OUTp/n = FPGA 144/143 = contacts 42/20** (schematic U1: IO144/PLL2_OUTp,
  IO143/PLL2_OUTn). Contact 42 sits between the GND contacts — ideal for a
  forwarded clock output.
- DPCLK5/DPCLK4 (FPGA 131/170, contacts 8/34) are dedicated clock-network
  inputs on the Cyclone — use these if a card ever drives a clock INTO the FPGA.
- **Audio-L (contact 49) ties into the board's SOUND_L analog path** — an
  expansion card can inject audio into the built-in amplifier/speakers/volume
  knob (mono; DC-block and attenuate to the 6-bit-ladder's level range).
- ±12V on 48/50: keep card pins and probes away unless deliberately used.

## OSD / FN keys (handled by the keyboard MCU, independent of the FPGA)
FN+1 LCD↔VGA, FN+3 backlight, FN+4 built-in↔external kbd, FN+F3/F4/F5 OSD menus, **FN+R = assert RESET**.

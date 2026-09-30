# Kronos on the OneChipBook (Altera Cyclone EP1C12)

Port of the Kronos CPU **and the real Excelsior OS** to the OneChipBook 1.2 — a
retro-style FPGA workstation ([OneChipBook-12 by 8086YES!](https://www.tindie.com/products/cycle/onechipbook-12-a-fpga-development-platform-2/))
built around an Altera Cyclone **EP1C12** (2011-era Cyclone I) with 32 MB SDRAM,
1024×768 VGA, a PS/2 port and a SD slot.

It is a working interactive workstation: the board boots the genuine Excelsior iV
OS from the SD card into SDRAM, brings up a VGA text console and a PS/2 keyboard,
lets you log in and run commands, and reads **and writes** the SD card — all on
real silicon. The Kronos C toolchain even self-hosts on it.

## What works (on hardware)

- **Boots the real OS** from SD into SDRAM (address bus widened to 23 bits →
  up to **31.75 MB** usable; see the RAM-extension notes in the design doc).
- **VGA text console** — 1024×768, 128×50 chars, 8×14 font, ANSI escapes + scrolling.
  The system clock *is* the pixel clock (no separate video PLL needed for text).
- **PS/2 keyboard** — scan-set-2 decoded into the DL11 console receiver.
- **Data cache** — 2-way set-associative, write-back (best ~15 k dhrystones, ≈7×
  the uncached core).
- **SD** — multi-partition card (MBR): boot + system + a native Excelsior
  sources volume + a FAT16 exchange partition. Reads and writes.
- **Serial console** — bidirectional 3.3 V TTL @ 57600 on the DB9 (see pinout doc).
- **Suspend-to-RAM** sleep mode (SDRAM self-refresh; wake on any PS/2 key).
- **Real-time clock** via the io2 bridge (survives disk access; year/day/time).

Default build config (in [src/OneChipBookTop.vhdl](src/OneChipBookTop.vhdl)):
system PLL on at **25.77 MHz** (`SYS_PLL_ON`, ×6/5 of the 21.477 MHz oscillator)
and suspend enabled (`SUSPEND_EN`). Flip those generics off to fall back to the
21.477 MHz / no-sleep configuration.

## Performance

The write-back data cache is what makes the OneChipBook usable — it took Dhrystone from
**2,157 to ~15,000 runs/s, about 7×**. Getting there was hard-won:

| Configuration | Dhrystones/s |
|---------------|--------------|
| No cache (SDRAM on every access) | 2,157 |
| Write-back, **direct-mapped** | ~10,700 best — but **8,700–12,200** run-to-run |
| Write-back, **2-way** set-associative | ~12,200 (run-to-run spread cut from 29% → 10%) |
| 2-way @ **25.77 MHz** (×6/5 PLL) | **14,976 mean, 15,060 best** (16 runs) ≈ **7×** |

The wild direct-mapped spread was **conflict misses, not measurement noise** —
performance tracked the miss count exactly, jumping in discrete steps. Only once
associativity tamed the misses did the clock bump pay off cleanly. (Lesson learned the
hard way: take ≥8 runs before trusting any single Dhrystone number on this machine.)

## The two buildable projects

| Quartus project | Top entity | Purpose |
|-----------------|------------|---------|
| `kronos_onechip` | `OneChipBookTop` | **The workstation** — CPU + SDRAM + cache + microcode + `sd_disk_controller` + DL11 console + VGA + PS/2. Boots the OS from SD. |
| `expansion_beacon`   | `expansion_beacon` | Expansion-slot pin-ID beacon — transmits each expansion-slot contact's FPGA pin number over UART to map the connector. |

(The earlier bring-up projects — core-fit, SDRAM, and SD diagnostics — were
one-shot scaffolding and have been removed now that the full design subsumes them.
They remain in git history.)

## Prerequisites

- **Quartus II 11.0sp1** — the last Quartus that supports the original Cyclone
  (EP1C12). Install it on a modern Linux with
  [tools/install_quartus11_linux.sh](tools/install_quartus11_linux.sh).
- **A USB-Blaster** (or clone) — to program the on-board EPCS serial flash.
- **A SD card** — holds the OS and disk images.
- **Python 3** — for the SD-card / volume builders (`mkxd.py`, `mkcard.py`) and the
  `tools/` scripts.
- **GHDL** — optional, for running the simulation before you flash.

## Quick start — build a Kronos workstation

From a fresh checkout to a booting board (paths are relative to this directory):

1. **Install Quartus 11.0** (once):
   ```sh
   bash tools/install_quartus11_linux.sh
   ```
2. **Build the bitstream, then the EPCS flash image** (the second step is required —
   `--flow compile` does *not* regenerate the `.pof`):
   ```sh
   ~/altera/11.0/quartus/bin/quartus_sh --flow compile kronos_onechip
   quartus_cpf -c -d EPCS4 output_files/kronos_onechip.sof output_files/kronos_onechip.pof
   ```
3. **Flash it:** program `output_files/kronos_onechip.pof` to the on-board EPCS with the
   USB-Blaster, unplug the Blaster, and power-cycle so the FPGA configures from flash.
4. **Build the SD** from the ready-made boot + system volumes (add `--xd2` with a
   sources volume if you want the Excelsior sources on the card — see *Build the SD
   card* below):
   ```sh
   python3 mkcard.py --xd0 ../../../excelsior/xd/xd0.dsk \
                     --xd1 ../../../excelsior/xd/xd1.dsk --out card.img
   sudo dd if=card.img of=/dev/sdX bs=1M      # /dev/sdX = your card, double-check it!
   ```
5. **Boot:** insert the SD and power on. At the `username:` prompt type **`sys`**
   and you are logged in to the Excelsior shell.

Recommended before flashing: run the simulation (see [Simulate](#simulate)) to confirm
the build boots to *"Loaded OK"*. The sections below detail each step.

## Build (Quartus)

The EP1C12 is the *original* Cyclone, supported only up to **Quartus II 11.0sp1**
(dropped in 11.1; 13.0sp1 is the last for Cyclone *II*, not I) — not modern Quartus
Prime. Device is `EP1C12Q240C8` (no "N" suffix in 11.0; C8 = slowest/safest grade).
Pins come from the board schematic — see [doc/ONECHIPBOOK_PINOUT.md](doc/ONECHIPBOOK_PINOUT.md).

Headless compile:

```sh
~/altera/11.0/quartus/bin/quartus_sh --flow compile kronos_onechip
```

**Then build the EPCS configuration image explicitly** — `--flow compile` does *not*
regenerate the serial-flash `.pof`, so a stale image gets flashed if you skip this:

```sh
quartus_cpf -c -d EPCS4 output_files/kronos_onechip.sof output_files/kronos_onechip.pof
```

Program the `.pof` to the on-board EPCS with the USB-Blaster, unplug the Blaster,
and power-cycle so the FPGA configures from flash.

## Simulate

```sh
cd sim && bash compile_onechip.sh          # boots the modelled SD boot chain
bash run_sdram_checks.sh                        # SDRAM controller / suspend checks
```

The sim uses the `*_sim.vhdl` CPU variants (the real `cpu/` files don't compile
under GHDL due to a textio `write()` overload — Quartus is unaffected) plus the same
inferred Altera memory the synthesis build uses.

## Build the SD card

The ready-made boot (`xd0.dsk`) and system (`xd1.dsk`) volumes live in
[`excelsior/xd`](../../../excelsior/xd). `mkcard.py` assembles them into one
partitioned image; `mkxd.py` builds extra native Excelsior volumes if you want them.

```sh
# minimal: boot + system only
python3 mkcard.py --xd0 ../../../excelsior/xd/xd0.dsk \
                  --xd1 ../../../excelsior/xd/xd1.dsk --out card.img

# optional: also carry the Excelsior sources as a third volume (unit 2)
python3 mkxd.py create src.dsk --size-mb 128 --label src
python3 mkxd.py import src.dsk ../../../excelsior/src
python3 mkcard.py --xd0 ../../../excelsior/xd/xd0.dsk \
                  --xd1 ../../../excelsior/xd/xd1.dsk --xd2 src.dsk --out card.img

sudo dd if=card.img of=/dev/sdX bs=1M      # write to the SD (check /dev/sdX!)
```

`mkcard.py` also lays down a FAT16 exchange partition (`--fat-mb`, default 64) so you
can move files between the board and a PC. `card.img` and the `*.dsk` volume images are
build outputs (git-ignored).

## Tooling

Host-side scripts for building, flashing, and driving the port:

| Script | Purpose |
|--------|---------|
| `mkxd.py` | Create and populate native Excelsior (XD) filesystem volumes host-side (the OS's own on-disk format). |
| `mkcard.py` | Assemble the partitioned SD image (MBR: boot + system + sources + FAT) from the XD volumes. |
| `tools/gen_font_rom.py` | Generate [`src/font_rom.vhdl`](src/font_rom.vhdl), the 8×14 text-console font ROM. |
| `tools/patch_columns.py` | Patch the terminal column constant (`tt_reset`) in a compiled `.cod` so the console uses all 128 columns (the OS ships configured for 80). |
| `tools/uart_capture.py` | Record the board's console stream off the UART, byte-exact (for replay/debugging). |
| `tools/install_quartus11_linux.sh` | Install Quartus II 11.0 on a modern Linux (works around the 2011 32-bit installer crash). |
| `sim/gen_xd0_dec.py` | Convert `xd0.dsk` (raw SD image) into `xd0.dec` for the sim's `sd_model_xd0`. |
| `sim/gen_mbrtest.py` | Build a tiny partitioned SD image for the MBR-parser testbench (`tb_mbr`). |
| `sim/gen_part_sim.py` | Build a partitioned reproduction image for the OneChipBook boot sim. |
| `os_util/` | Excelsior-side Modula sources (`sleep`, `sleeptime`) for the suspend-to-RAM feature. |

## Directory layout

```
src/                 synthesizable VHDL (OneChipBook* top, cache, SDRAM, DL11,
                     VGA console, PS/2, sd_disk_controller, suspend, PLLs, font ROM)
sim/                 GHDL testbenches + compile scripts (onechip boot, SDRAM, suspend)
tools/               host utilities (font ROM gen, column patcher, UART capture,
                     Quartus 11 installer)
os_util/             Excelsior-side Modula sources (sleep/suspend helpers)
doc/                 design documentation (see below)
mkcard.py, mkxd.py   SD card + Excelsior-volume image builders
*.qpf / *.qsf / *.sdc Quartus projects for the two buildable designs
```

## Design docs

- [doc/DESIGN.md](doc/DESIGN.md) — the OneChipBook architecture: the Kronos
  instruction-set gap, VM semantics, boot flow, MBR disk layout, and the bring-up
  debugging record.
- [doc/ONECHIPBOOK_PINOUT.md](doc/ONECHIPBOOK_PINOUT.md) — authoritative FPGA pin map
  (core I/O, VGA, PS/2, SD, and the expansion-slot contacts).
- [doc/SUSPEND_TO_RAM.md](doc/SUSPEND_TO_RAM.md) — the sleep-mode design (SDRAM
  self-refresh + inactivity sequencer + wake logic).

# Kronos CPU on FPGA

Kronos is a 32-bit stack/RISC CPU designed in Novosibirsk (1984–1991) to run the
Excelsior iV operating system. This repository pairs the upstream **Kronos3vm**
software emulator and the full **Excelsior OS** sources with FPGA implementations of
the CPU, plus a resurrected **Kronos C toolchain**.

The vendor-neutral Kronos core lives in [vm/vhdl/5.0](vm/vhdl/5.0); each board
implementation adapts that shared core to specific silicon.

## Prerequisites

Different parts of the repo need different tools:

| To build… | You need |
|-----------|----------|
| The **Kronos3vm emulator** ([vm/int](vm/int)) | CMake ≥ 3.5, SDL2, a C++ compiler (gcc/clang) |
| An **FPGA bitstream** | the board's vendor toolchain — the OneChipBook needs **Quartus II 11.0sp1** (see [its README](vm/vhdl/onechipbook_kronos/README.md)) |
| **Simulations** (GHDL testbenches) | GHDL (`--std=93`) |
| The **SD-card / disk-image builders** and host tools | Python 3 |
| The **Kronos C toolchain** ([vm/pcc-host](vm/pcc-host)) | a host C compiler (gcc) — see `vm/pcc-host/build.sh` |

## FPGA implementations

| Target | Board / FPGA | Status |
|--------|--------------|--------|
| **Xilinx Spartan-3** ([vm/vhdl/5.0/board](vm/vhdl/5.0/board)) | Digilent Spartan-3 Starter Board ([`DigilentSpartan3StarterBoard.vhdl`](vm/vhdl/5.0/board/DigilentSpartan3StarterBoard.vhdl)) and Xilinx SP305 ([`XilinxSpartan3SP305.vhdl`](vm/vhdl/5.0/board/XilinxSpartan3SP305.vhdl)) | The reference FPGA implementations (Kronos + serial + ATA). |
| **OneChipBook** ([vm/vhdl/onechipbook_kronos](vm/vhdl/onechipbook_kronos)) | OneChipBook 1.2 — Altera Cyclone EP1C12, 32 MB SDRAM, VGA, PS/2, SD | Boots the real Excelsior OS interactively on hardware. |

## OneChipBook port

The OneChipBook port takes the Kronos core to an Altera Cyclone EP1C12 board and boots
the genuine Excelsior OS from an SD card into SDRAM, running it as an interactive
workstation on real silicon — VGA text console, PS/2 keyboard, SD read/write, a 2-way
write-back data cache (≈7× the uncached core — ~15,000 Dhrystones), suspend-to-RAM, and
a self-hosting C compiler, all on the FPGA.
Its Cyclone-adapted VHDL (cache and memory copies) lives in its own `src/` so the
shared core is never edited in place.

- **[vm/vhdl/onechipbook_kronos/README.md](vm/vhdl/onechipbook_kronos/README.md)** —
  what works, the **prerequisites and step-by-step build/flash/SD-card guide**, how to
  simulate, and the host tooling.
- **[vm/vhdl/onechipbook_kronos/doc/DESIGN.md](vm/vhdl/onechipbook_kronos/doc/DESIGN.md)**
  — the design + bring-up story (the Kronos instruction-set gap, boot flow, disk
  layout, and how the boot fault was root-caused and fixed).

## The Excelsior OS

Kronos3vm is a software emulator of the Kronos computer; it runs Excelsior iV and
ships with the full OS source, ready for system builds.

- **Sources:** [`excelsior/src`](excelsior/src) — the complete Excelsior iV system
  (kernel `sys/`, firmware `fw/`, shell + utilities, fonts, and user accounts under
  `users/`).
- **Documentation:** [`excelsior/src/doc`](excelsior/src/doc) — the Excelsior manuals
  (`adm`, `ar1`–`ar3`, `lib`, `pr`, `shell`, `util1`/`util2`).
- **Disk images:** [`excelsior/xd`](excelsior/xd) — the bootable OS volumes
  (`xd0.dsk` = boot, `xd1.dsk` = system/root).

### Running it under the emulator

The [`vm/int`](vm/int) emulator is the reference oracle the FPGA work is validated
against (a portable CMake/SDL build sits alongside the original Visual Studio project):

```sh
cd vm/int && cmake -B build && cmake --build build
./build/kronos_vm ../../excelsior/xd/xd0.dsk ../../excelsior/xd/xd1.dsk
```

At the `user:` prompt, log in as **`sys`** — you'll get a shell prompt like
`17:12 sys !` (the time, the current directory, and `!`). Type `ls` to look around.

### Using the OS

The Excelsior command interface is Unix-like:

- **`ls`**, **`cd`**, **`cp`**, **`rm`**, **`ln`**, **`cat`** — the familiar file utilities.
- **`kc`** — a Norton-Commander-style file manager (more convenient than raw `ls`/`cd`).
- **`ex`** — the text editor.
- **`mx`** — the Modula-2 compiler (the docs call it `m2`, which is now outdated).
- Executables are `.cod` files — **run one by typing its name without the extension**.
  Scripts have the `.@` or `.sh` extension.
- Most utilities print usage with the `-h` switch.

## The Kronos C toolchain

[`vm/pcc-host/`](vm/pcc-host) is the Kronos **pcc** C compiler, cross-built on Linux
and emitting Kronos `.cod`. It self-hosts: a C program can be compiled entirely on the
Excelsior OS (cpp → pcc → a_asm → clink) and the resulting `.cod` loads and runs. The
[`ctool_scripts/`](ctool_scripts) are the on-board driver scripts, and
`vm/pcc-host/ktool/` packages the compiler + runtime + headers into an SD volume.

## Repository map

```
excelsior/            Excelsior OS sources (src/), docs (src/doc/), disk images (xd/)
vm/
  int/                Kronos3vm software emulator (VS project + portable CMake/SDL build)
  pcc-host/           Kronos C compiler host toolchain (cpp / pcc / a_asm / clink)
  uart/               UART host tools (serial console + file transfer)
  xdu/, client/       disk / client utilities (upstream)
  vhdl/
    5.0/              vendor-neutral Kronos core + Xilinx Spartan-3 reference boards
    onechipbook_kronos/   OneChipBook (Cyclone EP1C12) port
ctool_scripts/        on-board C-compiler driver scripts
```

## Upstream

- Kronos: <https://kronos.ru/>
- Repo: <https://github.com/leok7v/kronos> (this fork adds the FPGA ports and C toolchain)

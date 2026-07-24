#!/bin/bash
# Build + run the OneChipBook sim: OneChipBook (CPU + SDRAM + sd_disk_controller +
# DL11 console) boots the REAL xd0.dsk boot chain from the modelled SD card.
# Needs xd0.dec (python3 gen_xd0_dec.py). Same *_sim CPU substitution story as
# the other OneChipBook sims. Pass a --stop-time to override the default.
set -e
cd "$(dirname "$0")"
CPU=../../5.0/cpu
SRC=../src
STOP=${1:---stop-time=500ms}
[ -f xd0.dec ] || python3 gen_xd0_dec.py
rm -rf work_ocb && mkdir work_ocb
W="--workdir=work_ocb"
STD="--std=93 -fsynopsys -fexplicit -frelaxed"
STD0="--std=93 -fsynopsys -frelaxed"

echo "-- inferred memory --"
ghdl -a $STD  $W BlockRam_sim.vhdl $SRC/DistrRam.vhdl
echo "-- CPU core (microcode real; logic via *_sim) --"
# NOTE: the VM-convention Microcode (assembled from $SRC/Kronos.mkas by mkas.py),
# NOT the 5.0 one -- xd0.dsk needs io0/io1/io2/wmv/rcmp at the Kronos-3 numbers
ghdl -a $STD0 $W $CPU/uCmdBits.vhdl $CPU/KronosTypes.vhdl $SRC/Microcode.vhdl
# ONE cache source. src/DataCacheFM.vhdl is now the only copy -- its debug
# trace process (the sole thing GHDL could not compile) was removed, so the
# duplicate sim/DataCacheFM_realcache.vhdl is gone. Those two copies had
# drifted, and toggling the bypass in one while simulating the other made a
# whole evening of cache experiments compare a build against itself.
ghdl -a $STD  $W $CPU/ALU.vhdl Decode_sim.vhdl InstructionCache_sim.vhdl \
                $SRC/DataCacheFM.vhdl DataStorage_ocb_sim.vhdl Kronos_sim.vhdl
echo "-- SD disk + DL11 + SDRAM + top + tb --"
# TURBO=1 swaps in sd_reader_turbo (SIM-ONLY drop-in: serves sectors from
# xd0.dec directly, no SPI bit-banging) -> a full boot to "Loaded OK" runs in
# ~40 min instead of hours. Leave it unset for the FAITHFUL build (real
# sd_reader) when SPI timing itself is what's under test.
READER=${TURBO:+sd_reader_turbo.vhdl}
READER=${READER:-$SRC/sd_reader.vhdl}
echo "   reader: $READER"
# Console/keyboard/PLL were added to OneChipBook after this script was
# written, so the OneChipBook sim had silently stopped building. video_pll is fine to
# ANALYSE (its altpll sits inside a generate that USE_PLL=false switches off).
ghdl -a $STD  $W $READER $SRC/sd_disk_controller.vhdl $SRC/dl11_console.vhdl \
                sd_model_xd0.vhdl $SRC/sdram_controller.vhdl sdram_model.vhdl \
                $SRC/font_rom.vhdl $SRC/vga_console.vhdl \
                $SRC/ps2_keyboard.vhdl $SRC/console_selftest.vhdl $SRC/perf_report.vhdl \
                $SRC/video_pll.vhdl $SRC/suspend_ctrl.vhdl \
                $SRC/OneChipBook.vhdl tb_onechip.vhdl
ghdl -e $STD  $W tb_onechip
echo "-- run ($STOP) --"
# NOTE: do NOT put IOACC in the default filter. Every io2 DATA word drained is
# one IOACC line (~1024 per disk op), so it blows past any head limit the moment
# the OS load starts -- head then closes the pipe, SIGPIPEs ghdl, and the run
# dies right after "512KB memory" looking like a boot failure when it isn't.
# Set IOTRACE=1 to include it (and expect a flood). head limit is high enough
# that a normal run is never truncated.
FILT="HB |MMCON|FIRST|CONSOLE|PASS|FAIL|\*\*\*|failure"
[ -n "$IOTRACE" ] && FILT="$FILT|IOACC"
ghdl -r $STD  $W tb_onechip $STOP 2>&1 | grep -aE "$FILT" | head -20000

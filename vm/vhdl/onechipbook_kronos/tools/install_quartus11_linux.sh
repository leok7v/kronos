#!/bin/bash
# Install Quartus II 11.0 Web Edition (the LAST version supporting the original
# Cyclone / EP1C12) headlessly on a modern 64-bit Linux (tested on Ubuntu 26.04).
#
# Why this is needed: the 2011 installer is a 32-bit PyInstaller bundle whose
# Python crashes with "ImportError: No module named _md5". Two old-vs-new library
# conflicts cause it:
#   1. its bundled _hashlib.so won't dlopen (needs OpenSSL 0.9.8 symbols), so
#      Python's hashlib falls back to a _md5 module that isn't in the bundle;
#   2. its bundled old libz.so.1 shadows the system zlib, so the newer 32-bit
#      libpng16 fails to resolve inflateReset2 when PyQt is imported.
# Fix: LD_PRELOAD the installer's own OpenSSL (libcrypto/libssl .so.4) AND the
# system 32-bit zlib, and call altera_installer_cmd directly (the `setup` wrapper
# uses bash-isms that break under dash).
#
# Prereqs: i386 multiarch + 32-bit libc/zlib/png:
#   sudo dpkg --add-architecture i386 && sudo apt update
#   sudo apt install libc6:i386 libz1:i386 libpng16-16:i386 libfontconfig1:i386 \
#                    libsm6:i386 libxrender1:i386 libxext6:i386
#
# Usage: install_quartus11_linux.sh <extracted-installer-dir> [target-dir]
#   <extracted-installer-dir> = where 11.0_quartus_free_linux.sh unpacked to
#   (contains altera_installer/, devices/, linux_installer/, setup).
set -e
SRC="${1:?usage: $0 <extracted-installer-dir> [target-dir]}"
TARGET="${2:-$HOME/altera/11.0}"
BIN="$SRC/altera_installer/bin"
ZLIB="$(ldconfig -p | awk '/libz\.so\.1 \(libc6\)/{print $NF; exit}')"
ZLIB="${ZLIB:-/usr/lib/i386-linux-gnu/libz.so.1}"

export LD_LIBRARY_PATH="$BIN"
export LD_PRELOAD="$BIN/libcrypto.so.4:$BIN/libssl.so.4:$ZLIB"

echo "Installing Quartus II 11.0 Web Edition (Cyclone I) -> $TARGET"
exec "$BIN/altera_installer_cmd" \
    --source="$SRC" \
    --install=quartus_free \
    --web \
    --target="$TARGET" \
    --force \
    --no_space_check

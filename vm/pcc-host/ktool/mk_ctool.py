#!/usr/bin/env python3
"""Build an Excelsior XD volume carrying the native Kronos C toolchain, so C
sources compile to executables ON the Kronos machine.

The toolchain is the pcc port running natively as `.cod`:
    cpp (+cpy)  ->  p_code (+p_cgram..p_trees)  ->  a_asm (+a_asm1/2)  ->  lnk
plus the C runtime (clib/c_run/k_run/env) and clib.lib, and the C headers.

Unlike mkxd.py's `import`, this stores .c/.h RAW (LF): the 1988 cpp expects LF
line endings, NOT the Excelsior RS (0x1e) convention. .cod/.lib are binary.

Usage:
    # standalone toolchain volume (everything at the root) -- mount as /usr1:
    ./mk_ctool.py --out ctool.dsk

    # nest the toolchain under /c on a fresh sources volume that also holds
    # an Excelsior source tree (RS-converted) -- this is the xd2 for the SD card:
    ./mk_ctool.py --out xd2.dsk --size-mb 128 --src ../../../excelsior/src

On the machine (xd2 mounted at /usr1):
    cd /usr1/c
    cpp foo.c -o foo.i
    p_code foo.i foo.a
    a_asm foo.a
    lnk foo.o
    foo
"""
import argparse, importlib.util, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
# load mkxd.py (the Excelsior FS writer) as a module
_mkxd_path = os.path.normpath(os.path.join(HERE, "..", "..", "vhdl",
                                           "onechipbook_kronos", "mkxd.py"))
_spec = importlib.util.spec_from_file_location("mkxd", _mkxd_path)
mk = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(mk)


def toolchain_files():
    """(name, host_path) for every file the toolchain volume must carry."""
    out = []
    for sub in ("bin", "lib", "include", "examples"):
        d = os.path.join(HERE, sub)
        for name in sorted(os.listdir(d)):
            out.append((name, os.path.join(d, name)))
    return out


def add_raw_dir(v, files, parent_ino, t):
    """Create a directory inode holding `files` stored RAW (no RS conversion);
    return its inode number."""
    ino = v.alloc_inode()
    entries = [("..", parent_ino, mk.D_DIR | mk.D_HIDDEN)]
    for name, path in files:
        raw = open(path, "rb").read()          # RAW: keep LF for the C toolchain
        entries.append((name, v.store_file(raw, t, t), mk.D_FILE))
    v.write_dir(ino, entries, t, t)
    return ino


def main():
    ap = argparse.ArgumentParser(description="Build the Kronos C-toolchain XD volume")
    ap.add_argument("--out", default="ctool.dsk", help="output .dsk image")
    ap.add_argument("--size-mb", type=int, default=8, help="volume size (MB)")
    ap.add_argument("--label", default="ctool", help="volume label (<=7 chars)")
    ap.add_argument("--src", default=None,
                    help="also import this Excelsior source tree at the root "
                         "(RS-converted); the toolchain then goes under /c")
    args = ap.parse_args()

    t = mk.now_ktime()
    files = toolchain_files()
    v = mk.Volume.blank(args.size_mb, args.label, t)

    if args.src:
        # sources at root (RS-converted .m/.d), toolchain under /c (raw LF)
        cino = add_raw_dir(v, files, 0, t)
        stats = {"files": 0, "dirs": 0, "bytes": 0, "skipped": [], "truncated": []}
        root = []
        for name in sorted(os.listdir(args.src)):
            path = os.path.join(args.src, name)
            if os.path.islink(path):
                continue
            if os.path.isdir(path):
                root.append((name, v.import_dir(path, 0, stats), mk.D_DIR))
            elif os.path.isfile(path):
                raw = v._read_host_file(path)   # RS-converts .m/.d etc.
                root.append((name, v.store_file(raw, t, t), mk.D_FILE))
        root.append(("c", cino, mk.D_DIR))
        v._extend_root(root)
        where = "/c (%d toolchain files) + sources at root" % len(files)
    else:
        # standalone: toolchain at the root
        entries = [(name, v.store_file(open(p, "rb").read(), t, t), mk.D_FILE)
                   for name, p in files]
        v._extend_root(entries)
        where = "%d toolchain files at root" % len(files)

    v.save(args.out)
    print("built %s (%d MB, label %r): %s" % (args.out, args.size_mb, args.label, where))
    print("blocks free: %d / %d ; inodes free: %d / %d"
          % (v.blocks_free(), v.b_no, v.inodes_free(), v.i_no))


if __name__ == "__main__":
    main()

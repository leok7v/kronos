#!/usr/bin/env python3
"""mkxd.py -- create and populate native Excelsior (XD) filesystem volumes.

The Excelsior on-disk format is reverse-engineered from the OS source
(excelsior/src/sys/os/osFileSystem.m: make_volume / new_super / mount_super)
and cross-checked against the reference host reader (vm/xdu/src/xduDisk.c) and
against a real volume (xd1.dsk: the OS inode formula size-size/64-2 rounded to
x64 reproduces its 5184 inodes exactly).

Volume layout (4096-byte blocks == 1024 little-endian int32 words):
    block 0                : cold booter (zeroed here -- a data volume, not boot)
    block 1 ..             : superblock -- 16-word LABEL header, then the block
                             free-map (bset) and inode free-map (iset).
                             A bit that is SET (1) means FREE; clear (0) = busy.
    block inoL .. inoH     : inode table, 64 inodes/block, 64 bytes each
    block inoH+1 ..        : file/dir data blocks

    LABEL (16 words): word0-1 = 8-char volume label,
                      word4 = inode count, word5 = block count,
                      word6 = magic 'CXE' (0x00455843), word7 = creation time.
    inode count = blocks - blocks/64 - 2, rounded up to a multiple of 64
    inoL = 1 + ceil((bset_words + iset_words + 16) / 1024)   (super spans >=1 block)
    inoH = inoL + ceil(inode_count / 64) - 1
    root directory is inode 0; its data block is inoH+1; inodes 1,2 are the
    mandatory SYSTEM.BOOT / BAD.BLOCKS system files.

Kronos time = seconds since 1986-01-01 00:00:00.

Commands:
    mkxd.py info   <vol.dsk>
    mkxd.py ls     <vol.dsk>
    mkxd.py create <vol.dsk> --size-mb N [--label NAME]
    mkxd.py import <vol.dsk> <host-dir> [--as NAME]
"""
import argparse
import os
import struct
import sys
from datetime import datetime, timezone

BLOCK = 4096            # bytes per FS block
WORDS = BLOCK // 4      # 1024 int32 words per block
LABEL = 16              # words of superblock header before the bitmaps
INODE_SZ = 64           # bytes per inode (16 words)
INODES_PER_BLOCK = BLOCK // INODE_SZ            # 64
DNODE_SZ = 64           # bytes per directory entry
CXE = 0x00455843        # superblock magic ('C','X','E',0 little-endian)
MAXFILE_BLOCKS = WORDS  # a "long" file indexes at most 1024 blocks == 4 MB
RS = 0x1e               # Excelsior text line separator

# inode.mode bits
I_DIR = 2
I_LONG = 4
# dnode.kind bits
D_DEL = 1
D_FILE = 2
D_DIR = 4
D_HIDDEN = 8
D_ESC = 16
D_SYS = 32

# Extensions stored as Excelsior text (UTF-8 -> KOI8-R, newline -> RS).
# Everything else is copied as raw bytes so binaries (.sym/.cod/.obj/.fnt/...)
# are never corrupted.
TEXT_EXTS = {
    ".m", ".d", ".def", ".mod", ".c", ".h", ".@", ".mas", ".s", ".asm",
    ".doc", ".txt", ".red", ".msg", ".lst", ".map", ".cmd", ".pkg",
}

EPOCH = datetime(1986, 1, 1, tzinfo=timezone.utc)


def ceil_div(a, b):
    return (a + b - 1) // b


def ktime_of(unix_ts):
    return int((datetime.fromtimestamp(unix_ts, timezone.utc) - EPOCH).total_seconds())


def now_ktime():
    return int((datetime.now(timezone.utc) - EPOCH).total_seconds())


class Inode:
    __slots__ = ("ref", "mode", "links", "eof", "ctime", "wtime", "pro", "gen", "rfe")

    def __init__(self, words):
        self.ref = list(words[0:8])
        self.mode = words[8]
        self.links = words[9]
        self.eof = words[10]
        self.ctime = words[11]
        self.wtime = words[12]
        self.pro = words[13]
        self.gen = words[14]
        self.rfe = words[15]

    def is_dir(self):
        return (self.mode & I_DIR) != 0

    def is_long(self):
        return (self.mode & I_LONG) != 0


class Volume:
    """An Excelsior volume image held fully in memory (read + write)."""

    # ---------------------------------------------------------------- load ----
    def __init__(self, data):
        self.data = bytearray(data)
        if len(self.data) % BLOCK != 0:
            raise ValueError("image size %d is not a multiple of %d" %
                             (len(self.data), BLOCK))
        self.total_blocks = len(self.data) // BLOCK
        self._parse_super()
        self._blk_cursor = self.inoH + 2
        self._ino_cursor = 3

    @classmethod
    def load(cls, path):
        with open(path, "rb") as f:
            return cls(f.read())

    def save(self, path):
        with open(path, "wb") as f:
            f.write(self.data)

    # ------------------------------------------------------------- helpers ----
    def _wr(self, byte_off, val):
        struct.pack_into("<i", self.data, byte_off, val)

    def _rd(self, byte_off):
        return struct.unpack_from("<i", self.data, byte_off)[0]

    def _parse_super(self):
        sb = BLOCK  # superblock starts at block 1
        self.label = self.data[sb:sb + 8].split(b"\0")[0].decode("latin1")
        self.i_no = self._rd(sb + 4 * 4)
        self.b_no = self._rd(sb + 5 * 4)
        self.magic = self._rd(sb + 6 * 4)
        self.ctime = self._rd(sb + 7 * 4)
        self.bset_words = ceil_div(self.b_no, 32)
        self.iset_words = ceil_div(self.i_no, 32)
        self.bset_off = sb + LABEL * 4
        self.iset_off = self.bset_off + self.bset_words * 4
        span_words = self.bset_words + self.iset_words + LABEL
        self.supsz_words = ceil_div(span_words, WORDS) * WORDS
        self.inoL = 1 + ceil_div(span_words, WORDS)
        self.inoH = self.inoL + ceil_div(self.i_no, INODES_PER_BLOCK) - 1

    # ---- free-map bits: SET == free, CLEAR == busy ----
    def _get_bit(self, off, i):
        word = struct.unpack_from("<I", self.data, off + (i // 32) * 4)[0]
        return (word >> (i % 32)) & 1

    def _clear_bit(self, off, i):
        p = off + (i // 32) * 4
        word = struct.unpack_from("<I", self.data, p)[0]
        struct.pack_into("<I", self.data, p, word & ~(1 << (i % 32)))

    def _fill_free(self, off, nbits):
        words = ceil_div(nbits, 32)
        self.data[off:off + words * 4] = b"\xff\xff\xff\xff" * words
        tail = nbits % 32
        if tail:  # clear the non-existent bits in the last word (mount_super does this)
            p = off + (words - 1) * 4
            struct.pack_into("<I", self.data, p, (1 << tail) - 1)

    def blocks_free(self):
        return sum(self._get_bit(self.bset_off, i) for i in range(self.b_no))

    def inodes_free(self):
        return sum(self._get_bit(self.iset_off, i) for i in range(self.i_no))

    # ---- inodes ----
    def _inode_off(self, n):
        return (self.inoL + n // INODES_PER_BLOCK) * BLOCK + (n % INODES_PER_BLOCK) * INODE_SZ

    def inode(self, n):
        return Inode(struct.unpack_from("<16i", self.data, self._inode_off(n)))

    def _write_inode(self, n, ref, mode, links, eof, ctime, wtime):
        r = list(ref) + [0] * (8 - len(ref))
        struct.pack_into("<16i", self.data, self._inode_off(n),
                         r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
                         mode, links, eof, ctime, wtime, 0, 0, 0)

    def file_blocks(self, ino):
        n = ceil_div(ino.eof, BLOCK)
        if ino.is_long():
            idx = ino.ref[0] * BLOCK
            return [self._rd(idx + i * 4) for i in range(n)]
        return ino.ref[:n]

    def read_file(self, ino):
        out = bytearray()
        for b in self.file_blocks(ino):
            out += self.data[b * BLOCK:(b + 1) * BLOCK]
        return bytes(out[:ino.eof])

    def read_dir(self, ino):
        raw = self.read_file(ino)
        out = []
        for off in range(0, len(raw), DNODE_SZ):
            name = raw[off:off + 32].split(b"\0")[0].decode("koi8_r", "replace")
            inode_no = struct.unpack_from("<i", raw, off + 48)[0]
            kind = struct.unpack_from("<i", raw, off + 52)[0]
            if kind & D_DEL or not (kind & (D_FILE | D_DIR | D_ESC)):
                continue
            out.append((name, inode_no, kind))
        return out

    # ----------------------------------------------------------- allocate ----
    def alloc_block(self):
        i = self._blk_cursor
        while i < self.b_no:
            if self._get_bit(self.bset_off, i):
                self._clear_bit(self.bset_off, i)
                self._blk_cursor = i + 1
                return i
            i += 1
        raise RuntimeError("out of free blocks (volume too small)")

    def alloc_inode(self):
        i = self._ino_cursor
        while i < self.i_no:
            if self._get_bit(self.iset_off, i):
                self._clear_bit(self.iset_off, i)
                self._ino_cursor = i + 1
                return i
            i += 1
        raise RuntimeError("out of free inodes (volume too small)")

    def _store_blocks(self, raw):
        """Allocate blocks for `raw`, write it, return (ref[8], mode_long_flag)."""
        n = max(1, ceil_div(len(raw), BLOCK))
        if n > MAXFILE_BLOCKS:
            raise ValueError("file needs %d blocks (>%d, >4MB)" % (n, MAXFILE_BLOCKS))
        blocks = [self.alloc_block() for _ in range(n)]
        for k, b in enumerate(blocks):
            chunk = raw[k * BLOCK:(k + 1) * BLOCK]
            self.data[b * BLOCK:b * BLOCK + len(chunk)] = chunk
        if n > 8:
            idx = self.alloc_block()
            for j, b in enumerate(blocks):
                self._wr(idx * BLOCK + j * 4, b)
            return [idx], I_LONG
        return blocks, 0

    def store_file(self, raw, ctime, wtime):
        ref, long = self._store_blocks(raw)
        ino = self.alloc_inode()
        self._write_inode(ino, ref, long, 1, len(raw), ctime, wtime)
        return ino

    def _pack_dnode(self, name, inode, kind):
        nb = name.encode("koi8_r", "replace")[:32]
        nb = nb + b"\0" * (32 - len(nb))
        return nb + b"\0" * 16 + struct.pack("<ii", inode, kind) + b"\0" * 8

    def write_dir(self, ino, entries, ctime, wtime):
        raw = b"".join(self._pack_dnode(n, i, k) for (n, i, k) in entries)
        ref, long = self._store_blocks(raw)
        self._write_inode(ino, ref, I_DIR | long, 1, len(raw), ctime, wtime)

    # -------------------------------------------------------------- format ----
    @classmethod
    def blank(cls, size_mb, label, ktime):
        b_no = size_mb * (1024 * 1024 // BLOCK)
        i_no = b_no - b_no // INODES_PER_BLOCK - 2                 # OS formula
        i_no = ceil_div(i_no, INODES_PER_BLOCK) * INODES_PER_BLOCK
        if i_no <= 0:
            raise ValueError("volume too small")
        # OS too_large guard (mount_super/new_super): super must stay well within
        # the dirty-map-tracked region; 4 dirty words * 32 * 8 gives the ceiling.
        if (i_no + 31) + (b_no + 31) >= (4 * 32 * 8) * 512 - LABEL * 32:
            raise ValueError("volume too large for the Excelsior superblock format")

        v = cls.__new__(cls)
        v.data = bytearray(b_no * BLOCK)
        v.total_blocks = b_no
        sb = BLOCK
        lab = label.encode("koi8_r", "replace")[:7]
        v.data[sb:sb + len(lab)] = lab
        v._wr(sb + 4 * 4, i_no)
        v._wr(sb + 5 * 4, b_no)
        v._wr(sb + 6 * 4, CXE)
        v._wr(sb + 7 * 4, ktime)
        v._parse_super()

        # bitmaps: everything free, then punch out the metadata.
        v._fill_free(v.bset_off, b_no)
        v._fill_free(v.iset_off, i_no)
        for blk in range(0, v.inoH + 2):        # booter, super, inode table, root data
            v._clear_bit(v.bset_off, blk)
        for n in range(3):                      # root, SYSTEM.BOOT, BAD.BLOCKS
            v._clear_bit(v.iset_off, n)

        root_data = v.inoH + 1
        v._write_inode(0, [root_data], I_DIR, 1, DNODE_SZ * 3, ktime, ktime)
        v._write_inode(1, [0], 0, 1, 0, ktime, ktime)   # SYSTEM.BOOT (empty placeholder)
        v._write_inode(2, [0], 0, 1, 0, ktime, ktime)   # BAD.BLOCKS  (empty placeholder)
        root_entries = [("..", 0, D_DIR | D_HIDDEN),
                        ("SYSTEM.BOOT", 1, D_FILE | D_HIDDEN | D_SYS),
                        ("BAD.BLOCKS", 2, D_FILE | D_HIDDEN | D_SYS)]
        raw = b"".join(v._pack_dnode(n, i, k) for n, i, k in root_entries)
        v.data[root_data * BLOCK:root_data * BLOCK + len(raw)] = raw

        v._blk_cursor = v.inoH + 2
        v._ino_cursor = 3
        return v

    # -------------------------------------------------------------- import ----
    def _read_host_file(self, path):
        with open(path, "rb") as f:
            raw = f.read()
        ext = os.path.splitext(path)[1].lower()
        if ext in TEXT_EXTS:
            try:
                text = raw.decode("utf-8")
            except UnicodeDecodeError:
                text = raw.decode("koi8_r")
            text = text.replace("\r\n", "\n").replace("\r", "\n").replace("\n", chr(RS))
            return text.encode("koi8_r", "replace")
        return raw

    def import_dir(self, host_dir, parent_ino, stats):
        """Recursively import host_dir; return the new directory's inode."""
        my_ino = self.alloc_inode()
        entries = [("..", parent_ino, D_DIR | D_HIDDEN)]
        for name in sorted(os.listdir(host_dir)):
            path = os.path.join(host_dir, name)
            if os.path.islink(path):
                stats["skipped"].append(path + " (symlink)")
                continue
            xd_name = name
            if len(xd_name.encode("koi8_r", "replace")) > 32:
                xd_name = xd_name[:32]
                stats["truncated"].append(path)
            if os.path.isdir(path):
                sub = self.import_dir(path, my_ino, stats)
                entries.append((xd_name, sub, D_DIR))
                stats["dirs"] += 1
            elif os.path.isfile(path):
                try:
                    raw = self._read_host_file(path)
                    st = os.stat(path)
                    t = ktime_of(st.st_mtime)
                    fino = self.store_file(raw, t, t)
                    entries.append((xd_name, fino, D_FILE))
                    stats["files"] += 1
                    stats["bytes"] += len(raw)
                except ValueError as e:
                    stats["skipped"].append("%s (%s)" % (path, e))
        st = os.stat(host_dir)
        t = ktime_of(st.st_mtime)
        self.write_dir(my_ino, entries, t, t)
        return my_ino

    def import_at_root(self, host_dir, as_name, stats):
        if as_name:
            top = self.import_dir(host_dir, 0, stats)
            self._extend_root([(as_name, top, D_DIR)])
            return
        new = []
        for name in sorted(os.listdir(host_dir)):
            path = os.path.join(host_dir, name)
            if os.path.islink(path):
                stats["skipped"].append(path + " (symlink)")
                continue
            if os.path.isdir(path):
                sub = self.import_dir(path, 0, stats)
                new.append((name, sub, D_DIR))
                stats["dirs"] += 1
            elif os.path.isfile(path):
                raw = self._read_host_file(path)
                t = ktime_of(os.stat(path).st_mtime)
                new.append((name, self.store_file(raw, t, t), D_FILE))
                stats["files"] += 1
                stats["bytes"] += len(raw)
        self._extend_root(new)

    def _extend_root(self, new_entries):
        base = [("..", 0, D_DIR | D_HIDDEN),
                ("SYSTEM.BOOT", 1, D_FILE | D_HIDDEN | D_SYS),
                ("BAD.BLOCKS", 2, D_FILE | D_HIDDEN | D_SYS)]
        # root's format-time single block leaks (1 block); rewrite root fresh.
        self.write_dir(0, base + new_entries, self.ctime, self.ctime)


# ------------------------------------------------------------------ commands --
def cmd_info(args):
    v = Volume.load(args.vol)
    print("volume       : %s" % args.vol)
    print("  label      : %r" % v.label)
    print("  magic      : 0x%08x %s" % (v.magic, "OK (CXE)" if v.magic == CXE else "!! not CXE"))
    print("  blocks     : %d total (%d KB), %d free" % (v.b_no, v.b_no * 4, v.blocks_free()))
    print("  inodes     : %d total, %d free" % (v.i_no, v.inodes_free()))
    print("  super span : blocks 1..%d  (%d words)" % (v.inoL - 1, v.supsz_words))
    print("  inode tbl  : blocks %d..%d" % (v.inoL, v.inoH))
    print("  data from  : block %d" % (v.inoH + 1))
    print("  image size : %d blocks (header says %d: %s)" %
          (v.total_blocks, v.b_no, v.total_blocks == v.b_no))


def cmd_ls(args):
    v = Volume.load(args.vol)

    def walk(ino_no, prefix):
        for name, sub, kind in v.read_dir(v.inode(ino_no)):
            if name == "..":
                continue
            if kind & D_DIR:
                print("%s%s/" % (prefix, name))
                walk(sub, prefix + "  ")
            else:
                print("%s%-24s %8d" % (prefix, name, v.inode(sub).eof))

    print("/")
    walk(0, "  ")


def cmd_create(args):
    v = Volume.blank(args.size_mb, args.label, now_ktime())
    v.save(args.vol)
    print("created %s: %d MB, %d blocks, %d inodes, inode table blocks %d..%d" %
          (args.vol, args.size_mb, v.b_no, v.i_no, v.inoL, v.inoH))


def cmd_import(args):
    v = Volume.load(args.vol)
    stats = {"files": 0, "dirs": 0, "bytes": 0, "skipped": [], "truncated": []}
    v.import_at_root(args.host_dir.rstrip("/"), getattr(args, "as_name", None), stats)
    v.save(args.vol)
    print("imported %d files, %d dirs, %.1f MB into %s" %
          (stats["files"], stats["dirs"], stats["bytes"] / 1e6, args.vol))
    print("blocks free after: %d / %d ; inodes free: %d / %d" %
          (v.blocks_free(), v.b_no, v.inodes_free(), v.i_no))
    for s in stats["truncated"]:
        print("  TRUNCATED name >32: %s" % s)
    for s in stats["skipped"]:
        print("  SKIPPED: %s" % s)


def main(argv=None):
    ap = argparse.ArgumentParser(description="Excelsior XD volume tool")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("info"); p.add_argument("vol"); p.set_defaults(func=cmd_info)
    p = sub.add_parser("ls"); p.add_argument("vol"); p.set_defaults(func=cmd_ls)

    p = sub.add_parser("create")
    p.add_argument("vol")
    p.add_argument("--size-mb", type=int, required=True)
    p.add_argument("--label", default="src")
    p.set_defaults(func=cmd_create)

    p = sub.add_parser("import")
    p.add_argument("vol")
    p.add_argument("host_dir")
    p.add_argument("--as", dest="as_name", default=None,
                   help="nest the tree under /NAME instead of importing its contents at root")
    p.set_defaults(func=cmd_import)

    args = ap.parse_args(argv)
    args.func(args)


if __name__ == "__main__":
    main()

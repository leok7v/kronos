#!/usr/bin/env python3
"""Build a partitioned SD image for the OneChipBook Kronos.

The sd_disk_controller reads a real MBR partition table from
sector 0 of the card and maps io2 disk unit N to the Nth primary partition --
base sector and length come from the table, so the layout lives on the card,
not in the FPGA bitstream. This tool writes that layout:

    partition 1 (unit 0 = /dev/xd0) : Excelsior boot volume, type 0xDA
    partition 2 (unit 1 = /dev/xd1) : Excelsior system volume, type 0xDA
    partition 3 (unit 2 = /dev/xd2) : Excelsior volume (e.g. the sources), 0xDA
    partition 4 (unit 3 = /dev/xd3) : FAT16 exchange volume, type 0x0E

The controller reads all four MBR entries, so xd0..xd3 are genuinely distinct
media. Type 0xDA ("non-FS data") keeps Linux from touching the Excelsior
partitions; type 0x0E is FAT16-LBA, which both Linux and Excelsior's MSDOS
driver understand -- the shared partition.

Build the xd2 sources volume first with the companion tool:
    ./mkxd.py create ../../../xd2.dsk --size-mb 128 --label src
    ./mkxd.py import ../../../xd2.dsk ../../../excelsior/src

Output is an image FILE (default). Writing it to a real card is a separate,
deliberate `dd` step that this script only PRINTS for you to run -- it never
touches a block device itself.

Example (fit the card exactly -- a nominal "1 GB" card is often ~958 MiB, so
size the image to the kernel-reported sector count and let the FAT fill the rest):
    ./mkcard.py --xd0 ../../../xd0.dsk --xd1 ../../../xd1.dsk \\
                --xd2 ../../../xd2.dsk \\
                --card-secs $(cat /sys/block/sda/size) --out card.img
    # then, after double-checking the device name:
    sudo dd if=card.img of=/dev/sdX bs=4M conv=fsync status=progress
"""
import argparse
import os
import struct
import subprocess
import sys

SECTOR = 512


def align_up(lba, align):
    return ((lba + align - 1) // align) * align


def lba_to_chs(lba, hpc=255, spt=63):
    """Classic CHS for the MBR entry; caps at the max sentinel when the LBA is
    beyond CHS range (which is what fdisk writes for LBA partitions)."""
    cyl = lba // (hpc * spt)
    if cyl > 1023:
        cyl, head, sector = 1023, 254, 63
    else:
        head = (lba // spt) % hpc
        sector = (lba % spt) + 1
    b1 = head & 0xFF
    b2 = (sector & 0x3F) | ((cyl >> 2) & 0xC0)
    b3 = cyl & 0xFF
    return bytes((b1, b2, b3))


def part_entry(bootable, ptype, start_lba, num_sectors):
    if num_sectors == 0:
        return bytes(16)
    return (bytes((0x80 if bootable else 0x00,))
            + lba_to_chs(start_lba)
            + bytes((ptype,))
            + lba_to_chs(start_lba + num_sectors - 1)
            + struct.pack("<I", start_lba)
            + struct.pack("<I", num_sectors))


def build_mbr(parts):
    """parts: list of (ptype, start_lba, num_sectors), up to 4."""
    mbr = bytearray(SECTOR)
    off = 446
    for i, (ptype, start, count) in enumerate(parts[:4]):
        mbr[off:off+16] = part_entry(i == 0, ptype, start, count)
        off += 16
    mbr[510] = 0x55
    mbr[511] = 0xAA
    return bytes(mbr)


def secs(path):
    return (os.path.getsize(path) + SECTOR - 1) // SECTOR


def choose_spc(num_sectors, fat_type):
    """Sectors-per-cluster keeping the cluster count under the FAT limit
    (4084 for FAT12, 65524 for FAT16); mkfs.fat refuses volumes outside it."""
    limit = 4084 if fat_type == 12 else 65524
    for spc in (1, 2, 4, 8, 16, 32, 64):
        if num_sectors // spc <= limit:
            return spc
    return 64


def make_fat(path, num_sectors, label, fat_type):
    """Create a FAT12/FAT16 filesystem of num_sectors*512 bytes at `path`."""
    with open(path, "wb") as f:
        f.truncate(num_sectors * SECTOR)
    tool = None
    for cand in ("mkfs.fat", "mkfs.vfat"):
        if subprocess.run(["which", cand], capture_output=True).returncode == 0:
            tool = cand
            break
    if tool is None:
        sys.exit("error: mkfs.fat not found (install dosfstools) -- needed for "
                 "the FAT partition; or pass --fat-mb 0 to skip it")
    spc = choose_spc(num_sectors, fat_type)
    if fat_type == 16 and num_sectors // spc < 4085:
        sys.exit(f"error: FAT16 partition too small ({num_sectors} sectors); "
                 "use --fat-mb of at least ~2, or --fat-type 12")
    # -F selects FAT12/16 (Excelsior's MSDOS.m does FAT12/16, NOT FAT32);
    # -s sets sectors/cluster explicitly so small volumes are accepted.
    subprocess.run([tool, "-F", str(fat_type), "-s", str(spc), "-n", label, path],
                   check=True)


def main():
    ap = argparse.ArgumentParser(description="Build a partitioned Kronos SD image")
    ap.add_argument("--xd0", required=True, help="Excelsior boot/root volume (unit 0)")
    ap.add_argument("--xd1", help="second Excelsior volume (unit 1); optional")
    ap.add_argument("--xd2", help="third Excelsior volume (unit 2), e.g. the "
                    "excelsior/src sources built with mkxd.py; optional")
    ap.add_argument("--card-mb", type=int, default=0,
                    help="total card size in MiB (e.g. 1024 for a 1 GB card). When "
                         "set, the FAT partition is sized to fill the remaining space "
                         "to the card's end, and --fat-mb is ignored.")
    ap.add_argument("--card-secs", type=int, default=0,
                    help="exact total card size in 512-byte sectors, as reported by "
                         "`cat /sys/block/sdX/size`. Preferred over --card-mb: it makes "
                         "the image fit the card exactly (a nominal '1 GB' card is often "
                         "only ~958 MiB). The FAT fills the remainder; --fat-mb ignored.")
    ap.add_argument("--fat-mb", type=int, default=64,
                    help="size of the shared FAT partition in MiB (0 = none). "
                         "FAT12 is capped at 32. Ignored when --card-mb is given.")
    ap.add_argument("--fat-type", type=int, default=16, choices=(12, 16),
                    help="16 (default): larger; mounts in Excelsior via the MSDOS.cod "
                         "server (mou /mnt /dev/xd2 fs=MSDOS). 12: <=32 MB but readable "
                         "directly by the on-board 'msex' tool with no server.")
    ap.add_argument("--fat-label", default="KRONOS", help="FAT volume label")
    ap.add_argument("--out", default="card.img", help="output image file")
    ap.add_argument("--align", type=int, default=2048,
                    help="partition start alignment in sectors (default 2048 = 1 MiB)")
    args = ap.parse_args()

    # ---- lay out the partitions ----
    layout = []          # (name, ptype, start_lba, num_sectors, src_path)
    lba = args.align     # leave sector 0 (MBR) + alignment gap before partition 1

    xd0_secs = secs(args.xd0)
    layout.append(("xd0 (Excelsior)", 0xDA, lba, xd0_secs, args.xd0))
    lba = align_up(lba + xd0_secs, args.align)

    if args.xd1:
        xd1_secs = secs(args.xd1)
        layout.append(("xd1 (Excelsior)", 0xDA, lba, xd1_secs, args.xd1))
        lba = align_up(lba + xd1_secs, args.align)

    if args.xd2:
        xd2_secs = secs(args.xd2)
        layout.append(("xd2 (Excelsior)", 0xDA, lba, xd2_secs, args.xd2))
        lba = align_up(lba + xd2_secs, args.align)

    # ---- size the FAT partition: fill the card if a card size was given ----
    #  --card-secs (exact kernel size) wins over --card-mb (nominal MiB).
    card_secs = args.card_secs if args.card_secs > 0 else \
        (args.card_mb * (1024 * 1024 // SECTOR) if args.card_mb > 0 else 0)
    if card_secs > 0:
        if lba >= card_secs:
            ap.error("card size %d sectors (%.1f MiB) is too small for xd0..xd2 "
                     "(they already need %d sectors / %.1f MiB)"
                     % (card_secs, card_secs * SECTOR / (1024 * 1024),
                        lba, lba * SECTOR / (1024 * 1024)))
        # floor the FAT to whole MiB so the total image is guaranteed <= card size
        fat_mb = (card_secs - lba) * SECTOR // (1024 * 1024)
    else:
        fat_mb = args.fat_mb

    fat_path = None
    if fat_mb > 0:
        if args.fat_type == 12 and fat_mb > 32:
            ap.error("--fat-type 12 (msex-readable) is limited to 32 MB; use "
                     "--fat-mb <=32, or --fat-type 16 for a larger volume")
        fat_secs = fat_mb * (1024 * 1024 // SECTOR)
        fat_path = args.out + ".fat.tmp"
        ptype = 0x01 if args.fat_type == 12 else 0x0E   # 01=FAT12, 0E=FAT16-LBA
        layout.append((f"FAT{args.fat_type} (shared)", ptype, lba, fat_secs, fat_path))
        lba = align_up(lba + fat_secs, args.align)

    total_secs = lba

    # ---- report ----
    print(f"{'partition':<18} {'unit':<5} {'type':<6} {'start_lba':>10} "
          f"{'sectors':>10} {'size':>10}   {'4KB blocks (getsize)':>20}")
    for i, (name, ptype, start, count, _src) in enumerate(layout):
        mb = count * SECTOR / (1024 * 1024)
        print(f"{name:<18} xd{i:<4} 0x{ptype:02X}   {start:>10} {count:>10} "
              f"{mb:>8.1f}MB   {count // 8:>20}")
    print(f"total image size: {total_secs * SECTOR / (1024*1024):.1f} MB "
          f"({total_secs} sectors)")

    # ---- build the FAT filesystem, if any ----
    if fat_path:
        fat_secs = layout[-1][3]
        print(f"\ncreating FAT{args.fat_type} filesystem ({fat_secs} sectors)...")
        make_fat(fat_path, fat_secs, args.fat_label, args.fat_type)

    # ---- assemble the image (sparse) ----
    mbr = build_mbr([(p[1], p[2], p[3]) for p in layout])
    print(f"\nwriting {args.out} ...")
    with open(args.out, "wb") as out:
        out.truncate(total_secs * SECTOR)     # sparse: gaps stay holes
        out.seek(0)
        out.write(mbr)
        for name, ptype, start, count, src in layout:
            with open(src, "rb") as f:
                out.seek(start * SECTOR)
                # copy in chunks to keep memory flat
                while True:
                    buf = f.read(1 << 20)
                    if not buf:
                        break
                    out.write(buf)

    if fat_path and os.path.exists(fat_path):
        os.remove(fat_path)

    print("done.\n")
    print("Flash it (DESTROYS the card -- check the device name!):")
    print(f"    sudo dd if={args.out} of=/dev/sdX bs=4M conv=fsync status=progress")
    print("\nAfter flashing, on the host you can mount the shared partition:")
    print("    sudo mount -o loop,offset=$((%d*512)) %s /mnt   # or /dev/sdX3"
          % (layout[-1][2] if fat_path else 0, args.out))


if __name__ == "__main__":
    main()

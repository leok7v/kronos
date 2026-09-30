# `rx` — push files to the Kronos board over UART (no SD-card swap)

`rx` is a tiny Excelsior (Modula-2) receiver that lets you send a file from the
host to the board over the serial console, so you can edit a source file on the
laptop, stream it to the board, and rebuild — without pulling the SD card.

Proven 2026-07-23: streamed a full 28 KB `kmm.c` byte-perfect (checksum matched),
then `cc kmm` rebuilt it, entirely over UART.

```
host file  ──rxstream.py──▶  UART  ──▶  rx.cod on board  ──▶  file on disk
```

## Contents of this directory

| File | Runs on | What it is |
|------|---------|-----------|
| `rx.m`        | board | the receiver (Modula-2 source; build to `rx.cod`) |
| `exdrive.py`  | host  | one-time installer: drives the `ex` editor to type `rx.m` onto the board |
| `rxstream.py` | host  | streams a host file to a running `rx` and verifies the checksum |
| `sercmd.py`   | host  | minimal console helper — send a command, capture the reply (used to run `mx`, `cc`, etc.) |

All host scripts are pure Python 3 stdlib (termios), no pyserial needed.

## Physical setup (the serial console)

- 3.3 V TTL, **57600 8N1**. GND = DB9 pin 9, board TX = DB9 pin 2.
  (Full pinout: `../../[memory] onechipbook-uart-pinout`.)
- The USB-serial adapter (CP2102) **re-enumerates** — `/dev/ttyUSB0` becomes
  `/dev/ttyUSB1` and back, device number climbing. **Always auto-detect the port**;
  every script honors a `PORT=` env var. Detect with:
  ```sh
  export PORT=$(ls /dev/ttyUSB* | head -1)
  ```
  If a long transfer/measure dies with the fd going silent, the port re-enumerated
  mid-run — re-detect `PORT` and retry.
- The board must be **logged in and at a shell prompt** (e.g. `HH:MM dir ! `).
  Console text is KOI8-R.

## One-time install of `rx` on the board

`rx` is installed once and lives on the board's disk. Two steps: type the source
in with the `ex` editor, then compile it with `mx`.

```sh
export PORT=$(ls /dev/ttyUSB* | head -1)

# 1. Type rx.m onto the board (creates /usr1/c/rx.m).
EXFILE=/usr1/c/rx.m SRC=vm/uart/rx.m python3 vm/uart/exdrive.py

# 2. Compile it (Modula compiler). Produces rx.cod next to the source.
python3 vm/uart/sercmd.py '{ SYM=/sym mxOUT=*=/usr1/c CD=/usr1/c } mx rx\r' 20
```

Notes:
- Pick a directory that's on the shell's command search path so `rx` resolves as a
  command later; `/usr1/c` is where the C toolchain lives, so it's convenient.
- `exdrive.py` needs the ~4 s editor settle and a throwaway leading Enter (both
  built in) to absorb the first keystroke `ex` eats. `ex` auto-indents, but
  Modula is free-form so it's harmless. Ctrl-E (0x05) saves+exits.

## Sending a file

`rx` takes the **destination path** as its argument, prints `rx: ready`, then
reads a decimal byte-count followed by exactly that many raw bytes, writes them in
one block, and prints `rx: <N> bytes sum <S>`.

`rxstream.py` in `SIMPLE` mode drives that handshake and checks the checksum:

```sh
export PORT=$(ls /dev/ttyUSB* | head -1)
SIMPLE=1 CONTENT_FILE=kmm.c DEST=/usr1/c/kmm.c GAP=0.005 \
    python3 vm/uart/rxstream.py
```

Expected tail:
```
RESULT: rx: 28680 bytes sum 17260
host:   28680 bytes sum 17260
MATCH
```

`GAP` is the per-byte pacing in seconds (0.005 works; 28 KB ≈ 2.4 min). On
`MISMATCH`, just resend — it's idempotent (creates the file fresh each time).

After it lands, rebuild on the board as usual, e.g. `cc kmm` for the C toolchain
or `mx foo` for Modula, driven with `sercmd.py`.

## Why it's built this way (hard-won gotchas)

These are the constraints that shaped `rx`; ignore them and transfers silently
corrupt or hang.

- **No shell redirection.** `echo x > f` writes the `>` literally. The only
  built-in file *creator* is the `ex` editor — hence `exdrive.py` for the install.
- **A launched program reads the console via `Keyboard.read`, not `StdIO`.**
  The shell does `read := Keyboard.read`. `StdIO.read` gets nothing. `rx` uses
  `Keyboard.read` (aliased `key.read`).
- **`0x1C` (FS / ASCII.EOF) is swallowed** by the keyboard layer, so it can't be a
  terminator. `rx` uses a **decimal length prefix** instead.
- **The board drops incoming bytes whenever `rx` does its own I/O** — console
  output *or* a disk write. Ack-based flow control fails (the ack echo drops the
  next byte); even a silent per-byte-to-disk receive drops a byte and hangs.
  **The fix is the core design: read the whole file into a RAM array using ONLY
  `key.read`, then do one bulk `bio.write` at the very end.** `mem` is
  `ARRAY[0..40000]` — bump it for larger files.
- Modula quirks in `rx.m`: octal char constants (`034c`, not `1Cx`); no `RETURN`
  in a module body (use nested IF/ELSE).
- The board's serial channel itself is clean; a 200-char echo test was verbatim.
  (Earlier "corruption" was a `vm/int` emulation artifact, not the board.)

## Related gotcha — reading `.mm`/data files from FAT

Unrelated to `rx` but discovered alongside it: the C runtime's low-level
`open`/`read` (block reads via `BIO.read`) **works on the native FS but HANGS on
FAT (`/mnt`, the MSDOS server)** — returns garbage, then the reader hangs. Use
`fopen`/`fread` (byte/stream I/O), which works on both. See the
`metamath-verifier` and `uart-file-transfer` memory notes.

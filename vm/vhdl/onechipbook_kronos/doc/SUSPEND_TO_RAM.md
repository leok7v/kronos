# Suspend-to-RAM (sleep mode) — OneChipBook Kronos OneChipBook

Puts the interactive Excelsior workstation into a low-power **sleep** state: the
SDRAM goes into self-refresh (retaining every byte at microamp-class current),
the CPU is frozen, and the monitor powers down — while the whole machine state
survives, so a keypress resumes exactly where you left off.

Validated end-to-end on real Cyclone EP1C12 silicon (2026-07-23): auto-sleep on
a timeout, on-demand `sleep`, runtime-configurable delay, DPMS screen-off, and
the LED sleep indicator all confirmed working.

---

## 1. What it does

- **Auto-sleep.** After a configurable idle period (default **20 minutes**) with
  no keyboard input *and* no console output, the machine sleeps.
- **On-demand sleep.** The `sleep` command sleeps immediately.
- **Wake.** Any **PS/2 keypress** wakes it; the screen returns a second or so
  later once the monitor re-locks sync. *(UART-RX does not wake — see §8.)*
- **State preserved.** RAM is held in SDRAM self-refresh; the VGA console keeps
  its text; the CPU resumes mid-instruction. Nothing is lost.
- **Visible status.** In the normal (quiet) LED mode, **LED1 gives a brief blink
  roughly every 3 s while asleep**, and is off while awake.

While asleep: the CPU is halted, the SDRAM is in self-refresh, the 50 Hz timer
interrupt is masked, and the VGA outputs are blanked with hsync/vsync dropped
(DPMS off, so the monitor enters power-save).

---

## 2. Using it

### From the OS

| Command | Effect |
|---|---|
| `sleeptime 1200` | auto-sleep after 1200 s (20 min) of inactivity |
| `sleeptime 60` | ... after 1 minute |
| `sleeptime 0` | disable auto-sleep entirely |
| `sleep` | sleep **now**; press any PS/2 key to wake |

`sleeptime` and `sleep` are Modula-2 utilities (source in `os_util/`, see §6).
The power-on default is 20 min (baked into the bitstream); to make a chosen
delay persist across reboots, run `sleeptime <n>` from your startup script.

### LED / DIP behaviour (normal build, `SUSPEND_DIAG=false`)

| DIP | Position | LEDs |
|---|---|---|
| **SW4** | OFF (rest) → **normal mode** | LED1 = rare blink while asleep; LED9 = panic; LED2–8 dark |
| **SW4** | ON → **debug mode** | the historical boot-status LEDs (LED2 sdc_ready … LED8 "Loaded OK", LED9 panic) |

> **Not SW3.** SW1/SW2/SW3 disturb the board's VGA line-scan generator; only
> SW4–SW7 are safe live inputs. The mode select is SW4 = `sw(3)`.

---

## 3. Enabling & building

The feature is gated behind generics so a normal build is bit-for-bit the
validated non-suspend bitstream. The synthesis top `OneChipBookTop` sets:

```vhdl
SUSPEND_EN          => true,      -- master enable (false = feature absent)
SUSPEND_DEFAULT_SEC => 1200,      -- power-on timeout in SECONDS (20 min); 0 = off
SUSPEND_DIAG        => false       -- true: LED2..8 show the suspend internals
```

Build (Quartus II 11.0):

```sh
export QUARTUS_ROOTDIR=/home/dmitry/altera/11.0/quartus
export PATH="$QUARTUS_ROOTDIR/bin:$PATH"
cd vm/vhdl/onechipbook_kronos
quartus_sh --flow compile kronos_onechip
quartus_cpf -c -d EPCS4 output_files/kronos_onechip.sof \
                        output_files/kronos_onechip_epcs4.pof   # the -d is REQUIRED
```

Flash `kronos_onechip_epcs4.pof` to the EPCS4, unplug the Blaster,
power-cycle.

**Current image:** md5 `09b331631bffebb22a0425af8894c86a` —
10,329 / 12,060 LE (86 %), setup slack **+1.610 ns** at 25.77 MHz, 0 errors.
The suspend logic (FSM + counters + registers) is a small fraction of the fabric
and does not disturb timing.

**Rollback** if it ever misbehaves: `halt => '0'` in `OneChipBook` (sleep
still works via self-refresh), then `SUSPEND_EN => false` for the exact prior
bitstream.

---

## 4. Architecture

Three pieces, plus the top-level integration.

### `src/sdram_controller.vhdl` — SDRAM self-refresh (the "RAM" half)

New `suspend` input / `suspended` output, a registered `sr_cke` (CKE was
hardwired high), a `T_XSR` generic, and three states
`S_SELF_ENTRY → S_SELF_HOLD → S_SELF_EXIT`.

- Enters self-refresh **only from `S_IDLE`**, so any in-flight access finishes
  first and every bank is already auto-precharged — the legal precondition.
- Entry issues `AUTO_REFRESH` with **CKE dropping low on the same edge** (the
  JEDEC self-refresh entry); then holds CKE low, issuing nothing.
- Exit raises CKE, waits `T_XSR`, issues one refresh, resumes.
- While parked it **acks nothing**, so a master that touches RAM simply stalls
  until resume.

### `src/suspend_ctrl.vhdl` — the sequencer

```
generic ( IDLE_GATED : boolean := false )
port ( clk, reset,
       cpu_idle, activity, sleep_now, sec_tick : in;
       timeout_sec   : in std_logic_vector(15 downto 0);   -- seconds; 0 = off
       sdc_suspended : in;
       cpu_halt, sdc_suspend, sleeping : out;
       dbg_ramp      : out std_logic_vector(2 downto 0) )   -- diagnostic
```

- Inactivity is counted in **seconds** (`sec_tick` 1 Hz pulse), reset by
  `activity`. Seconds, not clocks: 20 min ≈ 31 billion clocks overflows a 32-bit
  counter, and seconds are the natural unit for the OS to set. `timeout_sec = 0`
  disables auto-sleep.
- `sleep_now` (the `sleep` command) forces an immediate sleep, bypassing the
  timer.
- FSM: `S_RUN → S_ENTER → S_SLEEP → S_EXIT → S_WAKE`. `ENTER` asserts
  suspend+halt and waits for `sdc_suspended`; on wake it deasserts suspend but
  **keeps halt through `EXIT`** until `sdc_suspended` clears (CKE high, tXSR
  elapsed, refreshed) — only then is it safe to run again.
- **`IDLE_GATED = false`** here: see §7.

### `src/OneChipBook.vhdl` — integration

- Generics `SUSPEND_EN`, `SUSPEND_DEFAULT_SEC`, `SUSPEND_DIAG`. The whole thing
  sits in a `gen_suspend`/`gen_no_suspend` generate; when `SUSPEND_EN=false` the
  sequencer is absent and `cpu_halt`/`sdc_suspend`/`sleeping` tie to `'0'`.
- `sec_tick`: a 1 Hz pulse = 50 of the existing 50 Hz OS ticks.
- **Activity** = `kbd_frame OR con_strobe` (a PS/2 frame *or* an OS console
  output byte) — the compute-safety guard (§8).
- Held in **reset until `lok_seen`** ("Loaded OK"), so the boot's long keyless
  disk load can't be mistaken for inactivity and sleep mid-boot.
- Timer masked while asleep: `cpu_ireq(1) <= tick and not sleeping`.
- VGA outputs routed through internal nets, then gated on `sleeping`: pixels
  blanked and hsync/vsync dropped (DPMS off).
- LED mode mux on SW4; `SUSPEND_DIAG` adds an internals view.
- `halt => cpu_halt` wired into the Kronos core (was `'0'`).

---

## 5. I/O register interface

Two registers in the CPU's I/O page (top 4 KB of the 23-bit word space), written
with the standard Kronos `out` op (`io1`, opcode `91h`,
`mem[iopage + reg] = val`) and read with `inp` (`io0`, `90h`).

| Word addr | `reg` | Access | Meaning |
|---|---|---|---|
| `0x7FF020` | `0x20` | read/write | **sleep timeout, in seconds.** `0` disables auto-sleep. Power-on default = `SUSPEND_DEFAULT_SEC`. |
| `0x7FF021` | `0x21` | write | **sleep now** — any write parks the machine immediately. |

Decoded in `OneChipBook.vhdl` as `sel_timeout` (`cpu_adr(11:0)=0x020`) and
`sel_sleepnow` (`0x021`), clear of the disk (`0x010–0x017`), DL11
(`0xFB8–0xFBB`) and legacy (`0x000`) registers.

---

## 6. OS utilities (`os_util/`)

Small Modula-2 commands that write those registers. Sources:
`os_util/sleeptime.m`, `os_util/sleep.m` (see `os_util/README.md`). Both use:

```modula
PROCEDURE out(reg,val: INTEGER); CODE 91h END out;   -- the io1 write op
```

`sleeptime` parses its seconds argument with `str.iscan` and does
`out(0x20, n)`; `sleep` does `out(0x21, 1)`.

**Install over UART** (no SD swap) with the `rx` tooling in `vm/uart/` — see
`../uart/README.md` and the `uart-file-transfer` memory:

```sh
export PORT=$(ls /dev/ttyUSB* | head -1)
SIMPLE=1 CONTENT_FILE=vm/vhdl/onechipbook_kronos/os_util/sleeptime.m \
    DEST=/usr1/c/sleeptime.m GAP=0.005 python3 vm/uart/rxstream.py
python3 vm/uart/sercmd.py '{ SYM=/sym mxOUT=*=/usr1/c CD=/usr1/c } mx sleeptime\r' 25
# (repeat for sleep.m)
```

They were deployed to `/usr1/c` this way (alongside the C toolchain). To make
them resolve from any directory, drop the `.cod` into the system command dir
(the shell's `BIN` = `env.bin` paths + cwd; `/bin` and `/usr/bin` are *not* on
this disk's path). Copy with `cp -omq foo.cod <dir>` (the plain interactive `cp`
prompts and desyncs a scripted console).

---

## 7. Why the trigger is inactivity, not `cpu_idle`

The clean design would sleep when the CPU parks on the `IDLE` instruction
(opcode `0x87`), reported by the core's `idle` output. **On this OS that never
happens:** the shell **busy-polls** the console and never executes `IDLE`, so
`cpu_idle` never asserts (confirmed on hardware with an on-LED diagnostic —
`idle_seen` stayed dark through boot, login and the shell, while the inactivity
ramp climbed fine). The synthesis Decode's idle detect is correct
(`u_adr = "11110000111"` = dispatch of `0x87`, routed out through Kronos); the OS
simply doesn't use it.

So `suspend_ctrl` has `IDLE_GATED = false` and sleeps on **console inactivity**
alone. The `IDLE_GATED` generic is kept (default aside) for a CPU/OS that *does*
idle.

---

## 8. Known limitations

- **Wake is PS/2 only.** `activity = kbd_frame OR con_strobe`; `kbd_frame` is a
  PS/2 frame and `con_strobe` is output (can't fire while halted). A **UART-RX**
  key does *not* wake it — so `sleep` invoked from the serial console cannot be
  woken over serial (only from the board's own PS/2 keyboard). Adding UART-RX as
  a wake term is a straightforward future change.
- **A fully silent long compute could sleep.** Console output resets the timer,
  so tasks that print (compiles, `kmm`, `ls`) stay awake; a task that produces
  **no** output for the whole timeout would sleep — press a key to resume. For
  such a task, `sleeptime 0` first, or add an SDRAM-miss-activity term to
  `activity`.
- **`halt` is belt-and-braces.** Correctness rests on the SDRAM parking from idle
  + the timer mask + the bus stall, not on `halt` freezing the core (its effect
  on this core build is unverified). Full CPU clock-gating for the extra
  dynamic-power win is a follow-on.
- **Monitor re-sync delay.** Dropping sync (DPMS) means the monitor takes ~1 s to
  re-lock on wake; the frame is intact underneath.

---

## 9. Verification

Simulation (GHDL):

| Testbench | Checks |
|---|---|
| `sim/tb_sdram_suspend.vhdl` | data survives self-refresh; CKE held low across a window ≫ the refresh interval; a request issued while parked stalls then serves on resume; repeatable; no protocol-assertion failures against the checking `sdram_model` |
| `sim/tb_suspend_ctrl.vhdl` | sleeps after the inactivity timeout; stays asleep; a key wakes it and halt is held until SDRAM resumes; activity keeps it awake; `timeout=0` disables; `sleep_now` sleeps immediately |
| `sim/run_sdram_checks.sh` | negative-control regression — baseline PASS, mutations FAIL (the entity change didn't regress it) |

Hardware: booted with the feature, auto-slept and woke on a key; `sleeptime`
set/disabled the delay; `sleep` slept + woke on demand; VGA blanked; LED1
blinked.

---

## 10. File map

| File | Role |
|---|---|
| `src/sdram_controller.vhdl` | SDRAM self-refresh (`suspend`/`suspended`, `sr_cke`, self-refresh states) |
| `src/suspend_ctrl.vhdl` | the sleep sequencer FSM + seconds timer |
| `src/OneChipBook.vhdl` | integration: generics, I/O registers, `sec_tick`, activity, LED mode, VGA gating, boot gate |
| `src/OneChipBookTop.vhdl` | synthesis top; sets the suspend generics |
| `os_util/sleeptime.m`, `os_util/sleep.m` | OS commands (write the I/O registers) |
| `os_util/README.md` | utility build/install notes |
| `sim/tb_sdram_suspend.vhdl`, `sim/tb_suspend_ctrl.vhdl` | unit tests |
| `../uart/` | `rx` receiver + host drivers used to install the utilities over UART |

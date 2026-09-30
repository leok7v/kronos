#!/bin/bash
# ===========================================================================
# NEGATIVE CONTROL for the sdram_model protocol checks.
# ===========================================================================
#
# The model's assertions exist to catch open-row bugs BEFORE they cost a
# hardware flash. An assertion that has never fired is not a check, it is a
# comment -- so this script deliberately BREAKS the controller in each of the
# ways the open-row work could plausibly break it, and requires that the model
# rejects every one.
#
# Why this exists at all: before hardening, sdram_model treated PRECHARGE as a
# no-op and ignored A10 entirely, so it behaved as though the row were always
# open. Every mutation below PASSED against that model. If a future change makes
# these mutations pass again, the model has regressed to useless.
#
#   0  baseline      the real controller               must PASS
#   1  no auto-prech A10=0 on READ/WRITE, nothing else must FAIL (row left open)
#   2  no tRCD       READ issued right after ACTIVE    must FAIL (tRCD)
#   3  refresh open  refresh without precharging       must FAIL (REFRESH)
#
# NOTE ON POLICY. These mutations target the AUTO-PRECHARGE controller, which is
# what this tree currently ships. An open-row version was built and reverted
# (see git history); if it is restored, mutation 1 becomes meaningless -- A10 is
# already 0 -- and the two mutations that matter instead are "refresh without
# precharging first" and "treat a row miss as a row hit". The verify-it-applied
# guard below is what makes that switchover safe rather than silent.
#
# KEEP THESE IN STEP WITH THE CONTROLLER. Each mutation verifies that it
# actually applied and reports "MUTATION DID NOT APPLY" if the source has moved
# on -- a mutation that no longer patches anything tests the unmodified design
# and passes, which looks identical to a healthy check. That already happened
# once: the original mutation set targeted the A10 auto-precharge bit, and when
# the open-row change removed auto-precharge the mutations quietly became
# no-ops.
#
# Usage: ./run_sdram_checks.sh
set -u
cd "$(dirname "$0")"

STD="--std=08 -fsynopsys -frelaxed"
SRC=../src
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

fails=0

run_case () {
    local name="$1" ctrl="$2" expect="$3" want="$4"
    local wd="$TMP/work_$name"
    rm -rf "$wd"; mkdir -p "$wd"
    local out="$TMP/$name.log"

    if ! ghdl -a $STD --workdir="$wd" "$ctrl" sdram_model.vhdl tb_sdram.vhdl >"$out" 2>&1 \
       || ! ghdl -e $STD --workdir="$wd" tb_sdram >>"$out" 2>&1; then
        echo "  $name: BUILD ERROR"; sed -n '1,10p' "$out"; fails=$((fails+1)); return
    fi
    ghdl -r $STD --workdir="$wd" tb_sdram --stop-time=2ms >>"$out" 2>&1

    local got
    if grep -q "PASS: all" "$out"; then got=PASS; else got=FAIL; fi

    if [ "$got" != "$expect" ]; then
        echo "  $name: WRONG OUTCOME -- wanted $expect, got $got"
        grep -E "PROTOCOL|TIMING|MISMATCH" "$out" | head -3
        fails=$((fails+1)); return
    fi
    if [ -n "$want" ] && ! grep -q "$want" "$out"; then
        echo "  $name: rejected, but NOT for the expected reason (wanted /$want/)"
        grep -E "PROTOCOL|TIMING" "$out" | head -3
        fails=$((fails+1)); return
    fi
    echo "  $name: ok ($expect${want:+, $want})"
}

echo "== sdram_model protocol checks =="

# 0 -- the real controller must be accepted. If this fails the model is wrong,
#      not the design: this controller is the one running on the board.
run_case baseline "$SRC/sdram_controller.vhdl" PASS ""

mutate () {   # name, expected outcome, expected message, sed script...
    local name="$1" expect="$2" want="$3"; shift 3
    cp "$SRC/sdram_controller.vhdl" "$TMP/$name.vhdl"
    local s
    for s in "$@"; do sed -i "$s" "$TMP/$name.vhdl"; done
    if cmp -s "$SRC/sdram_controller.vhdl" "$TMP/$name.vhdl"; then
        echo "  $name: MUTATION DID NOT APPLY (controller changed shape?)"
        fails=$((fails+1)); return
    fi
    run_case "$name" "$TMP/$name.vhdl" "$expect" "$want"
}

# 1 -- drop auto-precharge only (A10 1 -> 0). The row is then left open, so the
#      next ACTIVE to a different row is illegal. This is THE mutation the
#      unhardened model missed completely.
mutate no-autoprecharge FAIL "PROTOCOL" \
    "s|sd_a <= \"00\" & '1' & '0' & col;|sd_a <= \"00\" \& '0' \& '0' \& col;|"

# 2 -- issue READ/WRITE immediately after ACTIVE instead of waiting tRCD.
mutate no-trcd FAIL "tRCD" \
    "s|delay <= T_RCD; state <= S_ACTIVE;|delay <= 0; state <= S_ACTIVE;|"

# 3 -- refresh while a row is still open: mutation 1 (which leaves rows open)
#      PLUS a shortened refresh interval, so a refresh falls BETWEEN two
#      accesses rather than after them.
#
#      The interval is load-bearing and was tuned by sweep, not guessed. At the
#      tb's own T_REFI=40 mutation 1 trips the ACTIVE check first, so this case
#      would silently duplicate case 1 -- which is exactly what it did when
#      first written. Too SHORT (<=4) is equally useless: refresh then starves
#      the request path, no row is ever opened, and nothing fires at all,
#      because refreshing an idle bank is perfectly legal. 12 lands a refresh on
#      an open row. If this case ever reports the ACTIVE or tRCD message
#      instead, it has stopped testing REFRESH -- re-sweep it.
mutate refresh-row-open FAIL "REFRESH with bank" \
    "s|sd_a <= \"00\" & '1' & '0' & col;|sd_a <= \"00\" \& '0' \& '0' \& col;|" \
    "s|elsif ref_cnt >= T_REFI then|elsif ref_cnt >= 12 then|"

echo
if [ "$fails" -eq 0 ]; then
    echo "== ALL CHECKS BEHAVED AS INTENDED =="
else
    echo "== $fails CHECK(S) DID NOT BEHAVE AS INTENDED =="
fi
exit $fails

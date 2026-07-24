#!/bin/bash
# ===========================================================================
# NEGATIVE CONTROL for tb_datacache_wb against the 2-WAY write-back cache.
# ===========================================================================
#
# This cache has been broken on Cyclone twice, and both times SILENTLY: garbage
# written back to a garbage address, corrupting memory long before anything
# visibly failed. A green test suite is therefore not evidence on its own -- the
# suite has to be shown capable of catching each failure mode it claims to
# cover.
#
# So every mutation below breaks the cache in one specific, plausible way and
# must be REJECTED, and rejected for the RIGHT reason:
#
#   0  baseline     the real cache                     must PASS
#   1  one-way      replacement pinned to way 0        must FAIL (2-WAY checks)
#   2  flush-hit    write back the HIT way, not victim must FAIL (write-back addr)
#   3  never-dirty  MARK never sets the dirty bit      must FAIL (dirty eviction)
#
# Mutation 1 is the one that matters most and the reason this file exists. A
# 2-way cache that quietly only ever uses one way passes EVERY correctness check
# -- loads, stores, absorb, eviction, coherence -- while delivering exactly none
# of the conflict-miss reduction it was built for. Without a test that fails
# here, "2-way" would be an unverified claim.
#
# Mutations 2 and 3 guard the silent-corruption modes: a write-back that goes to
# the wrong address, and a dirty line that is never recognised as dirty.
#
# Usage: ./run_cache_checks.sh
set -u
cd "$(dirname "$0")"

STD="--std=93 -fsynopsys -fexplicit -frelaxed"
CPU=../../5.0/cpu
SRC=../src
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

fails=0

run_case () {
    local name="$1" cache="$2" expect="$3" want="$4"
    local wd="$TMP/work_$name"; rm -rf "$wd"; mkdir -p "$wd"
    local out="$TMP/$name.log"

    if ! ghdl -a $STD --workdir="$wd" $CPU/uCmdBits.vhdl $CPU/KronosTypes.vhdl >"$out" 2>&1 \
       || ! ghdl -a $STD --workdir="$wd" BlockRam_sim.vhdl "$cache" tb_datacache_wb.vhdl >>"$out" 2>&1 \
       || ! ghdl -e $STD --workdir="$wd" tb_datacache_wb >>"$out" 2>&1; then
        echo "  $name: BUILD ERROR"; grep -E "error" "$out" | head -3; fails=$((fails+1)); return
    fi
    ghdl -r $STD --workdir="$wd" tb_datacache_wb --stop-time=2ms >>"$out" 2>&1

    local got
    if grep -q "ALL WRITE-BACK CACHE TESTS PASSED" "$out"; then got=PASS; else got=FAIL; fi

    if [ "$got" != "$expect" ]; then
        echo "  $name: WRONG OUTCOME -- wanted $expect, got $got"
        grep -E "FAIL:" "$out" | head -3
        fails=$((fails+1)); return
    fi
    if [ -n "$want" ] && ! grep -E "FAIL:" "$out" | grep -q "$want"; then
        echo "  $name: rejected, but NOT for the expected reason (wanted /$want/)"
        grep -E "FAIL:" "$out" | head -3
        fails=$((fails+1)); return
    fi
    echo "  $name: ok ($expect${want:+, $want})"
}

mutate () {   # name, expected, expected-message, sed...
    local name="$1" expect="$2" want="$3"; shift 3
    cp "$SRC/DataCacheFM.vhdl" "$TMP/$name.vhdl"
    local s
    for s in "$@"; do sed -i "$s" "$TMP/$name.vhdl"; done
    if cmp -s "$SRC/DataCacheFM.vhdl" "$TMP/$name.vhdl"; then
        echo "  $name: MUTATION DID NOT APPLY (cache changed shape?)"
        fails=$((fails+1)); return
    fi
    run_case "$name" "$TMP/$name.vhdl" "$expect" "$want"
}

echo "== 2-way write-back cache checks =="

run_case baseline "$SRC/DataCacheFM.vhdl" PASS ""

# 1 -- pin replacement to way 0. The cache is then effectively direct-mapped:
#      every correctness property still holds, and only the associativity checks
#      can tell. If this ever passes, tb_datacache_wb has stopped proving that
#      the second way is used at all.
mutate one-way FAIL "2-WAY" \
    "s|                victim <= not victim;|                victim <= '0';|"

# 2 -- write back the HIT way's tag instead of the VICTIM's, so the flush goes
#      to the wrong address with the wrong data. Right data, wrong place: the
#      exact silent corruption this suite was written to catch.
mutate flush-hit-way FAIL "write-back" \
    "s|adr_o <= vic_keys0(cache_key) & a0_bus1(va_cache_hash);|adr_o <= hit_keys0(cache_key) \& a0_bus1(va_cache_hash);|"

# 3 -- MARK never sets the dirty bit, so an absorbed store is silently lost at
#      eviction: memory keeps the stale value and nothing reports an error.
mutate never-dirty FAIL "" \
    "s|    di0_keys(cache_dirty) <= '1' when marking = '1' else '0';|    di0_keys(cache_dirty) <= '0';|"

echo
if [ "$fails" -eq 0 ]; then
    echo "== ALL CHECKS BEHAVED AS INTENDED =="
else
    echo "== $fails CHECK(S) DID NOT BEHAVE AS INTENDED =="
fi
exit $fails

#!/bin/bash
# Build the pcc passes as NATIVE Kronos .cod from the pristine sources.
# CR-strips (pcc's scanner rejects \r), applies small native patches, compiles.
set -e
cd "$(dirname "$0")"
PR=/home/dmitry/dev/kronos/excelsior/src/users/misc/pcc
B=../bin
INC=../include

# --- cpp (preprocessor): pristine cpp.c + cpy.c, add -o output flag ---
build_cpp() {
  tr -d '\r' < $PR/docpp/cpp.c > cpp.c
  tr -d '\r' < $PR/docpp/cpy.c > cpy.c
  # patch: add a `-o <file>` case to cpp's option switch (pristine writes stdout only)
  python3 - <<'PY'
src=open("cpp.c").read()
anchor='\t\t\tcase \'T\':\n\t\t\t\tncps = 8;\t/* backward name compatability*/\n\t\t\t\tcontinue;\n'
patch=anchor+'\t\t\tcase \'o\':\t/* host-port: output file (pristine writes stdout only) */\n\t\t\t\tif ( NULL == ( fout = fopen( argv[++i], WRITE ) ) )\n\t\t\t\t{\n\t\t\t\t\tfprintf( stderr, "\\ncpp: can\'t create %s\\n", argv[i] );\n\t\t\t\t\texit(2);\n\t\t\t\t}\n\t\t\t\tcontinue;\n'
assert anchor in src, "anchor for -o patch not found"
open("cpp.c","w").write(src.replace(anchor,patch,1))
print("cpp.c: -o patch applied")
PY
  for f in cpp cpy; do
    $B/cpp -Dkronos -DexcII -I$PR/docpp -I$INC $f.c > $f.i 2>/dev/null
    $B/pcc $f.i > $f.a 2>/dev/null
    $B/asm $f.a -o $f.o 2>/dev/null
  done
  $B/clink cpp.o cpy.o >/dev/null 2>&1
  echo "cpp.cod=$(stat -c%s cpp.cod) cpy.cod=$(stat -c%s cpy.cod)"
}

build_cpp

# --- pcc (compiler proper): dopcc/*.c, -DKRONOS2 -DBUG4 ---
build_pcc() {
  local files="cgram code common local2 match order pftn reader scan trees"
  local objs=""
  for f in $files; do
    tr -d '\r' < $PR/dopcc/$f.c > p_$f.c
    $B/cpp -DKRONOS2 -DBUG4 -Dkronos -DexcII -I$PR/dopcc -I$INC p_$f.c > p_$f.i 2>/dev/null
    $B/pcc p_$f.i > p_$f.a 2>p_$f.pccerr
    if grep -qiE "error" p_$f.pccerr; then echo "PCC ERROR in $f:"; head -3 p_$f.pccerr; return 1; fi
    $B/asm p_$f.a -o p_$f.o 2>p_$f.asmerr
    if grep -qiE "error" p_$f.asmerr; then echo "ASM ERROR in $f:"; head -3 p_$f.asmerr; return 1; fi
    objs="$objs p_$f.o"
  done
  echo "pcc objects built: $(echo $objs | wc -w)"
  # the module carrying main() is code.c (p_code.o) -- link all together
  $B/clink $objs 2>&1 | grep -iE "error|unresolv|Linking" | head
  ls -la p_code.cod 2>/dev/null | awk '{print "p_code.cod="$5}'
}
build_pcc

#!/usr/bin/env python3
"""Rewrite Kronos yacc `asm("...")` table blocks into standard C array defs.

The Kronos yacc emitted its parser tables as inline Kronos assembly:

    yytabelem yyexca[];
    asm("
     .inb
    D11111 .int
    %
    -1, 1,
     ... values ...
     -2, 14
    %
     lsta D11111
     sw yyexca
     .ine ")

The `sw NAME` line names the C symbol; the values live between the two `%`
lines. yyparse() indexes these as ordinary C arrays, so each block becomes:

    yytabelem yyexca[] = { -1, 1, ... , -2, 14 };

Usage: deyacc.py FILE.c   (rewrites in place)
"""
import re
import sys

def convert(path):
    text = open(path, "rb").read().decode("latin1").replace("\r", "")
    lines = text.split("\n")
    out, i, n = [], 0, len(lines)
    converted, skipped = 0, 0
    while i < n:
        if lines[i].strip().startswith('asm("'):
            block, j = [], i
            while j < n:
                block.append(lines[j])
                if '")' in lines[j]:
                    break
                j += 1
            pct = [k for k, l in enumerate(block) if l.strip() == "%"]
            name = None
            for l in block:
                m = re.match(r"\s*sw\s+(\w+)", l)
                if m:
                    name = m.group(1)
            if len(pct) >= 2 and name:
                vals = []
                for vl in block[pct[0] + 1:pct[1]]:
                    for tok in vl.split(","):
                        tok = tok.strip()
                        if tok:
                            vals.append(tok)
                out.append("yytabelem %s[] = { %s };" % (name, ", ".join(vals)))
                converted += 1
                i = j + 1
                continue
            else:
                out.append("/* HOSTPORT: NON-TABLE asm block left intact below */")
                out.extend(block)
                skipped += 1
                i = j + 1
                continue
        out.append(lines[i])
        i += 1
    open(path, "w").write("\n".join(out))
    print("%s: converted %d table blocks, %d non-table asm blocks left"
          % (path, converted, skipped))

for p in sys.argv[1:]:
    convert(p)

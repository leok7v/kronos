#!/usr/bin/env python3
"""mkas.py -- Kronos microcode assembler.

A Python 3 reimplementation of the lost Java tool net.largest.tools.mkas.Main
(invoked by the `mkas` bash script), verified by byte-for-byte regeneration of
the checked-in Microcode.vhdl and Kronos.lst.

Usage:
    python3 mkas.py [-l] input.mkas [output.vhdl]

    -l  print the symbol table (Kronos.lst format: "xxxx name", sorted by
        value then name, CRLF line endings) to stdout.

Source language (.mkas):
  //...                        comment (to end of line)
  NAME = <number>              scalar constant (appears in the .lst listing)
  NAME = [ <items> ]           field/macro definition (does NOT appear in .lst)
  DEFAULT = [ <items> ]        template applied to every instruction first
  NAME:                        label (current location; appears in .lst)
  <number>                     org directive: set location counter
  [ <items> ]                  one microinstruction (one ROM word)

  <items> inside [ ]:
    <bit>=<v>                  set single bit <bit> (0..39) to <v>
    <bit>                      (in definitions only) next value-bit position;
                               listed MSB first, e.g. `goto = [ 24=1 23=0 35
                               34 ... 25 ]` places an 11-bit value in 35..25
    <field>=<value>            apply definition <field> with <value>
    <field>                    apply definition <field> with no value
  <value> is a decimal or 0x-hex literal, a scalar constant, or a label.

Instructions are encoded by first applying DEFAULT to an all-zero word, then
applying each item left to right; later items overwrite earlier bits.
Unused ROM words are emitted as all zeros.
"""

import re
import sys

NUM_RE = re.compile(r'^(0[xX][0-9a-fA-F]+|[0-9]+)$')
TOK_RE = re.compile(r'\[|\]|:|=|[^\s\[\]:=]+')

CRLF = '\r\n'

# Non-ROM boilerplate of the generated VHDL, reproduced verbatim from the
# output of the original tool.  {msb} = ADDRESS-SIZE - 1, {top} = 2**ADDRESS-SIZE - 1.
VHDL_HEADER = [
    "library ieee;",
    "use ieee.std_logic_1164.all;",
    "use ieee.std_logic_arith.all;",
    "use ieee.std_logic_unsigned.all;",
    "use ieee.std_logic_textio.all;",
    "use std.textio.all;",
    "use work.uCmdBits.all;",
    "use work.Kronos_Types.all;",
    "",
    "entity Microcode is",
    "    port (",
    "        clock   : in std_logic;",
    "        en      : in std_logic;",
    "        addr    : in std_logic_vector({msb} downto 0);",
    "\tdata\t: out std_logic_vector(ucmd_bits));",
    "end Microcode;",
    "",
    "architecture Behavioral of Microcode is",
    "begin",
    "",
    "    process (clock)",
    "        type rom_type is array (0 to {top}) of std_logic_vector (ucmd_bits);",
    "        constant rom : rom_type :=(",
    "            -- microcode start",
]
VHDL_FOOTER = [
    "            -- microcode end",
    "            );",
    "    begin",
    "       \tif clock'event and clock = '1' and en = '1' then",
    "            data <= rom(conv_integer(addr));",
    " \tend if;",
    "    end process;",
    "",
    "end Behavioral;",
    "",
]


class MkasError(Exception):
    pass


def is_number(tok):
    return NUM_RE.match(tok) is not None


def to_number(tok):
    if tok.lower().startswith('0x'):
        return int(tok, 16)
    return int(tok, 10)


def tokenize(text):
    """Return a list of (token, line_number). '//' starts a comment."""
    toks = []
    for lineno, line in enumerate(text.splitlines(), 1):
        line = line.split('//', 1)[0]
        for tok in TOK_RE.findall(line):
            toks.append((tok, lineno))
    return toks


class Parser:
    """Turns the token stream into a statement list.

    Statements:
        ('label', name, line)
        ('scalar', name, rhs_token, line)
        ('fielddef', name, items, line)
        ('org', number, line)
        ('instr', items, line)
    where items is a list of (lhs_token, rhs_token_or_None, line).
    """

    def __init__(self, toks):
        self.toks = toks
        self.pos = 0

    def peek(self):
        return self.toks[self.pos][0] if self.pos < len(self.toks) else None

    def next(self):
        if self.pos >= len(self.toks):
            raise MkasError("unexpected end of file")
        tok, line = self.toks[self.pos]
        self.pos += 1
        return tok, line

    def parse_items(self, open_line):
        items = []
        while True:
            if self.peek() is None:
                raise MkasError("line %d: missing ']'" % open_line)
            tok, line = self.next()
            if tok == ']':
                return items
            if tok in ('[', ':', '='):
                raise MkasError("line %d: unexpected '%s' inside [ ]" % (line, tok))
            rhs = None
            if self.peek() == '=':
                self.next()
                rhs, rline = self.next()
                if rhs in ('[', ']', ':', '='):
                    raise MkasError("line %d: bad value '%s' after '%s='"
                                    % (rline, rhs, tok))
            items.append((tok, rhs, line))

    def parse(self):
        stmts = []
        while self.peek() is not None:
            tok, line = self.next()
            if tok == '[':
                stmts.append(('instr', self.parse_items(line), line))
            elif tok in (']', ':', '='):
                raise MkasError("line %d: unexpected '%s'" % (line, tok))
            elif self.peek() == ':':
                self.next()
                stmts.append(('label', tok, line))
            elif self.peek() == '=':
                self.next()
                if self.peek() == '[':
                    _, oline = self.next()
                    stmts.append(('fielddef', tok, self.parse_items(oline), line))
                else:
                    rhs, rline = self.next()
                    if rhs in ('[', ']', ':', '='):
                        raise MkasError("line %d: bad value after '%s ='" % (rline, tok))
                    stmts.append(('scalar', tok, rhs, line))
            elif is_number(tok):
                stmts.append(('org', to_number(tok), line))
            else:
                raise MkasError("line %d: unexpected token '%s'" % (line, tok))
        return stmts


class Assembler:
    def __init__(self, stmts):
        self.stmts = stmts
        self.fields = {}    # name -> list of (lhs, rhs, line)
        self.symbols = {}   # name -> int   (scalar constants and labels)

    # ------------------------------------------------------------------ setup
    def collect_defs(self):
        pending = []  # scalar defs, resolved after all are known
        for st in self.stmts:
            if st[0] == 'fielddef':
                _, name, items, line = st
                if name in self.fields or name in self.symbols:
                    raise MkasError("line %d: '%s' redefined" % (line, name))
                self.fields[name] = items
            elif st[0] == 'scalar':
                _, name, rhs, line = st
                if name in self.fields or name in self.symbols:
                    raise MkasError("line %d: '%s' redefined" % (line, name))
                pending.append((name, rhs, line))
                self.symbols[name] = None  # reserve the name
        # resolve scalar constants (may reference other scalar constants)
        while pending:
            progress = False
            rest = []
            for name, rhs, line in pending:
                if is_number(rhs):
                    self.symbols[name] = to_number(rhs)
                    progress = True
                elif self.symbols.get(rhs) is not None:
                    self.symbols[name] = self.symbols[rhs]
                    progress = True
                else:
                    rest.append((name, rhs, line))
            if rest and not progress:
                name, rhs, line = rest[0]
                raise MkasError("line %d: cannot resolve '%s = %s'" % (line, name, rhs))
            pending = rest

    def assign_addresses(self):
        """Pass 1: resolve label addresses; collect (addr, items, line)."""
        self.instrs = []
        loc = 0
        for st in self.stmts:
            if st[0] == 'label':
                _, name, line = st
                if name in self.fields:
                    raise MkasError("line %d: label '%s' clashes with a definition"
                                    % (line, name))
                if name in self.symbols:
                    raise MkasError("line %d: label '%s' redefined" % (line, name))
                self.symbols[name] = loc
            elif st[0] == 'org':
                loc = st[1]
            elif st[0] == 'instr':
                self.instrs.append((loc, st[1], st[2]))
                loc += 1

    # ------------------------------------------------------------- evaluation
    def eval_value(self, tok, line):
        if is_number(tok):
            return to_number(tok)
        v = self.symbols.get(tok)
        if v is None:
            if tok in self.fields:
                raise MkasError("line %d: '%s' is a field, not a value" % (line, tok))
            raise MkasError("line %d: undefined symbol '%s'" % (line, tok))
        return v

    def set_bit(self, word, bit, value, line):
        if not 0 <= bit < len(word):
            raise MkasError("line %d: bit %d out of range" % (line, bit))
        if value not in (0, 1):
            raise MkasError("line %d: bit value must be 0 or 1" % line)
        word[bit] = value

    def apply_field(self, word, name, value, line, depth=0):
        if depth > 32:
            raise MkasError("line %d: recursive definition '%s'" % (line, name))
        items = self.fields.get(name)
        if items is None:
            raise MkasError("line %d: undefined field '%s'" % (line, name))
        value_bits = []
        for lhs, rhs, iline in items:
            if is_number(lhs):
                if rhs is None:
                    value_bits.append(to_number(lhs))
                else:
                    self.set_bit(word, to_number(lhs),
                                 self.eval_value(rhs, iline), line)
            else:
                sub = self.eval_value(rhs, iline) if rhs is not None else None
                self.apply_field(word, lhs, sub, line, depth + 1)
        if value_bits:
            if value is None:
                raise MkasError("line %d: field '%s' requires a value" % (line, name))
            if not 0 <= value < (1 << len(value_bits)):
                raise MkasError("line %d: value 0x%x does not fit in %d bit(s) of '%s'"
                                % (line, value, len(value_bits), name))
            n = len(value_bits)
            for i, bit in enumerate(value_bits):
                self.set_bit(word, bit, (value >> (n - 1 - i)) & 1, line)
        elif value is not None:
            raise MkasError("line %d: field '%s' takes no value" % (line, name))

    def encode(self, items, line, base):
        word = list(base)
        for lhs, rhs, iline in items:
            if is_number(lhs):
                if rhs is None:
                    raise MkasError("line %d: bare bit position in instruction" % iline)
                self.set_bit(word, to_number(lhs), self.eval_value(rhs, iline), iline)
            else:
                value = self.eval_value(rhs, iline) if rhs is not None else None
                self.apply_field(word, lhs, value, iline)
        return word

    # --------------------------------------------------------------- assembly
    def assemble(self):
        self.collect_defs()
        self.assign_addresses()
        width = self.symbols.get('INSTRUCTION-SIZE') or 40
        asize = self.symbols.get('ADDRESS-SIZE') or 11
        size = 1 << asize
        base = [0] * width
        if 'DEFAULT' in self.fields:
            base = self.encode(self.fields['DEFAULT'], 0, base)
        rom = [None] * size
        for addr, items, line in self.instrs:
            if not 0 <= addr < size:
                raise MkasError("line %d: address 0x%x outside ROM (size 0x%x)"
                                % (line, addr, size))
            if rom[addr] is not None:
                raise MkasError("line %d: address 0x%x assembled twice" % (line, addr))
            rom[addr] = self.encode(items, line, base)
        fill = '0' * width
        self.words = [''.join(str(b) for b in reversed(w)) if w is not None else fill
                      for w in rom]
        self.width = width
        self.asize = asize

    # ----------------------------------------------------------------- output
    def vhdl(self):
        lines = [l.format(msb=self.asize - 1, top=(1 << self.asize) - 1)
                 for l in VHDL_HEADER]
        last = len(self.words) - 1
        for i, w in enumerate(self.words):
            lines.append('            "%s"%s' % (w, ',' if i != last else ''))
        lines.extend(VHDL_FOOTER)
        return CRLF.join(lines)

    def listing(self):
        entries = sorted(self.symbols.items(), key=lambda kv: (kv[1], kv[0]))
        return ''.join('%04x %s%s' % (v, name, CRLF) for name, v in entries)


def main(argv):
    args = [a for a in argv[1:] if a != '-l']
    want_listing = '-l' in argv[1:]
    if not 1 <= len(args) <= 2:
        sys.stderr.write(__doc__.split('\n\n')[2] + '\n')
        return 2
    with open(args[0], 'r') as f:
        text = f.read()
    try:
        asm = Assembler(Parser(tokenize(text)).parse())
        asm.assemble()
    except MkasError as e:
        sys.stderr.write('%s: %s\n' % (args[0], e))
        return 1
    if len(args) == 2:
        with open(args[1], 'wb') as f:
            f.write(asm.vhdl().encode('ascii'))
    if want_listing:
        sys.stdout.buffer.write(asm.listing().encode('ascii'))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

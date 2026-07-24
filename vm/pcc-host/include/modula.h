/* SS. 1.Apr.87  macro for implementing C function call from Modula procedure */

#define modula_entry(p,l)  asm("	li	p "); asm("	li	l "); \
			   asm("	add "); asm("	decs "); \
			   asm("	store "); \
			   asm("	li	1 "); asm("	decs "); \
			   asm("	entr	l ")


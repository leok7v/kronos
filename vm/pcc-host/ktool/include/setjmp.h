/*	@(#)setjmp.h	1.3	*/
#ifndef _JBLEN

#define _JBLEN	20

typedef int jmp_buf[_JBLEN];

extern int setjmp();
extern void longjmp();
#endif


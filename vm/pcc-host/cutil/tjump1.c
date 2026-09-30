#include <stdio.h>
#include <setjmp.h>

extern jmp_buf	env;

f()
{
int	c;

	printf("Enter something: ");
	c = getchar();
	if (c == '\n')
		longjmp (env, -1);
	else longjmp (env, c);
}


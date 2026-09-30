#include <stdio.h>
#include <setjmp.h>

jmp_buf	env;

main()
{
int 	ret;

printf("starting MAIN\n");

	for (;;) {
	switch (ret = setjmp (env)) {
		case 0:
			printf("Testing longjumps\n");
			f();
			break;
		case -1:
			printf("Jumped back after RETURN\n");
			exit(0);
		case 'A': printf("\n\tA\n");
			break;
		case 'n': printf("\n\tn\n");
			break;
		default:
			printf("Jumped back with code=%d sym=%c\n", ret, ret);
			break;
		}
	continue;
	}
}


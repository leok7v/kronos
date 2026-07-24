#include "stdio.h"
#include "ctype.h"

main (argc, argv)
int argc;
char * argv[];
{
int i, l = 0, fd, sz, term, lines = 1;
char	c[8], * fmt;

	if (argc < 2) {
		fprintf(stderr, "Usage:   da  file  [-x]\n");
		return(1);
		}
	fd = open(argv[1], 0);

	if (fd == -1) {
		fprintf(stderr, "No file: %s\n", argv[1]);
		return (1);
		}
	if (argc > 2 && !strcmp(argv[2], "-x")) fmt = "%08x  ";
	else fmt = "%4d  ";

	sz = read(fd, c, 8);
	if (sz == 0) {
		fprintf(stderr, "Empty file\n");
		return (0);
		}
	term = isatty (1);	/* if stdout -> terminal */

	if (sz == 8) do {
		printf(fmt, l);
		l += 2;
		for (i=0; i < 8; i ++)
			printf("-%2x-", c[i]& 0377);
		printf("\t");
		for (i=0; i < 8; i ++)
			printf( isprint(c[i]&0377) ? "%c " : "_ ", 
					c[i]&0377);
		printf("\n");
		sz = read(fd, c, 8);
		if (term && (++lines) % 23 == 0) {
			printf("\t\t\t\t\t\t\t\tPress <ret> ");
			while (getchar() != '\n');
			}
		}
	while (sz == 8);
	if (sz) {
		printf(fmt, l);
		for (i=0; i < sz; i ++)
			printf("-%2x-", c[i]& 0377);
		printf("\t");
		if (sz < 7) printf("\t");
		if (sz < 5) printf("\t");
		if (sz < 3) printf("\t");
		for (i=0; i < sz; i ++)
			printf( isprint(c[i]&0377) ? "%c " : "_ ", 
					c[i]&0377);
		printf("\n");
		}
	close (fd);
	return (0);
}


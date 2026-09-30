
/*
 *	CONVERT: convert files
 *
 * Usage: convert ifile ofile [oldchar newchar]
 *
 */

#include <stdio.h>

#define	RS	0x1E
#define NL	0x0A

main(argc, argv)
int	argc;
char	*argv[];
{FILE	*fopen(), *ifile, *ofile;
 int	oldchar = RS, newchar = NL;
 unsigned char c;
 int	count = 0;

	if (!(argc == 3 || argc == 5)) {
	    fprintf(stderr, "Usage: cvt ifile ofile [oldchar newchar]\n");
	    fprintf(stderr, "\tby default substitute 1E with 0A\n");
	    exit(1);
	    }
	if ((ifile = fopen(argv[1], "r")) == (FILE *) 0) {
	    fprintf(stderr, "No file: %s\n", argv[1]);
	    exit(1);
	    }
	if ((ofile = fopen(argv[2], "w")) == (FILE *) 0) {
	    fprintf(stderr, "Can't create %s\n", argv[2]);
	    exit(1);
	    }
	if (argc == 5) {
	    if (!(sscanf(argv[3], "%x", &oldchar)) || 
		!(sscanf(argv[4], "%x", &newchar))) {
		fprintf(stderr, "Syntax error in hex numbers\n");
		exit(1);
		}
	    }
	if( argc == 5 )
	   fprintf(stderr, "Substituting 0x%02x with 0x%02x\n", oldchar, newchar);
	oldchar &= 0xFF;
	newchar &= 0xFF;

	c = getc (ifile);

	while (!feof(ifile)) {
		if (c == oldchar) {
		    count ++;
		    putc (newchar, ofile);
		    }
		else putc (c, ofile);
		c = getc (ifile);
		}
	fclose(ifile);
	fclose(ofile);
	if( argc == 5 )
	   	fprintf(stderr, "%d substitutions\n", count);
}


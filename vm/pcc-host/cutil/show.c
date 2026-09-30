
/*
 *	SHOW		06.Apr.87
 *
 *  Display information about Kronos C data files:
 *	object files
 *	library files
 *	Kronos code files
 *
 *  12.Jun.87 Kronos 2.5 code file
 *  16.Oct.87 Kronos 2 & 2.5 code & object files
 *  05.Feb.88 New object file versions (Asm.h)
 *  15.Feb.88 Still newer object file: pool before symbols
 *  19.Feb.88 -s: show import symbols for object files
 *  18.Apr.88 show heap and file number for code files
 */

#include <stdio.h>
#include "Asm.h"
#include "mkinstr.h"

/*	File types */

#define T_OBJ2	1
#define	T_COD2	2
#define T_OBJ25	3
#define T_COD25 4
#define T_LIB	5

typedef struct {
	char	name [MAXIDENT + 1];
	int	num;
	}
		EXPTSYM;

typedef	struct	{
	char	name [MAXIDENT + 1];
	int	creat_time;
	int	varno;
	int	funno;
	int	offset;
	}
		FILEREC;

FILEREC	buf [10];
int	header [LHDSIZE]; /* big enough for all file headers */
int	lines, term;

char	* files[5] = {"2 C object", "2 code", "2.5 C object", "2.5 code",
			"C library"};

/*	M A I N	*/

main(argc, argv)
char	* argv[];
{
int	fd, i, j, filetype = 0, 
	varno, funno, num, flag = 0, entrysize;
EXPTSYM	sym;
char	* ctime();
char	buffer [512], word[4];

	if (argc < 2)
		error ("\nUsage : Show file\n");

	if ((fd = open(argv[1], 0)) == -1)
		error ("\nNo file : %s\n",argv[1]);

	if (!strcmp(argv[2], "-s")) flag = 1;
	read (fd, (char *) & header[0], 8);
	if (header[0] == OBJ2_VERSION) filetype = T_OBJ2;
	else if (header [0] == COD2_VERSION) filetype = T_COD2;
	else if (header[0] == OBJ25_VERSION) filetype = T_OBJ25;
	else if (header [0] == COD25_VERSION) filetype = T_COD25;
	else if (header [0] == LIB_VERSION) filetype = T_LIB;

	if (!filetype)
		error ("\nUnknown file type: %s\n", argv[1]);
	else printf("\n\t***  %s  ***  (Kronos %s file)\n", argv[1],
				files[filetype - 1]);
	if (filetype == T_OBJ2 || filetype == T_COD2) {
		printf("\n\tNB !!!  OLD  VERSION !!!\n");
		entrysize = 2;
		}
	else entrysize = 4;
#if unix
	header[1] += MAGICTIME;
#endif
	printf("\nCREATED : %s", ctime ( & header [1]));
	lines = 4;
	term = isatty (1);	/* true if stdout -> terminal */

	switch (filetype) {

	case T_OBJ2:	 	/* OBJECT file */
	case T_OBJ25:

		printf("\nHEADER :\n");
		read (fd, (char *) & header[2], 4 * (HDSIZE - 2));
		funno = header [9];
		varno = header [10];
		lseek (fd, 4 * (HDSIZE + header[6]), 0);
		printf("\tVariables\t\t%d\tFunctions\t\t%d\n", varno-2,funno-1);
		printf("\tExport variables\t%d\tExport functions\t%d\n",
				header[2], header[3]);
		printf("\tImport variables\t%d\tImport functions\t%d\n",
				header[4], header[5]);
		printf("\tPool\t\t\t%d\tCode\t\t\t%d\n\tStack\t\t\t%d\n",
				header[6], header[8], header[12]);
		lines += 6;
		if (header [11]) {
			printf("\tmain() modification:\t%d\n", header [11]);
			lines += 1;
			}
		if (header[2]) {
			printf("\nVARIABLES :\n");
			lines += 2;
			for (i = 2, j = 0; j < header[2]; j ++, i++) {
				read (fd, (char *) & sym, sizeof (EXPTSYM));
				while (i < sym.num) {
					printf("\t%2d :  -\n", i);
					i ++; stop();
					}
				printf ("\t%2d :  %s\n", i, sym.name);
				stop();
				}
			while (i < varno) {
				printf("\t%2d :  -\n", i);
				i ++; stop();
				}
			}
		if (header[3]) {
			printf("\nFUNCTIONS :\n");
			lines += 2;
			for (i = 1, j = 0; j < header[3]; j ++, i++) {
				read (fd, (char *) & sym, sizeof (EXPTSYM));
				while (i < abs(sym.num)) {
					printf("\t%2d :  -\n", i);
					i ++; stop();
					}
				printf ("\t%2d :  %s", i, sym.name);
				if (sym.num < 0) printf("\t(void)");
				printf("\n");
				stop();
				}
			while (i < funno) {
				printf("\t%2d :  -\n", i);
				i ++; stop();
				}
			}
		if (flag && header[4]) {
			printf("\nIMPORT VARIABLES :\n");
			lines += 2;
			for (j = 0; j < header[4]; j ++) {
				read (fd, (char *) & sym, MAXIDENT + 1);
				printf ("\t%2d :  %s\n", j + 1, sym.name);
				stop();
				}
			}
		if (flag && header[5]) {
			printf("\nIMPORT FUNCTIONS :\n");
			lines += 2;
			for (j = 0; j < header[5]; j ++) {
				read (fd, (char *) & sym, MAXIDENT + 1);
				i = 0;
				if (sym.name[MAXIDENT] == '\1') {
					i = 1;
					sym.name[MAXIDENT] = '\0';
					}
				printf ("\t%2d :  %s", j + 1, sym.name);
				if (i) printf("\t(void)");
				printf("\n");
				stop();
				}
			}
		printf("\n");
		break;

	case T_COD2:		/*  CODE  file */
	case T_COD25:

		printf("\nHEADER :\n");
		read (fd, (char *) & header [2], 4 * (LHDSIZE - 2));
		printf("\tGlobals\t\t%d\tFunctions\t%d\n", header[7]-header[8],
								header[9]);
		printf("\tPool\t\t%d\tCode\t\t%d\n\tStack\t\t%d\n",
				header[3], header[4], header[5]);
		i =  4*LHDSIZE + 4*header[3] + entrysize*(header[9] - 1);
		lseek (fd, i, 0);
		i = 0;
		read (fd, (char *)&i, entrysize);
		lseek (fd, i - (entrysize*header[9]) + 15, 1);
		read (fd, word, 4);
		if (word[0] == LIB)  
			printf("\tHeap\t\t%d\tFile buffers\t%d\n", 
				word[1] & 0377, word[2] & 0377);
			
		lseek (fd, 4*LHDSIZE + 4*header[3] + 4*header[4], 0);
		read (fd, buffer, 512);
		printf("\nMODULES :\n");
		i = 0;
		if (header[8] <= 1) printf("\tno modules\n");
		else while (-- header[8] > 0) {
			printf("\t%s\n", buffer+i);
			i += strlen (buffer+i);
			if (i % 4)
				i += (4 - i % 4);
			else i += 4;
			}
		break;

	case T_LIB:		/* LIBRARY file */
		
		read (fd, (char *) & num, 4);
		printf("\n  %d  modules\n", num);
		read (fd, (char *) & buf[0], num * sizeof (FILEREC));
		for (i = 0; i < num; i ++) {
			long	otime = buf[i].creat_time;
#if unix
			otime += MAGICTIME;
#endif
			printf("\n******  %d    %-16sCREATED %s\n", i + 1,
				buf[i].name, ctime (& otime));
			printf("Variables:\t%d\tFunctions:\t%d\n",
				buf[i].varno, buf[i].funno);
			lines += 8;

			if (buf[i].varno) {
			    printf("\nVARIABLES :\n");
			    for (j = 0; j < buf[i].varno; j ++) {
				read (fd, (char *) & sym, sizeof (EXPTSYM));
				printf ("\t%2d :  %s\n", sym.num, sym.name);
				stop();
				}
			    }
			if (buf[i].funno) {
			    printf("\nFUNCTIONS :\n");
			    for (j = 0; j < buf[i].funno; j ++) {
				read (fd, (char *) & sym, sizeof (EXPTSYM));
				printf ("\t%2d :  %s", abs(sym.num), sym.name);
				if (sym.num < 0) printf("\t(void)");
				printf("\n");
				stop();
				}
			    }
			}
		printf("\n");
		break;
		}

	exit (0);
}


error (s, t)
char	* s, * t;
{
	fprintf (stderr, s, t);
	exit (1);
}

stop()
{
	if (term && (++lines) % 22 == 0) {
		printf("\t\t\t\t\t\t\t\tPress <ret> ");
		while (getchar() != '\n');
		}
}

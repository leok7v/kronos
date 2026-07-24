/*
 *	MTIME  by S.Siibak	22.Dec.87
 *
 *  Modify the creation time of Kronos C data files:
 *	object files
 *	library files
 *	Kronos code files
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include "Asm.h"

/*	File types */

#define T_OBJ2	1
#define	T_COD2	2
#define T_OBJ25	3
#define T_COD25 4
#define T_LIB	5

int	header [LHDSIZE]; /* big enough for all file headers */

char	* files[5] = {"2 C object", "2 code", "2.5 C object", "2.5 code",
			"C library"};

/*	M A I N	*/

main(argc, argv)
char	* argv[];
{
long 	curtime, time();
int	fd, res, filetype = 0; 
char	*p, *ctime();

	if (argc != 2){
		fprintf (stderr,"\nUsage : mtime file\n");
 		error("Purpose: Modify the creation time of Kronos C code file.\n");
	}

	if ((fd = open(argv[1], O_RDWR)) == -1)
		error ("\nNo file : %s\n",argv[1]);

	read (fd, (char *) & header[0], 12);
	if (header[0] == OBJ2_VERSION) filetype = T_OBJ2;
	else if (header [0] == COD2_VERSION) filetype = T_COD2;
	else if (header[0] == OBJ25_VERSION) filetype = T_OBJ25;
	else if (header [0] == COD25_VERSION) filetype = T_COD25;
	else if (header [0] == LIB_VERSION) filetype = T_LIB;

	if (!filetype)
		error ("\nUnknown file type: %s\n", argv[1]);
	else printf("***  %s  ***  (Kronos %s file)\n\n", argv[1],
				files[filetype - 1]);
	if (filetype == T_OBJ2 || filetype == T_COD2)
		printf("NB !!!  OLD  VERSION !!!\n\n");

#if unix
	header[1] += MAGICTIME;
#endif
	p = ctime( &header[1] );
	p[strlen(p)-1] = 0;
	printf("Creation time: %s", p );
	
        curtime = time(0L);
#if unix
        header[1] = (int) curtime - MAGICTIME; /* definition module creation time */
#else
        header[1] = (int) curtime; /* definition module creation time */
#endif
        if (filetype == T_COD25 || filetype == T_COD2)
           	header[2] = header[1]; /* implementation module creation time */

        printf("   Set to: %s\n", ctime(& curtime) );

        if( (res = lseek(fd,0,0)) == -1 )
		error("\nCan't seek.\n");
	write( fd,(char *) &header[0], 12);
	close(fd);
	return(0);
}


error (s, t)
char	* s, * t;
{
	fprintf (stderr, s, t);
	exit (1);
}

/*			    M I C R O L					*/
/*			    ===========					*/
/*									*/
/*  Function								*/
/*	downloads character set (#2) to the Microline 93		*/
/*									*/
/*  Usage								*/
/*	microl [-h] [-c[w][h]] [-f<filename>]				*/
/*	 h	generate help message					*/
/*	 c	condensed print:					*/
/*	    w	12 CPI letters (default 10 CPI)				*/
/*	    b	bold letters						*/
/*	    h	8 LPI lines (default 6 LPI)				*/
/*	 f	read font from the specified file			*/
/*									*/
/*  Operation								*/
/*	Font description is read from the file  (default "MICROLIN.DEF"	*/
/*	on the current disk drive). Each line from this file 		*/
/*	describes one character image in the following format:		*/
/*	    %c %x %x %x %x %x %x %x %x %x %x <comment>			*/
/*	where the first character is either 'A'(ascender) 		*/
/*	or 'D'(descender); line is ignored if any other 		*/
/*	character is found in this position.				*/
/*	The next 9 hex bytes define image (cf. Microline 93		*/
/*	User's Manual). The last hexbyte defines character code.	*/ 
/*	The bytes are transferred to the printer by bdos call#5		*/
/*									*/
/*   Modification log							*/
/*	Date		Author		Reason				*/
/*	24/01/85  A.L.Chmoundack	Written				*/
/*	11/06/85  A.L.Chmoundack	Modified for Hi-Tech C		*/
/*					File missing error is reported	*/
/*	18/08/85  A.L.Chmoundack	Parameters added		*/
/*	15/11/85  A.L.Chmoundack	Tab stops every 8 positions	*/
/*	11/12/87  S.Siibak		updated for KRONOS (see bdos()) */

#include <stdio.h>
#ifdef CPM
#include <cpm.h>	
#endif
#include <ctype.h>
#if excII
#include <fcntl.h>
#endif

FILE	*infile,
	*fopen(),*fopenb();

char	*fgets();

char	fname[60] = "MICROLIN.DEF";

int	cw = 0,				/* condensed width flag		*/
	ch = 0,				/* condensed lines flag		*/
	cb = 0;				/* bold letters flag		*/

char	help[] = 
"\nUsage\n    microl [-h] [-c[w][h]] [-f<filename>]\n\
-c\tcondensed print\n\
    w\tcondensed (12 CPI) letters (default 10 CPI)\n\
    h\tcondensed ( 8 LPI) lines   (default  6 LPI)\n\
    b\tbold letters\n\
-f\tread font from the specified file (default MICROLIN.DEF)\n\n";

/*
-h\tgenerate help message\n\
*/

char	tabstop[] =
"\033\t001,009,017,025,033,041,049,057,065,073,081,089,097\r";

#if excII
char buffer[5000];  /* size of file MICROLIN.DEF is 3376 bytes */
int cnt, outfile;

void bdos(c){
	buffer[cnt++] = c;
}
#endif

main(argc,argv)
int	argc;
char	*argv[];
{
char	*s,c;
int res;
    for (argc--, s = *++argv; argc; --argc, s = *++argv) { /* SS fixed bug */
	if (*s++ == '-') {
	    c = *s++;
	    if ((c == 'h') || (c == 'H')) {
		fputs(help,stderr);
		exit(1);
		}
	    else if ((c == 'c') || (c == 'C')) {
		while ((c = *s++)) 
		    if ((c == 'w') || (c == 'W'))
			cw++;
		    else if ((c == 'h') || (c == 'H'))
			ch++;
		    else if ((c == 'b') || (c == 'B'))
			cb++;
		    else
			fputs("invalid c-parameter, ignored\n",stderr);
		}
	    else if ((c == 'f') || (c == 'F')) {
#if excII
		strcpy(fname,s);
#endif
#ifdef CPM
		makefname(fname,s);
#endif
		}
	    else 
		fputs("invalid option, ignored\n",stderr);
	    }
	else {
	    fputs("invalid command line\n",stderr);
	    fputs(help,stderr);
	    exit(1);
	    }
	}
#if excII
    if( (outfile = open("/dev/lp",O_WRONLY)) == -1){
	printf(stderr, "Can't open: /dev/lp\n");
	exit(1);
    }
#endif
    process();
#if excII
    if( (res = write( outfile,buffer,cnt )) != cnt)
	printf(stderr, "Can't write all bytes: %d\n",res);
    close(outfile);
#endif
    return;
}
	    
process()
{int 	chno, 
	c1,c2,c3,c4,c5,c6,c7,c8,c9;
 char	inbuf[80], ad,*s;
	/* SS. 18.Jun.87 local char must be zeroed before using scanf() */
    ad = 0;
    if (!(infile = fopen(fname,"r"))) {
	fprintf(stderr,"%s file is missing\n",fname);
	exit(1);
	}
    while (fgets(inbuf,80,infile)) {
	if (sscanf(inbuf,		/* SS. 18.Jun.87 added char * cast */
	    "%c  %x   %x   %x   %x   %x   %x   %x   %x   %x  %x",
	    (char *)&ad, &c1, &c2, &c3, &c4, &c5, &c6, &c7, &c8, &c9,&chno) == 11) {
	    if ((ad == 'D') || (ad == 'A')) {
		bdos('\033'); bdos('%');
		bdos(ad);  bdos(chno);
		bdos(c1);  bdos(c2);  bdos(c3);
		bdos(c4);  bdos(c5);  bdos(c6);
		bdos(c7);  bdos(c8);  bdos(c9);
		bdos(0);   bdos(0);
		}
	    else {
		fprintf(stderr,"A or D must be in this position\n");
		exit(1);
		}
	    }
	}
    bdos('\033');  bdos('2');
    ad = (cw)? 0x1c : 0x1e;			/* Character		*/
    bdos(ad);				/*     width		*/
    ad = (ch)? '8' : '6';			/* Line			*/
    bdos('\033'); bdos(ad);	/*    spacing		*/
    ad = (cb)? 'T' : 'I';			/* Bold/normal		*/
    bdos('\033'); bdos(ad);	/*    spacing		*/
    for (s = tabstop; *s; )
	bdos(*s++);
    fputs("OK\n",stderr);
    return;
}

#ifdef CPM
makefname(to,from)
char	*to,*from;
{char	c;
    do {
	c = *from++;
	*to++ = (isalpha(c)) ? toupper(c) : c;
	} while (c);
    return;
}
#endif

/****** docmake.c       create a doc file from "C" source file
 *
 *   Usage:        DOCMAKE  infile  [outfile]
 *
 *           outfile     default : infile.doc
 *
 *           infile is read and all characters
 *              outside  {  } pairs are written to outfile
 *              Includes function headers, etc.
 *
 *                          DOCMAKE
 *
 *              C Documentation Utility
 *
 *             For "standard" C compilers
 *
 *             Author:  Joseph E. Ramus     August 1984
 *
 *    Revisions:      March   1985   by  Joseph E. Ramus
 *                    April   1985   by  Dan Teven
 *		      January 1987 by  Sulev Siibak
 * This program contributed to C Helper by Joseph E. Ramus.
 *
 */

#include <stdio.h>
#include "chelper.h"

#include <ctype.h>


#define LBRACE  123   /* ASCII char left brace  */
#define RBRACE  125   /* ASCII char right brace */
#define SLASH    47   /* ASCII char slash       */
#define ASTERISK 42   /* ASCII char asterisk    */
#define SQUOTE   39   /* ASCII char single quote  */
#define DQUOTE   34   /* ASCII char double quote  */
#define CTLZ     26   /* ASCII Control Z        */

FILE *fp1, *fp2;    /* file pointers for buffered I/O */
FILE *fopen();
char infile[84];    /* input file path name */
char infile1[84];   /* another input file path name */
char wfile[84];     /* output file path name */
int  fno;	    /* function number in file */

/*------ - - - - - - - - - - - - - - - - - - - - - - */
main (argc, argv)
int argc;
char *argv[];
{
    char *pc;

    fp2 = stdout;    /* changed later  */
    fp1 = stdin;
    banner();

    /*  check number of input items */
    if (argc < 2)
        {
        fprintf(stderr, "\n Usage:    DOCMAKE infile [outfile]\n\n");
        exit();
        }

    /*  open input file  */
    strcpy(infile, argv[1]);
    if ((fp1 = fopen(infile, "r")) == NULL){
	strcpy(infile1, argv[1]);
	strcat( infile1, ".c" );
	if( (fp1 = fopen(infile1, "r") )== NULL){
        	fprintf(stderr, " Input file(s) missing:  %s, %s\n", infile,infile1);
        	exit();
        }
	strcpy(infile,infile1);
    }

    /*  open output file  */
    if (argc > 2)
        strcpy(wfile, argv[2]);
    else
        {
        /*  form outfile name by changing  .c   to  .doc  */
        strcpy (wfile, argv[1]);
        strcat (wfile, ".doc");
        }
    if ((fp2 = fopen(wfile, "w")) == NULL)
        {
        fprintf(stderr, " Output file create error: %s\n", wfile);
        fclose(fp1);
        exit();
        }
    fprintf(stderr, " Output file is:  %s\n", wfile);

    fprintf(fp2, " DOCMAKER Version 1.1\n" );
    fprintf(fp2, " Documentation file is:  %s\n", wfile);
    fprintf(fp2, " Input file was:         %s\n\n", infile);

    fromto();       /*  process input to output  */

    /*  close files  */
    fclose(fp1);
    fclose(fp2);
    /* end of main */
}

/*------ - - - - - - - - - - - - - - - - - - - - - - */
int
banner()
{
    fprintf(stderr, " DOCMAKER Version  1.1\n" );
    fprintf(stderr, " Copyright (c) 1985 Solution Systems\n");
}

/*------ - - - - - - - - - - - - - - - - - - - - - - */
/*  copy documentation from input to output   */
int
fromto()
{
    short num=0;
    short nc;
    char  ch;

    while (TRUE){
        if ((nc = getc(fp1)) == EOF)
            return(0);
        if (nc == LBRACE){
            if (skips() == EOF)
                return(0);
	    else           /* SS. 04.feb.87 fixed the bug, that there is */
            	           /* no need to output last char before { twice */
		continue;
	}
        else
            /*  count successive blank lines,  omit if exceeds 2 */
        ch = nc;
        if (ch == '\n')
            ++num;
        else
            num = 0;
        if (num <= 3)
            putc(ch, fp2);
    }
}

/*------ - - - - - - - - - - - - - - - - - - - - - - */
/*  skip over { } pairs       */
int
skips()
{
    short nc;

    while (TRUE){
        if ((nc = getc(fp1)) == EOF)
            return(EOF);
start:
        switch (nc){

        case RBRACE:
            return(0);
        case LBRACE:
            if (skips() == EOF)        /* recursive call */
                return(EOF);
            break;
        case SLASH:
            nc = getc(fp1);
            if (nc == ASTERISK)
                nc = hunt(ASTERISK);
            break;
        case DQUOTE:
            nc = hunt(DQUOTE);
            break;
        case SQUOTE:
            nc = hunt(SQUOTE);
	    nc = getc(fp1);	/* SS. 06-Feb-87 for situations like '\'' */
	    if( nc == SQUOTE )
            	break;
	    else
		goto start;
        default:
            break;
       }      /*  end switch */
    }     /*  end while  */
}

/*------ - - - - - - - - - - - - - - - - - - - - - - */
/*  hunt for target char  */
int
hunt(target)
int target;
{
    short nc;

    while (TRUE){
        if ((nc = getc(fp1)) == EOF)
            return(EOF);
        if (nc == target){
            if (target == ASTERISK){
                nc = getc(fp1);
                if (nc == SLASH)
                    return(0);
            }
            else
                return(0);
        }
    }
}

/*------ - - END OF FILE  - - - - - - - - - - - - - - - - - */


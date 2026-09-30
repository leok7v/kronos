/*	ascii.c	ASCII Table Generator
 *
 *	usage: ascii
 *
 *	Last revisions:	March 1984  by Bob Green
 *			April 1985  by Dan Teven
 *			Oct   1986  by S.Siibak
 */

#include "stdio.h"

main(argc,argv)
char *argv[];
int argc;
{
	register int g, i, j, k;
	char name[4];

	printf("\n\tASCII Table Generator, v1.3");
	printf("\n\tCopyright (c) 1986 KDG\n");

	printf("\n|---------------------|-----------------|-----------------|");
	printf("\n| sig chr dec hex oct | chr dec hex oct | chr dec hex oct |");

	for (i=0; i < 43; i++)
	{
#ifdef DELAY
		if (argc==1)
			for (g=0; g<10000; g++) 
				/* delay */;
#endif
		j = i + 43; 
		k = j + 43;
		if (i % 5 == 0) 
			printf("\n|---------------------|-----------------|-----------------|");

		char_name(i, name);
		if (i < 32)
			printf("\n| %s  ^%c  %02d  %02x %03o ", name, (64 + i), i, i, i);
		else
			printf("\n| %s   %c  %02d  %02x %03o ", name, i, i, i, i);
		printf("|   %c  %02d  %02x %03o ", j, j, j, j);
		if (k < 127)
			printf("|   %c %3d  %02x %03o |", k, k, k, k);
		else
		{
			if (k == 127)
				printf("| DEL %3d  %02x %03o |", k, k, k);
			else			
				printf("|                 |");
		}
	}
	printf("\n|---------------------|-----------------|-----------------|\n");
}

char_name(n, cname)
int n;			  
char cname[4];	
{
	switch(n) 
	{
		case 0 :
			strcpy(cname, "NUL");
			break;
		case 1 :
			strcpy(cname, "SOH");
			break;
		case 2 :
			strcpy(cname, "STX");
			break;
		case 3 :
			strcpy(cname, "ETX");
			break;
		case 4 :
			strcpy(cname, "EOT");
			break;
		case 5 :
			strcpy(cname, "ENQ");
			break;
		case 6 :
			strcpy(cname, "ACK");
			break;
		case 7 :
			strcpy(cname, "BEL");
			break;
		case 8 :
			strcpy(cname, " BS");
			break;
		case 9 :
			strcpy(cname, " HT");
			break;
		case 10 :
			strcpy(cname, " LF");
			break;
		case 11 :
			strcpy(cname, " VT");
			break;
		case 12 :
			strcpy(cname, " FF");
			break;
		case 13 :
			strcpy(cname, " CR");
			break;
		case 14 :
			strcpy(cname, " SO");
			break;
		case 15 :
			strcpy(cname, " SI");
			break;
		case 16 :
			strcpy(cname, "DLE");
			break;
		case 17 :
			strcpy(cname, "DC1");
			break;
		case 18 :
			strcpy(cname, "DC2");
			break;
		case 19 :
			strcpy(cname, "DC3");
			break;
		case 20 :
			strcpy(cname, "DC4");
			break;
		case 21 :
			strcpy(cname, "NAK");
			break;
		case 22 :
			strcpy(cname, "SYN");
			break;
		case 23 :
			strcpy(cname, "ETB");
			break;
		case 24 :
			strcpy(cname, "CAN");
			break;
		case 25 :
			strcpy(cname, " EM");
			break;
		case 26 :
			strcpy(cname, "SUB");
			break;
		case 27 :
			strcpy(cname, "ESC");
			break;
		case 28 :
			strcpy(cname, " FS");
			break;
		case 29 :
			strcpy(cname, " GS");
			break;
		case 30 :
			strcpy(cname, " RS");
			break;
		case 31 :
			strcpy(cname, " US");
			break;
		case 32 :
			strcpy(cname, "SPC");
			break;
		default :
			strcpy(cname, "   ");
			break;
	}
}


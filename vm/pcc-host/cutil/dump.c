/*		dump.c		core style dump of a file	*/

/*		Usage: A>DUMP hello.c					*/
/* Updated: 09.sep.87 by S.Siibak added some casts to avoid user errors */
/* File hello.c dumped using this program:

0000  23 69 6E 63 6C 75 64 65 20 3C 73 74 64 69 6F 2E
      #  i  n  c  l  u  d  e     <  s  t  d  i  o  .

0010  68 3E 0A 0A 6D 61 69 6E 28 29 0A 0A 7B 0A 70 72
      h  >  .  .  m  a  i  n  (  )  .  .  {  .  p  r

0020  69 6E 74 66 28 22 48 65 6C 6C 6F 20 77 6F 72 6C
      i  n  t  f  (  "  H  e  l  l  o     w  o  r  l

0030  64 21 5C 6E 22 29 3B 0A 7D 0A 00 00 00 00 00 00
      d  !  \  n  "  )  ;  .  }  .  .  .  .  .  .  .
*/

char buffer[4096];
extern void exit();	/* SS. 25.Oct.87 */

main(argc,argv)
	int  argc;
	char *argv[]; {
	unsigned i,numin,tot,file;
	char *cfrom;

	if (argc < 2) {
		puts("Missing Filename\n");
		}

	tot=0;
	if ((file=open(argv[1],0)) == -1) {
		puts("Cannot Open ");
		puts(argv[1]);
		exit(1);
		}

/*	read and dump 4k at a time	*/

	do {
		numin=read(file,buffer,4096);
		if (numin == -1) {
			puts("Cannot Read ");
			puts(argv[1]);
			exit(1);
			}
		cfrom=0;
		while (cfrom < (char *)numin) {  /* SS.09.sep.87 cast added */

/*	print the offset in hex	*/

			ohw(cfrom+tot);
			putchar(' ');

/*	print 16 bytes in hex	*/

			for (i=0; i < 16; i++) {
				putchar(' ');
/* SS. 9.sep.87 added cast (int) to avoid errors 112 and 98 */
				ohb(buffer[(int)cfrom++]); 
				}
			cfrom-=16;
			printf("\n");

/*	print the bytes in ascii	*/

			printf( "    " );	/* offset */
			for (i=0; i < 16; i++) {
/* SS. 9.sep.87 added three casts (int) to avoid errors 112 and 98 */
			if(buffer[(int)cfrom] >= ' ' && buffer[(int)cfrom] < 0x7f)
				printf("  %c", buffer[(int)cfrom] );
	/* putchar((buffer[(int)cfrom] >= ' ' && buffer[(int)cfrom] < 0x7f) 
					 ? buffer[(int)cfrom]: '.');   */
			else
				printf("  .");
				cfrom++;
				}
			printf("\n\n");
			}
		tot+=numin;
		}
	while (numin == 4096);
	}

/*	print a word in hex	*/

ohw(wrd)
	unsigned wrd; {
	ohb(wrd>>8);
	ohb(wrd);
	}

/*	print a byte in hex	*/

ohb(byt)
	char byt; {
	onib(byt>>4);
	onib(byt);
	}

/*	print a nibble as a hex character	*/

onib(nib)
	char nib; {

	nib&=15;
	putchar((nib >= 10) ? nib-10+'A': nib+'0');
	}

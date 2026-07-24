/*
 *	grep, finds a string in an ascii file.
 *
 *		for MS-DOS and Computer Innovations C86
 *
 *		Copyright (c) 1983,84 Solution Systems
 *
 *			  May 1984
 * 		  Last modified April 1985 by Dan Teven.
 *
 * 		See below for more information.
 *			Modified for Kronos 19.dec.86 by S.Siibak
 *
 */


#include <stdio.h>
#include <ctype.h>
#include "chelper.h"


#define LMAX	512
#define PMAX	256
#define CHAR	1
#define BOL	2
#define EOL	3
#define ANY	4
#define CLASS	5
#define NCLASS	6
#define STAR	7
#define PLUS	8
#define MINUS	9
#define ALPHA	10
#define DIGIT	11
#define NALPHA	12
#define PUNCT	13
#define RANGE	14
#define ENDPAT	15
int	cflag;
int	fflag;
int	nflag;
int	vflag;
int	nfile;
int	rflag;
int	debug = 0;
char	*pp;
char	file_name[81];
char	lbuf[LMAX];
char	pbuf[PMAX];
FILE	*fopen();

/*	grep
*/
main(argc, argv)
char *argv[];
int argc;
{
	char		*p;
	register int	c, i;
	int		gotpattern;

	FILE		*f;


	cflag=fflag=nflag=vflag=nflag=rflag=0;
	fprintf(errmsg, "GREP: Global Regular Expression Parser %s\n", VERSION);
	fprintf(errmsg, "Copyright (c) 1985 Solution Systems\n");

	if(argc < 2) quit();
	if(argc==2 && (!strcmp(argv[1],"?"))) quit();

	nfile = argc-1;
	gotpattern = 0;
	for (i=1; i < argc; ++i)
		{
		p = argv[i];
		if (*p == '-')
			{
			++p;
			while (c = *p++)
				{
				switch(tolower(c))
					{
				case '?':
					help();
					break;
				case 'c':
					++cflag;
					break;
				case 'd':
					++debug;
					break;
				case 'f':
					++fflag;
					break;
				case 'n':
					++nflag;
					break;
				case 'v':
					++vflag;
					break;
				default:
					usage("Unknown flag");
					}
				}
			argv[i] = 0;
			--nfile;
			}
		else if (!gotpattern)
			{
			compile(p);
			argv[i] = 0;
			++gotpattern;
			--nfile;
			}
		}
	if (!gotpattern)
		usage("No pattern");
	if (nfile==0)
		grep(stdin, "");
	else
		{
		fflag = fflag ^ (nfile > 0);
		for (i=1; i < argc; ++i)
			{
			if (p = argv[i])
				{
				if ((f=fopen(p, "r")) == NULL)
					cant(p);
				else
					{
					grep(f, p);
					fclose(f);
					}
				}
			}
		}
}

file(s)
char *s;
{
	fprintf(errmsg,"File %s:\n", s);
}

cant(s)
char *s;
{
	fprintf(errmsg, "File not opened: %s\n", s);
}

help()

/*
 * Give good help
 */

{

	fprintf(errmsg,
	"\nform:\n\t\tgrep [options] regular_expression file_list [>output]");
	fprintf(errmsg,
	"\n\noptions:");
	fprintf(errmsg,
	"\n\t\t-c   just print count of matching lines");
	fprintf(errmsg,
	"\n\t\t-f   don't print file name");
	fprintf(errmsg,
	"\n\t\t-n   number matching lines");
	fprintf(errmsg,
	"\n\t\t-v   print non-matching lines");
	fprintf(errmsg,
	"\n\nin pattern:");
	fprintf(errmsg,
	"\n\t\t^  = beginning of line");
	fprintf(errmsg,
	"\n\t\t$  = end of line");
	fprintf(errmsg,
	"\n\t\t.  = any character except newline");
	fprintf(errmsg,
	"\n\t\t:a = any alphabetic character");
	fprintf(errmsg,
	"\n\t\t:d = any digit");
	fprintf(errmsg,
	"\n\t\t:n = any alphanumeric");
	fprintf(errmsg,
	"\n\t\t:  = any white space character");
	fprintf(errmsg,
	"\n\t\t*  = zero or more occurrences of previous character");
	fprintf(errmsg,
	"\n\t\t+  = one or more occurrences");
	fprintf(errmsg,
	"\n\t\t-  = previous character optional");
	fprintf(errmsg,
	"\n\t\t\\  = escape (match following metacharacter literally)");
	fprintf(errmsg,
	"\n\t\t[]   enclose character set (form: [abcde] or [a-e])\n");
}

quit()
{

	help();
	exit(1);
}

usage(s)
char *s;
{
	fprintf(errmsg, "? %s\n", s);
	help();
	exit(1);
}

compile(source)
char *source;		 /* Pattern to compile			 */

/*
 * Compile the pattern into global pbuf[]
 */

{
	register char		*s;	/* Source string pointer	*/
	register char		*lp;	/* Last pattern pointer 	*/
	register int		c;	/* Current character		*/
	int			o;	/* Temp 			*/
	char			*spp;	/* Save beginning of pattern	*/
	char		*cclass();	/* Compile class routine	*/
	s = source;
	if (debug)
		printf("Pattern = \"%s\"\n", s);
	pp = pbuf;
	while (c = *s++)
		{
		/*
						 * STAR, PLUS and MINUS are special.
						 */

		if (c == '*' || c == '+' || c == '-')
			{
			if (pp == pbuf || (o=pp[-1]) == BOL ||
				o == EOL || o == STAR ||
				o == PLUS || o == MINUS)
				badpat("Illegal occurrence op.", source, s);
			store(ENDPAT);
			store(ENDPAT);
			spp = pp;		/* Save pattern end	*/
			while (--pp > lp)	/* Move pattern down	*/
				*pp = pp[-1];	/* one byte		*/
			*pp =	((c == '*') ? STAR : (c == '-' ? MINUS : PLUS));
			pp = spp;		/* Restore pattern end	*/
			continue;
			}

		/*
				 *	 All the rest.
				 */

		lp = pp;			/*	Remember start	*/
		switch(c)
			{
		case '^':
			store(BOL);
			break;
		case '$':
			store(EOL);
			break;
		case '.':
			store(ANY);
			break;
		case '[':
			s = cclass(source, s);
			break;
		case ':':
			if (*s)
				{
				c = *s++;
				switch(tolower(c))
					{
				case 'a':
					store(ALPHA);
					break;
				case 'd':
					store(DIGIT);
					break;
				case 'n':
					store(NALPHA);
					break;
				case ' ':
					store(PUNCT);
					break;
				default:
					badpat("Unknown : type", source, s);
					}
				break;
				}
			else	badpat("No : type", source, s);
		case '\\':
			if (*s)
				c = *s++;
		default:
			store(CHAR);
			store(tolower(c));
			}
		}
	store(ENDPAT);
	store(0);				/*   Terminate string	  */
	if (debug)
		{
		for (lp = pbuf; lp < pp;)
			{
			if ((c = (*lp++ & 0377)) < ' ')
				printf("\\%o ", c);
			else	printf("%c ", c);
			}
		printf("\n");
		}
}

char *cclass(source, src)
char		*source;	/* Pattern start -- for error msg.	*/
char		*src;		/* Class start				*/

/*
 * Compile a class (within [])
 */

{
	register char	*s;		/* Source pointer		*/
	register char	*cp;		/* Pattern start		*/
	register int	c;		/* Current character		*/
	int		o;		/* Temp 			*/
	s = src;
	o = CLASS;
	if (*s == '^')
		{
		++s;
		o = NCLASS;
		}
	store(o);
	cp = pp;
	store(0);				/* Byte count		*/
	while ((c = *s++) && c!=']')
		{
		if (c == '\\')
			{		/* Store quoted char	*/
			if ((c = *s++) == '\0') /* Gotta get something	*/
				badpat("Class terminates badly", source, s);
			else	store(tolower(c));
			}
		else if (c == '-' &&
			(pp - cp) > 1 && *s != ']' && *s != '\0')
			{
			c = pp[-1];		/* Range start		*/
			pp[-1] = RANGE; 	/* Range signal 	*/
			store(c);		/* Re-store start	*/
			c = *s++;		/* Get end char and	*/
			store(tolower(c));	/* Store it		*/
			}
		else
			{
			store(tolower(c));	/* Store normal char	*/
			}
		}
	if (c != ']')
		badpat("Unterminated class", source, s);
	if ((c = (pp - cp)) >= 256)
		badpat("Class too large", source, s);
	if (c == 0)
		badpat("Empty class", source, s);
	*cp = c;
	return(s);
}

store(op)
{
	if (pp >= &pbuf[PMAX])
		abort("Pattern too complex\n");
	*pp++ = op;
}


badpat(message, source, stop)
char		*message;	/* Error message			*/
char		*source;	/* Pattern start			*/
char		*stop;		/* Pattern end				*/
{
	fprintf(errmsg, "GREP %s, pattern is\"%s\"\n", message, source);
	fprintf(errmsg, "GREP Stopped at byte %d, '%c'\n",
	stop-source, stop[-1]);
	abort("? GREP Bad pattern\n");
}

grep(fp, fn)
FILE		*fp;		/* File to process			*/
char		*fn;		/* File name (for -f option)		*/

/*
 * Scan the file for the pattern in pbuf[]
 */

{
	register int lno, count, m;
	lno = 0;
	count = 0;
	while (fgets(lbuf, LMAX, fp))
		{
		++lno;
		m = match();
		if ((m && !vflag) || (!m && vflag))
			{
			++count;
			if (!cflag)
				{
				if (fflag && fn)
					{
					file(fn);
					fn = 0;
					}
				if (nflag)
					printf("%d\t", lno);
				printf("%s", lbuf);
				}
			}
		}
	if (cflag)
		{
		if (fflag && fn)
			file(fn);
		printf("%d\n", count);
		}
}

match()

/*
 * Match the current line (in lbuf[]), return 1 if it does.
 */

{
	register char	*l;		/* Line pointer 		*/
	char *pmatch();
	for (l = lbuf; *l; l++)
		{
		if (pmatch(l, pbuf))
			return(1);
		}
	return(0);
}

char *pmatch(line, pattern)
char		*line;		/* (partial) line to match		*/
char		*pattern;	/* (partial) pattern to match		*/
{
	register char	*l;		/* Current line pointer 	*/
	register char	*p;		/* Current pattern pointer	*/
	register char	c;		/* Current character		*/
	char		*e;		/* End for STAR and PLUS match	*/
	int		op;		/* Pattern operation		*/
	int		n;		/* Class counter		*/
	char		*are;		/* Start of STAR match		*/
	l = line;
	if (debug > 1)
		printf("pmatch(\"%s\")\n", line);
	p = pattern;
	while ((op = *p++) != ENDPAT)
		{
		if (debug > 1)
			printf("byte[%d] = 0%o, '%c', op = 0%o\n",
			l-line, *l, *l, op);
		switch(op)
			{
		case CHAR:
			if (tolower(*l++) != *p++)
				return(0);
			break;
		case BOL:
			if (l != lbuf)
				return(0);
			break;
		case EOL:
			if (*l != '\0')
				return(0);
			break;
		case ANY:
			if (*l++ == '\0')
				return(0);
			break;
		case DIGIT:
			if ((c = *l++) < '0' || (c > '9'))
				return(0);
			break;
		case ALPHA:
			c = tolower(*l++);
			if (c < 'a' || c > 'z')
				return(0);
			break;
		case NALPHA:
			c = tolower(*l++);
			if (c >= 'a' && c <= 'z')
				break;
			else if (c < '0' || c > '9')
				return(0);
			break;
		case PUNCT:
			c = *l++;
			if (c == 0 || c > ' ')
				return(0);
			break;
		case CLASS:
		case NCLASS:
			c = tolower(*l++);
			n = *p++ & 0377;
			do
				{
				if (*p == RANGE)
					{
					p += 3;
					n -= 2;
					if (c >= p[-2] && c <= p[-1])
						break;
					}
				else if (c == *p++)
					break;
				}
			while (--n > 1);
			if ((op == CLASS) == (n <= 1))
				return(0);
			if (op == CLASS)
				p += n - 2;
			break;
		case MINUS:
			e = pmatch(l, p);	/* Look for a match	*/
			while (*p++ != ENDPAT); /* Skip over pattern	*/
			if (e)			/* Got a match? 	*/
				l = e;		/* Yes, update string	*/
			break;			/* Always succeeds	*/
		case PLUS:			/* One or more ...	*/
			if ((l = pmatch(l, p)) == 0)
				return(0);	/* Gotta have a match	*/
		case STAR:			/* Zero or more ...	*/
			are = l;		/* Remember line start	*/
			while (*l && (e = pmatch(l, p)))
				l = e;		/* Get longest match	*/
			while (*p++ != ENDPAT); /* Skip over pattern	*/
			while (l >= are)
				{	/* Try to match rest	*/
				if (e = pmatch(l, p))
					return(e);
				--l;		/* Nope, try earlier	*/
				}
			return(0);		/* Nothing else worked	*/
		default:
			printf("Bad op code %d\n", op);
			abort("Cannot happen -- match\n");
			}
		}
	return(l);
}


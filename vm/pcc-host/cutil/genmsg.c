
/*	G e n e r a t e  M e s s a g e s  F i l e		*/
/*	=========================================		*/
/*								*/
/*  MK  09.Oct.86  (from CP/M)					*/

#include <stdio.h>
#include <ctype.h>

#define	issp(x)	((x) == ' ' || (x) == '\t')

FILE	* infile,
	* fopen();
int	outfile;

char	* alloc(),
	* malloc();

int	cmp();

int	debug = 0,
	msgno = 0,
	msgnum = 200,
	c_newl = 0,
	c_cont = 0;

typedef struct {
	int	m_nr;		/* message's number		*/
	int	m_len;		/* message's length		*/
	int	m_offset;	/* message's offset in file	*/
	char	*m_txt;		/* message's text		*/
	}
		MSG;

MSG	* msg;

char	ifname[100] = "";
char	ofname[100] = "";

main (argc, argv)
int	argc;
char	* argv[];
{int	temp = 0;

	while (++argv, --argc) {
	    if (**argv == '-') {
		switch (*(*argv+1)) {
		    case 'd':
			debug ++;
			break;
		    case 'i':
			if (*(*argv+2))
			    strcpy (ifname, *argv+2);
			else
			    strcpy (ifname, *++argv), --argc;
			break;
		    case 'o':
			if (*(*argv+2))
			    strcpy (ofname, *argv+2);
			else
			    strcpy (ofname, *++argv), --argc;
			break;
		    case 'n':
			if (*(*argv+2))
			    sscanf (*argv+2, "%d", &temp);
			else
			    sscanf (*++argv, "%d", &temp), --argc;
			if (temp > msgnum)
			    msgnum = temp;
			break;
		    default:
			fprintf(stderr, "Invalid option: %s\n", *argv);
			break;
		    }
		continue;
		}
	    else fprintf(stderr, "Invalid parameter: %s\n", *argv);
	    }
	if (!(*ofname && *ifname)) {
	    fprintf(stderr, 
		"Usage: genmsg -i<infile> -o<outfile> [-d] [-n<num>]\n");
	    exit (1);
	    }
	if (!strcmp(ifname, ofname)) {
	    fprintf(stderr, "Output file must be different\n");
	    exit (1);
	    }
	if (!(infile = fopen(ifname, "r"))) {
	    fprintf(stderr, "Unable to open %s for input\n", ifname);
	    exit (1);
	    }
	if (!(outfile = creat(ofname, 0666)) == -1) {
	    fprintf(stderr, "Unable to open %s for output\n", ofname);
	    exit (1);
	    }
	process();
	return;
}

process()

{char	buf [640], *s, *p;
 int	i, n;
 MSG	*m, *end;
 char	c;
 int	offset;

	if (debug) {
	    fprintf(stderr, "\n** Creating %s from %s\n", ofname, ifname);
	    fprintf(stderr, "**Max. no of messages: %d\n", msgnum);
	    }
	msg = (MSG *) alloc (msgnum * sizeof (MSG));
	for (i = 0; i < msgnum; i ++)
	    msg[i].m_nr = 0;
	getline (buf);
	while (!feof(infile)) {
	    m = & msg[msgno++];
	    if (msgno > msgnum)
		error ("Too many messages: restart genmsg\n", 0);
	    if (!sscanf (buf, "%d", &(m->m_nr)))
		error ("No message number: line %d\n", msgno);
	    s = & buf[0];
	    buf[strlen(s)-1] = '\0';
	    while (isdigit(*s++));
	    while (issp(*s)) s++;
	    p = m->m_txt = alloc(m->m_len = strlen(s)+1);
	    while (c = *p++ = *s++) {
		if (c == '\\' && *s == 'n') {
		    c_newl ++;
		    *(p-1) = '\n';
		    m->m_len --;
		    s ++;
		    }
		}
	    if (debug > 3)
		fprintf(stderr, "(%d) %d:%s\n", m->m_len, m->m_nr, 
								m->m_txt);
	    getline (buf);
	    }
	qsort ((char *)msg, msgno, sizeof (MSG), cmp);
	end = &msg[msgno];
	offset = (sizeof(MSG)-sizeof(char *)) * msgno + sizeof (int);
	for (m = msg; m < end; m ++) {
	    m->m_offset = offset;
	    offset += m->m_len;
	    }
	write (outfile, & msgno, sizeof(int));
	for (m = msg; m < end; m ++)
	    write (outfile, m, sizeof(MSG) - sizeof(char *));
	for (m = msg; m < end; m ++)
	    write (outfile, m->m_txt, m->m_len);
	fclose (infile);
	close (outfile);
	if (debug > 1) {
	    fprintf(stderr, "Newlines in messages : %d\n", c_newl);
	    fprintf(stderr, "Continued on next line : %d\n", c_cont);
	    }
	fprintf(stderr, "\n  %d  messages\n", msgno);
	fprintf(stderr, "%s : %d bytes\n", ofname, msg[msgno-1].m_offset +
					msg[msgno-1].m_len);
}

char	* alloc(n)
{char	* p;

	if ((p = malloc(n)) == (char *) 0) {
	    fprintf(stderr, "Malloc failed\n");
	    exit (1);
	    }
	return (p);
}

int	cmp (a, b)
MSG	*a, *b;
{
	return (a->m_nr - b->m_nr);
}

int	error (s, a)
char	* s;
int	a;
{
	fprintf(stderr, s, a);
	exit (1);
}

int	getline (buf)
char	* buf;
{char	buf1[320], * fgets();

	do { /* skip empty lines */
		if (!fgets (buf, 320, infile)) return;
		}
	while (strlen(buf) < 2);

	while (*(buf+strlen(buf)-2) == '\\') {
	    c_cont ++;
	    *(buf+strlen(buf)-2) = '\0';
	    fgets (buf1, 320, infile);
	    strcat (buf, buf1);
	    }
}


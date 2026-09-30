/*	@(#)stdio.h	2.7	*/
#ifndef _NFILE
#define _NFILE	20

#define BUFSIZ	512

/* buffer size for multi-character output to unbuffered files */
#define _SBFSIZ 8

typedef struct {
	int	_cnt;
	unsigned char	*_ptr;
	unsigned char	*_base;
	char	_flag;
	char	_file;
} FILE;

/*
 * _IOLBF means that a file's output will be buffered line by line
 * In addition to being flags, _IONBF, _IOLBF and _IOFBF are possible
 * values for "type" in setvbuf.
 */
#define _IOFBF		0000
#define _IOREAD		0001
#define _IOWRT		0002
#define _IONBF		0004
#define _IOMYBUF	0010
#define _IOEOF		0020
#define _IOERR		0040
#define _IOLBF		0100
#define _IORW		0200

#ifndef NULL
#define NULL		0
#endif
#ifndef EOF
#define EOF		(-1)
#endif

#define stdin		(&_iob[0])
#define stdout		(&_iob[1])
#define stderr		(&_iob[2])

#define _bufend(p)	_bufendtab[(p)->_file]
#define _bufsiz(p)	(_bufend(p) - (p)->_base)

#ifndef lint
#define getc(p)		(--(p)->_cnt < 0 ? _filbuf(p) : (int) *(p)->_ptr++)
#define putc(x, p)	(--(p)->_cnt < 0 ? \
			_flsbuf((unsigned char) (x), (p)) : \
			(int) (*(p)->_ptr++ = (unsigned char) (x)))
#define getchar()	getc(stdin)
#define putchar(x)	putc((x), stdout)
#define clearerr(p)	((void) ((p)->_flag &= ~(_IOERR | _IOEOF)))
#define feof(p)		((p)->_flag & _IOEOF)
#define ferror(p)	((p)->_flag & _IOERR)
#define fileno(p)	(p)->_file
#endif

extern FILE	_iob[_NFILE];
extern FILE	*fopen(), *fdopen(), *freopen(), *tmpfile();
extern long	ftell(), tell(), lseek(), atol(), strtol();
extern double	modf(), frexp(), ldexp(), strtod(), atof();

extern void	exit(), _exit(), free(), perror(), rewind(), setbuf(), poke(),
		disable_intr(), enable_intr(), qsort(), _assert(), srand();

extern void 	longjmp();

extern char	*ctermid(), *fgets(), *gets(), *tempnam(), *tmpnam(), *mktemp(),
		*malloc(), *calloc(), *realloc(), *sbrk(), *ecvt(), *fcvt(),
		*bsearch(), *lsearch(), *getenv(), *strcpy(), *strncpy(),
		*strcat(), *strncat(), *strchr(), *strrchr(), 
		*strpbrk(), *strtok(), *memccpy(), *memchr(), 
		*memcpy(), *memset();

extern unsigned char *_bufendtab[];

#define L_ctermid	9

#if unix
#define P_tmpdir	"/usr/tmp/"
#define L_tmpnam	(sizeof(P_tmpdir) + 15)
#endif
#if excII
/* host-port: no TMPDIR/no /tmp on Excelsior -> use the cwd (writable) so
   tmpfile()/tmpnam() create scratch files in the working directory. */
#define P_tmpdir	""
#define L_tmpnam	50
#endif
#endif

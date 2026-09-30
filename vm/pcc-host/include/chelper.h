/* CHELPER.H */

/*
   definition of where error messages are displayed 
   define one or the other
*/

#define errmsg stderr 
/* #define errmsg stdout */

#define VERSION "1.3" 


#define bits	ushort
#define bool	  int
#define metachar short
#define tbool	 char
#define ushort	unsigned
/* #define void	  int   SS. 28.nov.86 */
#define FAIL	    1
#define FOREVER for (;;)
#define NO	    0
#define YES	    1
#define TRUE        1
#define FALSE       0
#define ERROR      -1
#define OK          0
#define FF       0x0c
#define NL	 '\n'
#define BELL     0x07
/* #define CPMEOF   0x1A  SS. 28.nov.86 */
#define EOS       '\0'		/* STANDARD END OF STRING */
#define ASCIIMASK 0x7f
#define CTRLMASK  0x40
#define PRINTMASK 0x1f
#define SECSIZ    1024
/* #define BUFSIZ    1030  SS. */
#define MAXLINE    134
#ifdef CPM
#define KEY_STAT  0X0B   /* BDOS FUNCTION 11 : CHECK KEYBOARD STATUS*/
#define KEY_IN    0X01   /* BDOS FCN 1 : GET A CHARACTER */
#endif
#define unless(e)       if(!(e))
#define getln(s, n)	((fgets(s, n, stdin) == NULL) ? EOF : strlen(s))
#define ABS(x)		(((x) < 0) ? -(x) : (x))
#define MAX(x, y)	(((x) < (y)) ? (y) : (x))
#define MIN(x, y)	(((x) < (y)) ? (x) : (y))




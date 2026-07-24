/* MK 31.10.86 AUTOINIT 96 -> +2*32 for "swap" -> 160 */
/* SS. 16.Mar.87 makecc() commented out from scan.c */
/* SS. 23.Oct.87 SWAPINIT removed */
/* SS. 19.Nov.87 restored LLW4 for saving # of args(0) in case of switching processes */
/* # define makecc(val,i)  lastcon = (lastcon<<8)|((val<<24)>>24);  */

# define  ARGINIT 32 
# define  AUTOINIT (96+32)

/* SZCHAR M.K. 25.03.86 */
# define  SZCHAR 32
# define  SZINT 32
# define  SZFLOAT 32
# define  SZDOUBLE 32
# define  SZLONG 32
# define  SZSHORT 32
# define  SZPOINT 32

/* all alignments on word boundary, M.K. 25.03.86 */
# define ALCHAR 32
# define ALINT 32
# define ALFLOAT 32
# define ALDOUBLE 32
# define ALLONG 32
# define ALSHORT 32
# define ALPOINT 32
# define ALSTRUCT 32
# define ALSTACK 32 

/* size in which constants are converted */
/* should be long if feasable */

# define CONSZ long
# define CONFMT "%ld"
# define FCONFMT "%.18e" 

/* size in which offsets are kept
/* should be large enough to cover address space in bits
*/

# define OFFSZ long

 /* character set macro */

# define  CCTRANS(x) x

 /* register cookie for stack pointer */

# define STKREG 13
# define ARGREG 12

 /* maximum and minimum register variables */

# define MAXRVAR 11
# define MINRVAR 6

 /* various standard pieces of code are used */
/* M.K. L->M */
# define LABFMT "M%d"

/* SS. Kronos stack grows positively ,don't care ... */
/* show stack grows negatively */
#undef  BACKAUTO
#define BACKTEMP

/* show field hardware support on VAX */
/* SS. no hardware support on Kronos  */
#undef FIELDOPS

/* bytes are numbered from right to left */
#define RTOLBYTES

# define ENUMSIZE(high,low) INT
# define BUFSTDERR

/* MK 15.Oct.86
  # define FIXDEF(p) outstab(p)
  # define FIXARG(p) fixarg(p)
*/

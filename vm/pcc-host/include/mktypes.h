
/*
 *	MK	definitions
 */

/*	TABLE	SIZES		*/

#define	EXTVAR	500	/* external variables		*/
#define	EXTFUN	500	/* external functions		*/
#define JUMPLAB 500	/* jump labels in one function	*/
#define	DATLAB	500	/* .dat & .equ labels in module	*/	

#define PRTABSIZE 256
/* Note: only 128 is needed for Kronos 2 */

#define	MAXIDENT 15	/* no. of significant symbols in names	*/

#define	HDSIZE	15	/* object file header size */
#define LHDSIZE 16	/* load file header size   */

#define	OBJ2_VERSION	0x4321		/* object file version for 2   */
#define OBJ25_VERSION   0x5432		/* object file version for 2.5 */
#define COD2_VERSION 	0x100   	/* code file version (KRONOS 2) */
#define COD25_VERSION 	0x101   	/* code file version (KRONOS 2.5) */
#define	LIB_VERSION	0x101010	/* library file version */

#define MOD0    0x30646F4D

#define KRONOS_BIT	0x01000000	/* value to be added to NS32000 */
					/* FP numbers to get Kronos FP  */

#define MAGICTIME	504885580	/* value to be subtracted from	*/
					/* Unix time to get Kronos time	*/
#define	SUF_OBJ	".o"
#define SUF_LD	".cod"
#define SUF_LIB	".lib"

#define	MAIN	"main"

/* KRONOS has no sign extension  => ... */

#if	unix
#define	BYTE(x)		((x) & 0377)
#else
#define	BYTE(x)		(x)
#endif

/*	symbol table manipulation flags	*/

#define	FIND	00
#define	INSERT	01
#define	STAT	02
#define VOID	04
#define USED	0100

struct	stab {
		char	name [MAXIDENT+1];
		int	flag;
		};

typedef	struct	stab	STAB;

/*	data label flags	*/

#define	D_DAT	010
#define	D_EQU	020
#define	D_NVAL	040

struct	dlab {
		int	letter;
		int	num;
		int	flag;
		int	val;
		};

typedef struct	dlab	DLAB;

struct	jlab {
		int	letter;
		int	num;
		int	pc;
		};

typedef struct	jlab	JLAB;

struct	ftab {
		int	length;	  /* function length in bytes		*/
		int	labno;	  /* number of jump labels in function	*/
		};

typedef	struct	ftab	FTAB;


/*	location flags		*/

#define	L_FUN	01
#define	L_INIT	02

#define	I_BLK	4096
#define	I_EXTRA	512



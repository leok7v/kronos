/*	values.h		*/
/* Revision log:
 *  17.Mar.87 SS  double is same as float, macros changed accordingly
 *  22.Jun.87 SS  MAXFLOAT and MINFLOAT as PDP-11
*/
#ifndef	_VALUES
#define _VALUES		1
#define BITSPERBYTE	8
#define BITS(type)	(BITSPERBYTE * (int)sizeof(type))

/* short, regular and long ints with only the high-order bit turned on */
#define HIBITS	((short)((unsigned)1 << BITS(short) - 1))
#define HIBITI	((unsigned)1 << BITS(int) - 1)
#define HIBITL	((unsigned)1L << BITS(long) - 1)

/* largest short, regular and long int */
#define MAXSHORT	((short)~HIBITS)
#define MAXINT	(~HIBITI)
#define MAXLONG	(~HIBITL)

/* various values that describe the binary floating-point representation
 * _EXPBASE	- the exponent base
 * DMAXEXP 	- the maximum exponent of a double (as returned by frexp())
 * FMAXEXP 	- the maximum exponent of a float  (as returned by frexp())
 * DMINEXP 	- the minimum exponent of a double (as returned by frexp())
 * FMINEXP 	- the minimum exponent of a float  (as returned by frexp())
 * MAXDOUBLE	- the largest double
			((_EXPBASE ** DMAXEXP) * (1 - (_EXPBASE ** -DSIGNIF)))
 * MAXFLOAT	- the largest float
			((_EXPBASE ** FMAXEXP) * (1 - (_EXPBASE ** -FSIGNIF)))
 * MINDOUBLE	- the smallest double (_EXPBASE ** (DMINEXP - 1))
 * MINFLOAT	- the smallest float (_EXPBASE ** (FMINEXP - 1))
 * DSIGNIF	- the number of significant bits in a double
 * FSIGNIF	- the number of significant bits in a float
 * DMAXPOWTWO	- the largest power of two exactly representable as a double
 * FMAXPOWTWO	- the largest power of two exactly representable as a float
 * _IEEE	- 1 if IEEE standard representation is used
 * _DEXPLEN	- the number of bits for the exponent of a double
 * _FEXPLEN	- the number of bits for the exponent of a float
 * _HIDDENBIT	- 1 if high-significance bit of mantissa is implicit
 * LN_MAXDOUBLE	- the natural log of the largest double  -- log(MAXDOUBLE)
 * LN_MINDOUBLE	- the natural log of the smallest double -- log(MINDOUBLE)
 */

/*
#define MAXFLOAT	((float)3.4028234663852892e+38)
#define MINFLOAT	((float)1.1754943508222874e-38)
*/

#if pdp11 || vax || kronos
/* #define MAXDOUBLE	1.701411834604692293e+38	*/
#define MAXFLOAT	((float)1.701411733192644299e+38)
/* The following is kludged because the PDP-11 compilers botch the simple form.
   The kludge causes the constant to be computed at run-time on the PDP-11,
   even though it is still "folded" at compile-time on the VAX. */

#define MINFLOAT	((float)0.01 * 2.938735877055718770e-37)
#endif

#define MAXDOUBLE	MAXFLOAT
#define MINDOUBLE	MINFLOAT

#define	_IEEE		0
#define _DEXPLEN	_FEXPLEN
#define _HIDDENBIT	1
#define DMINEXP		FMINEXP
#define FMINEXP		(-FMAXEXP)
#define DSIGNIF		FSIGNIF
#define DMAXPOWTWO	FMAXPOWTWO
#define DMAXEXP		FMAXEXP

#define _LENBASE	1
#define _EXPBASE	((unsigned)1 << _LENBASE)
#define _FEXPLEN	8

#define FSIGNIF	(BITS(float)  - _FEXPLEN + _HIDDENBIT - 1)
#define FMAXPOWTWO	((float)((unsigned)1L << FSIGNIF - 1))
#define FMAXEXP	(((unsigned)1 << _FEXPLEN - 1) - 1 + _IEEE)
#define LN_MAXDOUBLE	(M_LN2 * DMAXEXP)
#define LN_MINDOUBLE	(M_LN2 * (DMINEXP - 1))
#define H_PREC	(DSIGNIF % 2 ? ((unsigned)1L << DSIGNIF/2) * M_SQRT2 : (unsigned)1L << DSIGNIF/2)
#define X_EPS	(1.0/H_PREC)
#define X_PLOSS	((double)(long)(M_PI * H_PREC))
#define X_TLOSS	(M_PI * DMAXPOWTWO)
#define M_LN2	0.69314718055994530942
#define M_PI	3.14159265358979323846
#define M_SQRT2	1.41421356237309504880
#define MAXBEXP	DMAXEXP /* for backward compatibility */
#define MINBEXP	DMINEXP /* for backward compatibility */
#define MAXPOWTWO	DMAXPOWTWO /* for backward compatibility */
#endif


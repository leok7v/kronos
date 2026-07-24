/***********************************************
*                                              *
*      C language math library for             *
*         SOVIET microcomputer KRONOS          *
*                                              *
*               Version 1.0                    *
*                                 	       * 
* Created: 14.sep.87 by N.Saard	               *
*					       *
***********************************************/
/*
 * Revision log:
 * 15.Sep.87 SS unneeded "includes" removed
 * 07.Jan.88 SS matherr() is now a separate module
 */

/*	asin.c	*/
/*
 *	C program for double-precision asin/acos.
 *	Returns EDOM error and value 0 if |argument| > 1.
 *	Algorithm and coefficients from Cody and Waite (1980).
 *	Calls sqrt if |argument| > 0.5.
 */

#include <math.h>
#include <values.h>
#include <errno.h>

double
asin(x)
double x;
{
	extern double asin_acos();

	return (asin_acos(x, 0));
}

static double
asin_acos(x, acosflag)
register double x;
int acosflag;
{
	register double y;
	register int neg = 0, large = 0;
	struct exception exc;
	
	exc.arg1 = x;
	if (x < 0) {
		x = -x;
		neg++;
	}
	if (x > 1) {
		exc.type = DOMAIN;
		exc.name = acosflag ? "acos" : "asin";
		exc.retval = 0.0;
		if (!matherr(&exc)) {
			(void) write(2, exc.name, 4);
			(void) write(2, ": DOMAIN error\n", 15);
			errno = EDOM;
		}
		return (exc.retval);
	}
	if (x > X_EPS) { /* skip for efficiency and to prevent underflow */
		static double p[] = {
			-0.69674573447350646411e0,
			 0.10152522233806463645e2,
			-0.39688862997504877339e2,
			 0.57208227877891731407e2,
			-0.27368494524164255994e2,
		}, q[] = {
			 1.0,
			-0.23823859153670238830e2,
			 0.15095270841030604719e3,
			-0.38186303361750149284e3,
			 0.41714430248260412556e3,
			-0.16421096714498560795e3,
		};

		if (x <= 0.5)
			y = x * x;
		else {
			large++;
			y = 0.5 - 0.5 * x;
			x = -sqrt(y);
			x += x;
		}
		x += x * y * _POLY4(y, p)/_POLY5(y, q);
	}
	if (acosflag) {
		if (!neg)
			x = -x;
		return (!large ? M_PI_2 + x : neg ? M_PI + x : x);
	}
	if (large)
		x += M_PI_2;
	return (neg ? -x : x);
}

double
acos(x)
double x;
{
	return (asin_acos(x, 1));
}

/*	atan.c		*/
/*
 *	atan returns the arctangent of its double-precision argument,
 *	in the range [-pi/2, pi/2].
 *	There are no error returns.
 *
 *	atan2(y, x) returns the arctangent of y/x,
 *	in the range [-pi, pi].
 *	atan2 discovers what quadrant the angle is in and calls atan.
 *	atan2 returns EDOM error and value 0 if both arguments are zero.
 *
 *	Coefficients are #5077 from Hart & Cheney (19.56D).
 */

#define SQ2_M_1	0.41421356237309504880
#define SQ2_P_1	2.41421356237309504880
#define NEG	1
#define INV	2
#define MID	4

double
atan(x)
register double x;
{
	register int type = 0;

	if (x < 0) {
		x = -x;
		type = NEG;
	}
	if (x > SQ2_P_1) {
		x = 1/x;
		type |= INV;
	} else if (x > SQ2_M_1) {
		x = (x - 1)/(x + 1);
		type |= MID;
	}
	if (x < -X_EPS || x > X_EPS) {
		/* skip for efficiency and to prevent underflow */
		static double p[] = {
			0.161536412982230228262e2,
			0.26842548195503973794141e3,
			0.11530293515404850115428136e4,
			0.178040631643319697105464587e4,
			0.89678597403663861959987488e3,
		}, q[] = {
			1.0,
			0.5895697050844462222791e2,
			0.536265374031215315104235e3,
			0.16667838148816337184521798e4,
			0.207933497444540981287275926e4,
			0.89678597403663861962481162e3,
		};
		register double xsq = x * x;

		x *= _POLY4(xsq, p)/_POLY5(xsq, q);
	}
	if (type & INV)
		x = M_PI_2 - x;
	else if (type & MID)
		x += M_PI_4;
	return (type & NEG ? -x : x);
}

double
atan2(y, x)
register double y, x;
{
	register int neg_y = 0;
	struct exception exc;
	double at;

	exc.arg1 = y;
	if (!x && !y) {
		exc.type = DOMAIN;
		exc.name = "atan2";
		exc.arg2 = x;
		exc.retval = 0.0;
		if (!matherr(&exc)) {
			(void) write(2, "atan2: DOMAIN error\n", 20);
			errno = EDOM;
		}
		return (exc.retval);
	}
	/*
	 * The next lines determine if |x| is negligible compared to |y|,
	 * without dividing, and without adding values of the same sign.
	 */
	if (exc.arg1 < 0) {
		exc.arg1 = -exc.arg1;
		neg_y++;
	}
	if (exc.arg1 - _ABS(x) == exc.arg1) 
		return (neg_y ? -M_PI_2 : M_PI_2);
	/*
	 * The next line assumes that if y/x underflows the result
	 * is zero with no error indication, so it's safe to divide.
	 */
	at = atan(y/x);
	if (x > 0)		     /* return arctangent directly */
		return (at);
	if (neg_y)		    /* x < 0, adjust arctangent for */
		return (at - M_PI);        /* correct quadrant */
	else
		return (at + M_PI);
}
/*	erf.c	*/
/*
 *	erf returns the error function of its double-precision argument.
 *	erfc(x) returns 1 - erf(x).
 *
 *	erf(x) is defined by
 *	${2 over sqrt pi} int from 0 to x e sup {- t sup 2} dt$.
 *
 *	The entry for erfc is provided because of the
 *	extreme loss of relative accuracy if erf(x) is
 *	called for large x and the result subtracted
 *	from 1 (e.g. for x = 5, 12 places are lost).
 *
 *	There are no error returns.
 *
 *	Calls exp for |x| > 0.5.
 *
 *	Coefficients for large x are #5667 from Hart & Cheney (18.72D).
 */

/* approx sqrt(log(MAXDOUBLE)) */
#if u3b
#define MAXVAL	27.23
#else
#define MAXVAL	 9.27
#endif
#define DPOLYD(y, p, q)	for (n = d = 0, i = sizeof(p)/sizeof(p[0]); --i >= 0; ) \
				{ n = n * y + p[i]; d = d * y + q[i]; }

static double p1[] = {
	0.804373630960840172832162e5,
	0.740407142710151470082064e4,
	0.301782788536507577809226e4,
	0.380140318123903008244444e2,
	0.143383842191748205576712e2,
	-.288805137207594084924010e0,
	0.007547728033418631287834e0,
}, q1[]  = {
	0.804373630960840172826266e5,
	0.342165257924628539769006e5,
	0.637960017324428279487120e4,
	0.658070155459240506326937e3,
	0.380190713951939403753468e2,
	1.0,
	0.0,
};
static double p2[]  = {
	0.18263348842295112592168999e4,
	0.28980293292167655611275846e4,
	0.2320439590251635247384768711e4,
	0.1143262070703886173606073338e4,
	0.3685196154710010637133875746e3,
	0.7708161730368428609781633646e2,
	0.9675807882987265400604202961e1,
	0.5641877825507397413087057563e0,
	0.0,
}, q2[]  = {
	0.18263348842295112595576438e4,
	0.495882756472114071495438422e4,
	0.60895424232724435504633068e4,
	0.4429612803883682726711528526e4,
	0.2094384367789539593790281779e4,
	0.6617361207107653469211984771e3,
	0.1371255960500622202878443578e3,
	0.1714980943627607849376131193e2,
	1.0,
};

double
erf(x)
register double x;
{
	int neg = 0;

	if (x < 0) {
		x = -x;
		neg++;
	}
	if (x > 0.5)
		x = 1 - erfc(x);
	else {
		register double n, d, xsq = x * x;
		register int i;

		DPOLYD(xsq, p1, q1);
		x *= M_2_SQRTPI * n/d;
	}
	return (neg ? -x : x);
}

double
erfc(x)
register double x;
{
	register double n, d;
	register int i;

	if (x < 0.5)
		return (1 - erf(x));
	if (x >= MAXVAL) /* exp(-x * x) sure to underflow */
		return (0.0);
	DPOLYD(x, p2, q2);
	return (exp(-x * x) * n/d);
}
/*	fabs.c	*/
/*
 *	fabs returns the absolute value of its double-precision argument.
 */

double
fabs(x)
register double x;
{
	return (x < 0 ? -x : x);
}
/*	floor.c		*/
/*
 *	floor(x) returns the largest integer (as a double-precision number)
 *	not greater than x.
 *	ceil(x) returns the smallest integer not less than x.
 */

extern double modf();

double
floor(x) 
double x;
{
	double y; /* can't be in register because of modf() below */

	return (modf(x, &y) < 0 ? y - 1 : y);
}

double
ceil(x)
double x;
{
	double y; /* can't be in register because of modf() below */

	return (modf(x, &y) > 0 ? y + 1 : y);
}
/*	fmod.c		*/
/*
 *	fmod(x, y) returns the remainder of x on division by y,
 *	with the same sign as x,
 *	except that if |y| << |x|, it returns 0.
 */

double
fmod(x, y)
register double x, y;
{
	double d; /* can't be in register because of modf() below */

	/*
	 * The next lines determine if |y| is negligible compared to |x|,
	 * without dividing, and without adding values of the same sign.
	 */
	d = _ABS(x);
	if (d - _ABS(y) == d)
		return (0.0);
#ifndef	pdp11	/* pdp11 "cc" can't handle cast of double to void */
	(void)
#endif
	modf(x/y, &d); /* now it's safe to divide without overflow */
	return (x - d * y);
}
/*	gamma.c		*/
/*
 *	gamma returns the log of the absolute value of the gamma function
 *	of its double-precision argument.
 *	The sign of the gamma function is returned in the
 *	external integer signgam.
 *	Returns EDOM error and value HUGE if argument is non-negative integer.
 *	Returns ERANGE error and value HUGE if the correct value would overflow.
 *
 *	The coefficients for expansion around zero
 *	are #5243 from Hart & Cheney; for expansion
 *	around infinity they are #5404.
 *
 *	Calls log and sin.
 */

#define X_MAX	(3.0 * H_PREC)
#define GOOBIE	0.9189385332046727417803297

int signgam;

double
gamma(x)
register double x;
{
	extern double pos_gamma();
	struct exception exc;

	exc.type = 0;
	exc.name = "gamma";
	exc.arg1 = x;
	exc.retval = HUGE;
	signgam = 1;
	if (x > 0)
		x = pos_gamma(x, &exc);
	else {
		static double pi = M_PI;
		double temp; /* can't be in register because of modf() below */

		if (!modf(x = -x, &temp)) { /* SING if x is negative integer */
			exc.type = SING;
			if (!matherr(&exc)) {
				(void) write(2, "gamma: SING error\n", 18);
				errno = EDOM;
			}
			return (exc.retval);
		}
		if (x >= X_MAX)
			exc.type = OVERFLOW;
		else {
			if ((temp = sin(pi * x)) < 0)
				temp = -temp;
			else
				signgam = -1;
			return (-(log(x * temp/pi) + pos_gamma(x, &exc)));
		}
	}
	if (exc.type != OVERFLOW)
		return (x);
	if (!matherr(&exc))
		errno = ERANGE;
	return (exc.retval);
}

static double
pos_gamma(x, excp)
register double x;
struct exception *excp;
{
	static double p2_[] = {
		-0.67449507245925289918e1,
		-0.50108693752970953015e2,
		-0.43933044406002567613e3,
		-0.20085274013072791214e4,
		-0.87627102978521489560e4,
		-0.20886861789269887364e5,
		-0.42353689509744089647e5,
	}, q2_[] = {
		 1.0,
		-0.23081551524580124562e2,
		 0.18949823415702801641e3,
		-0.49902852662143904834e3,
		-0.15286072737795220248e4,
		 0.99403074150827709015e4,
		-0.29803853309256649932e4,
		-0.42353689509744090010e5,
	};
	register double y, z;

	if (x > 8) { /* asymptotic approximation */
		static double p[] = {
			-0.1633436431e-2,
			 0.83645878922e-3,
			-0.5951896861197e-3,
			 0.793650576493454e-3,
			-0.277777777735865004e-2,
			 0.83333333333333101837e-1,
		};
	
		if (x >= MAXDOUBLE/LN_MAXDOUBLE) {
			excp->type = OVERFLOW;
			return (excp->retval);
		}
		z = (x - 0.5) * log(x) - x + GOOBIE;
		if (x > X_MAX)
			return (z);
		x = 1/x;
		y = x * x;
		return (z + x * _POLY5(y, p));
	}
	y = 1;
	if (x < y)
		y /= (x * (y + x));
	else if (x < 2) {
		y /= x;
		x -= 1;
	} else {
		for ( ; x >= 3; y *= x)
			x -= 1;
		x -= 2;
	}
	return (log(y * _POLY6(x, p2_)/_POLY7(x, q2_)));
}
/*	hypot.c		*/
/*
 *	hypot(a, b) returns sqrt(a^2 + b^2), avoiding unnecessary overflows.
 *	Returns ERANGE error and value HUGE if the correct value would overflow.
 */

#define ITERATIONS	4

double
hypot(a, b)
register double a, b;
{
	register double t;
	register int i = ITERATIONS;
	struct exception exc;

	if ((exc.arg1 = a) < 0)
		a = -a;
	if ((exc.arg2 = b) < 0)
		b = -b;
	if (a > b) {				/* make sure |a| <= |b| */
		t = a;
		a = b;
		b = t;
	}
	if ((t = a/b) < X_EPS)			/* t <= 1 */
		return (b);			/* t << 1 */
	a = 1 + t * t;				/* a = 1 + (a/b)^2 */
	t = 0.5 + 0.5 * a;			/* first guess for sqrt */
	do {
		t = 0.5 * (t + a/t);
	} while (--i > 0);			/* t <= sqrt(2) */
	if (b < MAXDOUBLE/M_SQRT2)		/* result can't overflow */
		return (t * b);
	if ((t *= 0.5 * b) < MAXDOUBLE/2)	/* result won't overflow */
		return (t + t);
	exc.type = OVERFLOW;
	exc.name = "hypot";
	exc.retval = HUGE;
	if (!matherr(&exc))
		errno = ERANGE;
	return (exc.retval);
}
/*	jn.c		*/
/*
 *	Double-precision Bessel's function of
 *	the first and second kinds and of
 *	integer order.
 *
 *	jn(n, x) returns the value of Jn(x) for all
 *	integer values of n and all real values
 *	of x.
 *	Returns ERANGE error and value 0 for large arguments.
 *	Calls j0, j1.
 *
 *	For n = 0, j0(x) is called,
 *	For n = 1, j1(x) is called.
 *	For n < x, forward recursion is used starting
 *	from values of j0(x) and j1(x).
 *	For n > x, a continued fraction approximation to
 *	j(n, x)/j(n - 1, x) is evaluated and then backward
 *	recursion is used starting from a supposed value
 *	for j(n, x).  The resulting value of j(0, x) is
 *	compared with the actual value to correct the
 *	supposed value of j(n, x).
 *
 *	yn(n, x) is similar in all respects, except
 *	that y0 and y1 are called, and that forward recursion
 *	is used for values of n > 1.
 *	Returns EDOM error and value -HUGE if argument <= 0.
 */

extern double jn_error();

double
jn(n, x)
register int n;
register double x;
{
	double a, b, temp, t;
	int i;

	if (_ABS(x) > X_TLOSS)
		return (jn_error(n, x, 1));
	if (n == 0)
		return (j0(x));
	if (x == 0)
		return (x);
	if (n < 0) {
		n = -n;
		x = -x;
	}
	if (n == 1)
		return (j1(x));
	if (n <= x) {
		a = j0(x);
		b = j1(x);
		for (i = 1; i < n; i++) {
			temp = b;
			b = (i + i)/x * b - a;
			a = temp;
		}
		return (b);
	}
	temp = x * x;
	for (t = 0, i = n + 16; i > n; i--)
		t = temp/(i + i - t);
	a = t = x/(n + n - t);
	b = 1;
	for (i = n - 1; i > 0; i--) {
		temp = b;
		b = (i + i)/x * b - a;
		a = temp;
	}
	return (t * j0(x)/b);
}

double
yn(n, x)
register int n;
register double x;
{
	double a, b, temp;
	int i, neg;

	if (x <= 0) {
		struct exception exc;

		exc.type = DOMAIN;
		exc.name = "yn";
		exc.arg1 = n;
		exc.arg2 = x;
		exc.retval = -HUGE;
		if (!matherr(&exc)) {
			(void) write(2, "yn: DOMAIN error\n", 17);
			errno = EDOM;
		}
		return (exc.retval);
	}
	if (x > X_TLOSS)
		return (jn_error(n, x, 0));
	if (n == 0)
		return (y0(x));
	neg = 0;
	if (n < 0) {
		n = -n;
		neg = n % 2;
	}
	b = y1(x);
	if (n > 1) {
		a = y0(x);
		for (i = 1; i < n; i++) {
			temp = b;
			b = (i + i)/x * b - a;
			a = temp;
		}
	}
	return (neg ? -b : b);
}

static double
jn_error(n, x, jnflag)
int n;
double x;
int jnflag;
{
	struct exception exc;

	exc.type = TLOSS;
	exc.name = jnflag ? "jn" : "yn";
	exc.arg1 = n;
	exc.arg2 = x;
	exc.retval = 0.0;
	if (!matherr(&exc)) {
		(void) write(2, exc.name, 2);
		(void) write(2, ": TLOSS error\n", 14);
		errno = ERANGE;
	}
	return (exc.retval);
}
/*	j0.c		*/
/*
 *	Double-precision Bessel's function
 *	of the first and second kinds
 *	of order zero.
 *
 *	j0(x) returns the value of J0(x)
 *	for all real values of x.
 *
 *	Returns ERANGE error and value 0 for large arguments.
 *	Calls sin, cos, sqrt.
 *
 *	There is a niggling bug in J0 that
 *	causes errors up to 2e-16 for x in the
 *	interval [-8, 8].
 *	The bug is caused by an inappropriate order
 *	of summation of the series.
 *
 *	Coefficients are from Hart & Cheney.
 *	#5849 (19.22D)
 *	#6549 (19.25D)
 *	#6949 (19.41D)
 *
 *	y0(x) returns the value of Y0(x)
 *	for positive real values of x.
 *	Returns EDOM error and value -HUGE if argument <= 0.
 *
 *	Calls sin, cos, sqrt, log, j0.
 *
 *	The values of Y0 have not been checked
 *	to more than ten places.
 *
 *	Coefficients are from Hart & Cheney.
 *	#6245 (18.78D)
 *	#6549 (19.25D)
 *	#6949 (19.41D)
 */

#define P2_0_Q2_0	0.999999999999999999944688442
#define P3_0_Q3_0	-0.0156249999999999999611615235
#define P4_0_Q4_0	0.073804295108687225110222
#define DPOLYD(y, p, q)	for (n = d = 0, i = sizeof(p)/sizeof(p[0]); --i >= 0; ) \
				{ n = n * y + p[i]; d = d * y + q[i]; }

static double tpi = 0.6366197723675813430755350535;
static double p1_[] = {
	0.4933787251794133561816813446e21,
	-.1179157629107610536038440800e21,
	0.6382059341072356562289432465e19,
	-.1367620353088171386865416609e18,
	0.1434354939140344111664316553e16,
	-.8085222034853793871199468171e13,
	0.2507158285536881945555156435e11,
	-.4050412371833132706360663322e8,
	0.2685786856980014981415848441e5,
}, q1_[] = {
	0.4933787251794133562113278438e21,
	0.5428918384092285160200195092e19,
	0.3024635616709462698627330784e17,
	0.1127756739679798507056031594e15,
	0.3123043114941213172572469442e12,
	0.6699987672982239671814028660e9,
	0.1114636098462985378182402543e7,
	0.1363063652328970604442810507e4,
	1.0,
};
static double p2__[] = {
	0.5393485083869438325262122897e7,
	0.1233238476817638145232406055e8,
	0.8413041456550439208464315611e7,
	0.2016135283049983642487182349e7,
	0.1539826532623911470917825993e6,
	0.2485271928957404011288128951e4,
	0.0,
}, q2__[] = {
	0.5393485083869438325560444960e7,
	0.1233831022786324960844856182e8,
	0.8426449050629797331554404810e7,
	0.2025066801570134013891035236e7,
	0.1560017276940030940592769933e6,
	0.2615700736920839685159081813e4,
	1.0,
};
static double p3__[] = {
	-.3984617357595222463506790588e4,
	-.1038141698748464093880530341e5,
	-.8239066313485606568803548860e4,
	-.2365956170779108192723612816e4,
	-.2262630641933704113967255053e3,
	-.4887199395841261531199129300e1,
	0.0,
}, q3__[] = {
	0.2550155108860942382983170882e6,
	0.6667454239319826986004038103e6,
	0.5332913634216897168722255057e6,
	0.1560213206679291652539287109e6,
	0.1570489191515395519392882766e5,
	0.4087714673983499223402830260e3,
	1.0,
};
static double p4__[] = {
	-.2750286678629109583701933175e20,
	0.6587473275719554925999402049e20,
	-.5247065581112764941297350814e19,
	0.1375624316399344078571335453e18,
	-.1648605817185729473122082537e16,
	0.1025520859686394284509167421e14,
	-.3436371222979040378171030138e11,
	0.5915213465686889654273830069e8,
	-.4137035497933148554125235152e5,
}, q4__[] = {
	0.3726458838986165881989980e21,
	0.4192417043410839973904769661e19,
	0.2392883043499781857439356652e17,
	0.9162038034075185262489147968e14,
	0.2613065755041081249568482092e12,
	0.5795122640700729537480087915e9,
	0.1001702641288906265666651753e7,
	0.1282452772478993804176329391e4,
	1.0,
};

extern double j0_asympt();

double
j0(x)
register double x;
{
	register double n, d;
	register int i;

	if ((n = x) < 0)
		x = -x;
	if (x > 8)
		return (j0_asympt(x, n, 1));
	if (x < X_EPS)
		return (1);
	x *= x;
	DPOLYD(x, p1_, q1_);
	return (n/d);
}

double
y0(x)
register double x;
{
	register double n, d, y, z;
	register int i;

	if (x <= 0) {
		struct exception exc;

		exc.type = DOMAIN;
		exc.name = "y0";
		exc.arg1 = x;
		exc.retval = -HUGE;
		if (!matherr(&exc)) {
			(void) write(2, "y0: DOMAIN error\n", 17);
			errno = EDOM;
		}
		return (exc.retval);
	}
	if (x > 8)
		return (j0_asympt(x, x, 0));
	y = tpi * log(x);
	if (x < X_EPS)
		return (y - P4_0_Q4_0);
	z = x * x;
	DPOLYD(z, p4__, q4__);
	return (n/d + y * j0(x));
}

static double
j0_asympt(x, n, j0flag)
register double x, n;
int j0flag;
{
	register double z, d, pzero, qzero;
	register int i;

	if (x > X_TLOSS) {
		struct exception exc;

		exc.type = TLOSS;
		exc.name = j0flag ? "j0" : "y0";
		exc.arg1 = n;
		exc.retval = 0.0;
		if (!matherr(&exc)) {
			(void) write(2, exc.name, 2);
			(void) write(2, ": TLOSS error\n", 14);
			errno = ERANGE;
		}
		return (exc.retval);
	}
	if (x > X_PLOSS) {
		pzero = P2_0_Q2_0;
		qzero = P3_0_Q3_0;
	} else {
		z = 64/(x * x);
		DPOLYD(z, p2__, q2__);
		pzero = n/d;
		DPOLYD(z, p3__, q3__);
		qzero = n/d;
	}
	qzero *= 8/x;
	z = sqrt(tpi/x);
	pzero *= z;
	qzero *= z;
	x -= M_PI_4;
	return (j0flag ? pzero * cos(x) - qzero * sin(x) :
			 pzero * sin(x) + qzero * cos(x));
}
/*	j1.c		*/
/*
 *	Double-precision Bessel's function
 *	of the first and second kinds
 *	of order one.
 *
 *	j1(x) returns the value of J1(x)
 *	for all real values of x.
 *
 *	Returns ERANGE error and value 0 for large arguments.
 *	Calls sin, cos, sqrt.
 *
 *	There is a niggling bug in J1 that
 *	causes errors up to 2e-16 for x in the
 *	interval [-8, 8].
 *	The bug is caused by an inappropriate order
 *	of summation of the series.
 *
 *	Coefficients are from Hart & Cheney.
 *	#6050 (20.98D)
 *	#6750 (19.19D)
 *	#7150 (19.35D)
 *
 *	y1(x) returns the value of Y1(x)
 *	for positive real values of x.
 *	Returns EDOM error and value -HUGE if argument <= 0.
 *
 *	Calls sin, cos, sqrt, log, j1.
 *
 *	The values of Y1 have not been checked
 *	to more than ten places.
 *
 *	Coefficients are from Hart & Cheney.
 *	#6447 (22.18D)
 *	#6750 (19.19D)
 *	#7150 (19.35D)
 */

#define P1_0_Q1_0	0.4999999999999999999989557017
#define P2_0_Q2_1	1.0000000000000000000646346901
#define P3_0_Q3_1	0.046874999999999999955398015174
#define DPOLYD(y, p, q)	for (n = d = 0, i = sizeof(p)/sizeof(p[0]); --i >= 0; ) \
				{ n = n * y + p[i]; d = d * y + q[i]; }

static double p1__[] = {
	0.581199354001606143928050809e21,
	-.6672106568924916298020941484e20,
	0.2316433580634002297931815435e19,
	-.3588817569910106050743641413e17,
	0.2908795263834775409737601689e15,
	-.1322983480332126453125473247e13,
	0.3413234182301700539091292655e10,
	-.4695753530642995859767162166e7,
	0.2701122710892323414856790990e4,
}, q1__[] = {
	0.1162398708003212287858529400e22,
	0.1185770712190320999837113348e20,
	0.6092061398917521746105196863e17,
	0.2081661221307607351240184229e15,
	0.5243710262167649715406728642e12,
	0.1013863514358673989967045588e10,
	0.1501793594998585505921097578e7,
	0.1606931573481487801970916749e4,
	1.0,
};
static double p2___[] = {
	-.4435757816794127857114720794e7,
	-.9942246505077641195658377899e7,
	-.6603373248364939109255245434e7,
	-.1523529351181137383255105722e7,
	-.1098240554345934672737413139e6,
	-.1611616644324610116477412898e4,
	0.0,
}, q2___[] = {
	-.4435757816794127856828016962e7,
	-.9934124389934585658967556309e7,
	-.6585339479723087072826915069e7,
	-.1511809506634160881644546358e7,
	-.1072638599110382011903063867e6,
	-.1455009440190496182453565068e4,
	1.0,
};
static double p3___[] = {
	0.3322091340985722351859704442e5,
	0.8514516067533570196555001171e5,
	0.6617883658127083517939992166e5,
	0.1849426287322386679652009819e5,
	0.1706375429020768002061283546e4,
	0.3526513384663603218592175580e2,
	0.0,
}, q3___[] = {
	0.7087128194102874357377502472e6,
	0.1819458042243997298924553839e7,
	0.1419460669603720892855755253e7,
	0.4002944358226697511708610813e6,
	0.3789022974577220264142952256e5,
	0.8638367769604990967475517183e3,
	1.0,
};
static double p4___[] = {
	-.9963753424306922225996744354e23,
	0.2655473831434854326894248968e23,
	-.1212297555414509577913561535e22,
	0.2193107339917797592111427556e20,
	-.1965887462722140658820322248e18,
	0.9569930239921683481121552788e15,
	-.2580681702194450950541426399e13,
	0.3639488548124002058278999428e10,
	-.2108847540133123652824139923e7,
	0.0,
}, q4___[] = {
	0.5082067366941243245314424152e24,
	0.5435310377188854170800653097e22,
	0.2954987935897148674290758119e20,
	0.1082258259408819552553850180e18,
	0.2976632125647276729292742282e15,
	0.6465340881265275571961681500e12,
	0.1128686837169442121732366891e10,
	0.1563282754899580604737366452e7,
	0.1612361029677000859332072312e4,
	1.0,
};

extern double j1_asympt();

double
j1(x)
register double x;
{
	register double n, d, y;
	register int i;

	if ((y = x) < 0)
		x = -x;
	if (x > 8)
		return (j1_asympt(x, y, 1));
	if (x < X_EPS)
		return (P1_0_Q1_0 * y);
	x *= x;
	DPOLYD(x, p1__, q1__);
	return (y * n/d);
}

double
y1(x)
register double x;
{
	register double n, d, z;
	register int i;

	if (x <= 0) {
		struct exception exc;

		exc.type = DOMAIN;
		exc.name = "y1";
		exc.arg1 = x;
		exc.retval = -HUGE;
		if (!matherr(&exc)) {
			(void) write(2, "y1: DOMAIN error\n", 17);
			errno = EDOM;
		}
		return (exc.retval);
	}
	if (x > 8)
		return (j1_asympt(x, x, 0));
	z = x * x;
	DPOLYD(z, p4___, q4___);
	return (x * n/d + tpi * (j1(x) * log(x) - 1/x));
}

static double
j1_asympt(x, n, j1flag)
register double x, n;
int j1flag;
{
	register double z, d, pzero, qzero;
	register int i;
	struct exception exc;

	exc.arg1 = n;
	if (x > X_TLOSS) {
		exc.type = TLOSS;
		exc.name = j1flag ? "j1" : "y1";
		exc.retval = 0.0;
		if (!matherr(&exc)) {
			(void) write(2, exc.name, 2);
			(void) write(2, ": TLOSS error\n", 14);
			errno = ERANGE;
		}
		return (exc.retval);
	}
	if (x > X_PLOSS) {
		pzero = P2_0_Q2_1;
		qzero = P3_0_Q3_1;
	} else {
		z = 64/(x * x);
		DPOLYD(z, p2___, q2___);
		pzero = n/d;
		DPOLYD(z, p3___, q3___);
		qzero = n/d;
	}
	qzero *= 8/x;
	z = sqrt(tpi/x);
	pzero *= z;
	qzero *= z;
	x -= 3 * M_PI_4;
	if (!j1flag)
		return (pzero * sin(x) + qzero * cos(x));
	x = pzero * cos(x) - qzero * sin(x);
	return (exc.arg1 < 0 ? -x : x);
}
/*	pow.c		*/
/*
 *	pow(x, y) returns x ** y.
 *	Returns EDOM error and value 0 for 0 to a non-positive power
 *	or negative to a non-integral power;
 *	ERANGE error and value HUGE or -HUGE when the correct value
 *	would overflow, or 0 when the correct value would underflow.
 *	uses log and exp
 *	This version accepts negative x and integral y,
 *	apparently in violation of the ANSI FORTRAN standard for x ** y.
 *	There is a much more accurate but much more elaborate algorithm
 *	in Cody and Waite, which should be substituted for this.
 */

double
pow(x, y)
register double x, y;
{
	register int neg;
	struct exception exc;

	if (y == 1) /* easy special case */
		return (x);
	exc.name = "pow";
	exc.arg1 = x;
	exc.arg2 = y;
	exc.retval = 0.0;
	if (!x) {
		if (y > 0)
			return (x); /* (0 ** pos) == 0 */
		goto domain;
	}
	neg = 0;
	if (x < 0) { /* test using integer arithmetic if possible */
		if (y >= -MAXLONG && y <= MAXLONG) {
			register long ly = (long)y;

			if ((double)ly != y)
				goto domain; /* y not integral */
			neg = ly % 2;
		} else {
			double fr, dum, modf();

			if (fr = modf(0.5 * y, &dum)) {
				if (fr != 0.5)
					goto domain; /* y not integral */
				neg++; /* y is odd */
			}
		}
		x = -x;
	}
	if (x != 1) { /* x isn't the final result */
		/* the following code protects against multiplying x and y
		 * until there is no chance of multiplicative overflow */
		if ((x = log(x)) < 0) { /* preserve sign of product */
			x = -x;
			y = -y;
		}
		if (y > LN_MAXDOUBLE/x) {
			exc.type = OVERFLOW;
			exc.retval = neg ? -HUGE : HUGE;
			if (!matherr(&exc))
				errno = ERANGE;
			return (exc.retval);
		}
		if (y < LN_MINDOUBLE/x) {
			exc.type = UNDERFLOW;
			if (!matherr(&exc))
				errno = ERANGE;
			return (exc.retval);
		}
		x = exp(x * y); /* finally; no mishap can occur */
	}
	return (neg ? -x : x);

domain:
	exc.type = DOMAIN;
	if (!matherr(&exc)) {
		(void) write(2, "pow: DOMAIN error\n", 18);
		errno = EDOM;
	}
	return (exc.retval);
}
/*	log.c		*/
/*
 *	log returns the natural logarithm of its double-precision argument.
 *	log10 returns the base-10 logarithm of its double-precision argument.
 *	Returns EDOM error and value -HUGE if argument <= 0.
 *	Algorithm and coefficients from Cody and Waite (1980).
 *	Calls frexp.
 */

#define C1	0.693359375
#define C2	-2.121944400546905827679e-4

extern double log_error();

double
log(x)
register double x;
{
	static double p[] = {
		-0.78956112887491257267e0,
		 0.16383943563021534222e2,
		-0.64124943423745581147e2,
	}, q[] = {
		 1.0,
		-0.35667977739034646171e2,
		 0.31203222091924532844e3,
		-0.76949932108494879777e3,
	};
	register double y;
	int n; /* can't be in register because of frexp() below */

	if (x <= 0)
		return (log_error(x, "log", 3));
	y = 1.0;
	x = frexp(x, &n);
	if (x < M_SQRT1_2) {
		n--;
		y = 0.5;
	}
	x = (x - y)/(x + y);
	x += x;
	y = x * x;
	x += x * y * _POLY2(y, p)/_POLY3(y, q);
	y = (double)n;
	x += y * C2;
	return (x + y * C1);
}

double
log10(x)
register double x;
{
	return (x > 0 ? log(x) * M_LOG10E : log_error(x, "log10", 5));
}

static double
log_error(x, f_name, name_len)
double x;
char *f_name;
unsigned int name_len;
{
	register char *err_type;
	unsigned int mess_len;
	struct exception exc;

	exc.name = f_name;
	exc.retval = -HUGE;
	exc.arg1 = x;
	if (x) {
		exc.type = DOMAIN;
		err_type = ": DOMAIN error\n";
		mess_len = 15;
	} else {
		exc.type = SING;
		err_type = ": SING error\n";
		mess_len = 13;
	}
	if (!matherr(&exc)) {
		(void) write(2, f_name, name_len);
		(void) write(2, err_type, mess_len);
		errno = EDOM;
	}
	return (exc.retval);
}
/*	sin.c		*/
/*
 *	sin and cos of double-precision argument.
 *	Returns ERANGE error and value 0 if argument too large.
 *	Algorithm and coefficients from Cody and Waite (1980).
 */

double
sin(x)
double x;
{
	extern double sin_cos();

	return (sin_cos(x, 0));
}

static double
sin_cos(x, cosflag)
register double x;
int cosflag;
{
	register double y;
	register int neg = 0;
	struct exception exc;
	
	exc.arg1 = x;
	if (x < 0) {
		x = -x;
		neg++;
	}
	y = x;
	if (cosflag) {
		neg = 0;
		y += M_PI_2;
		exc.name = "cos";
	} else
		exc.name = "sin";
	if (y > X_TLOSS) {
		exc.type = TLOSS;
		exc.retval = 0.0;
		if (!matherr(&exc)) {
			(void) write(2, exc.name, 3);
			(void) write(2, ": TLOSS error\n", 14);
			errno = ERANGE;
		}
		return (exc.retval);
	}
	y = y * M_1_PI + 0.5;
	if (x <= MAXLONG) { /* reduce using integer arithmetic if possible */
		register long n = (long)y;

		y = (double)n;
		if (cosflag)
			y -= 0.5;
		_REDUCE(long, x, y, 3.1416015625, -8.908910206761537356617e-6);
		neg ^= (int)n % 2;
	} else {
		extern double modf();
		double dn;

		x = (modf(y, &dn) - 0.5) * M_PI;
		if (modf(0.5 * dn, &dn))
			neg ^= 1;
	}
	if (x < 0) {
		x = -x;
		neg ^= 1;
	}
	if (x > X_EPS) { /* skip for efficiency and to prevent underflow */
		static double p[] = {
			 0.27204790957888846175e-14,
			-0.76429178068910467734e-12,
			 0.16058936490371589114e-9,
			-0.25052106798274584544e-7,
			 0.27557319210152756119e-5,
			-0.19841269841201840457e-3,
			 0.83333333333331650314e-2,
			-0.16666666666666665052e0,
		};

		y = x * x;
		x += x * y * _POLY7(y, p);
		if (x > 1.0) /* inhibit roundoff out of range */
			x = 1.0;
	}
	exc.retval = neg ? -x : x;
	if (exc.arg1 > X_PLOSS || exc.arg1 < -X_PLOSS) {
		exc.type = PLOSS;
		if (!matherr(&exc))
			errno = ERANGE;
	}
	return (exc.retval);
}

double
cos(x)
register double x;
{
	return (x ? sin_cos(x, 1) : 1.0);
}
/*	sinh.c		*/
/*
 *	sinh returns the hyperbolic sine of its double-precision argument.
 *	A series is used for arguments smaller in magnitude than 1.
 *	The exponential function is used for arguments
 *	greater in magnitude than 1.
 *
 *	cosh returns the hyperbolic cosine of its double-precision argument.
 *	cosh is computed from the exponential function for
 *	all arguments.
 *
 *	Returns ERANGE error and value HUGE (or -HUGE for sinh of
 *	negative argument) if the correct result would overflow.
 *
 *	Algorithm and coefficients from Cody and Waite (1980).
 */

#define X_MAX1	(LN_MAXDOUBLE + M_LN2)
#define LNV	0.6931610107421875
#define V2M1	0.13830277879601902638e-4

extern double sinh_exc();

double
sinh(x)
register double x;
{
	register double y = _ABS(x);

	if (y <= 1) {
		static double p[] = {
			-0.78966127417357099479e0,
			-0.16375798202630751372e3,
			-0.11563521196851768270e5,
			-0.35181283430177117881e6,
		}, q[] = {
			 1.0,
			-0.27773523119650701667e3,
			 0.36162723109421836460e5,
			-0.21108770058106271242e7,
		};

		if (y < X_EPS) /* for efficiency and to prevent underflow */
			return (x);
		y = x * x;
		return (x + x * y * _POLY3(y, p)/_POLY3(y, q));
	}
	if (y > LN_MAXDOUBLE) /* exp(x) would overflow */
		return (sinh_exc(x, y, 1));
	x = exp(x);
	return (0.5 * (x - 1.0/x));
}

double
cosh(x)
register double x;
{
	register double y = _ABS(x);

	if (y > LN_MAXDOUBLE) /* exp(x) would overflow */
		return (sinh_exc(x, y, 0));
	x = exp(y);
	return (0.5 * (x + 1.0/x));
}

static double
sinh_exc(x, y, sinhflag)
register double x, y;
register int sinhflag;
{
	int neg = (x < 0 && sinhflag); /* sinh of negative argument */
	struct exception exc;

	if (y < X_MAX1) { /* result is still representable */
		x = exp(y - LNV);
		x += V2M1 * x;
		return (neg ? -x : x);
	}
	exc.type = OVERFLOW;
	exc.name = sinhflag ? "sinh" : "cosh";
	exc.arg1 = x;
	exc.retval = neg ? -HUGE : HUGE;
	if (!matherr(&exc))
		errno = ERANGE;
	return (exc.retval);
}
/*	sqrt.c		*/
/*
 *	sqrt returns the square root of its double-precision argument,
 *	using Newton's method.
 *	Returns EDOM error and value 0 if argument negative.
 *	Calls frexp and ldexp.
 */

#define ITERATIONS	4

double
sqrt(x)
register double x;
{
	register double y;
	int iexp; /* can't be in register because of frexp() below */
	register int i = ITERATIONS;

	if (x <= 0) {
		struct exception exc;

		if (!x)
			return (x); /* sqrt(0) == 0 */
		exc.type = DOMAIN;
		exc.name = "sqrt";
		exc.arg1 = x;
		exc.retval = 0.0;
		if (!matherr(&exc)) {
			(void) write(2, "sqrt: DOMAIN error\n", 19);
			errno = EDOM;
		}
		return (exc.retval);
	}
	y = frexp(x, &iexp); /* 0.5 <= y < 1 */
	if (iexp % 2) { /* iexp is odd */
		--iexp;
		y += y; /* 1 <= y < 2 */
	}
	y = ldexp(y + 1.0, iexp/2 - 1); /* first guess for sqrt */
	do {
		y = 0.5 * (y + x/y);
	} while (--i > 0);
	return (y);
}
/*	tan.c		*/
/*
 *	tan returns the tangent of its double-precision argument.
 *	Returns ERANGE error and value 0 if argument too large.
 *	Algorithm and coefficients from Cody and Waite (1980).
 */

double
tan(x)
register double x;
{
	register double y;
	register int neg = 0, invert;
	struct exception exc;

	exc.name = "tan";
	exc.arg1 = x;
	if (x < 0) {
		x = -x;
		neg++;
	}
	if (x > X_TLOSS/2) {
		exc.type = TLOSS;
		exc.retval = 0.0;
		if (!matherr(&exc)) {
			(void) write(2, "tan: TLOSS error\n", 17);
			errno = ERANGE;
		}
		return (exc.retval);
	}
	y = x * M_2_PI + 0.5;
	if (x <= MAXLONG) { /* reduce using integer arithmetic if possible */
		register long n = (long)y;

		y = (double)n;
		_REDUCE(long, x, y, 1.57080078125, -4.454455103380768678308e-6);
		invert = (int)n % 2;
	} else {
		extern double modf();
		double dn;

		x = (modf(y, &dn) - 0.5) * M_PI_2;
		invert = modf(0.5 * dn, &dn) ? 1 : 0;
	}
	if (x > -X_EPS && x < X_EPS) {
		if (!x && invert)
			/*
			 * This case represents a range reduction
			 * from (2n + 1) * PI/2 which happens to return 0.
			 * This is mathematically insignficant, and extremely
			 * unlikely in any case, so it is best simply
			 * to substitute an arbitrarily small value
			 * as if the reduction were one bit away from 0,
			 * and not to deal with this as a range error.
			 */
			x = 1.0/X_TLOSS;
		y = 1.0; /* skip for efficiency and to prevent underflow */
	} else {
		static double p[] = {
			-0.17861707342254426711e-4,
			 0.34248878235890589960e-2,
			-0.13338350006421960681e+0,
		}, q[] = {
			 0.49819433993786512270e-6,
			-0.31181531907010027307e-3,
			 0.25663832289440112864e-1,
			-0.46671683339755294240e+0,
			 1.0,
		};

		y = x * x;
		x += x * y * _POLY2(y, p);
		y = _POLY4(y, q);
	}
	if (neg)
		x = -x;
	exc.retval = invert ? -y/x : x/y;
	if (exc.arg1 > X_PLOSS/2 || exc.arg1 < -X_PLOSS/2) {
		exc.type = PLOSS;
		if (!matherr(&exc))
			errno = ERANGE;
	}
	return (exc.retval);
}
/*	tanh.c		*/
/*
 *	tanh returns the hyperbolic tangent of its double-precision argument.
 *	It calls exp for absolute values of the argument > ~0.55.
 *	There are no error returns.
 *	Algorithm and coefficients from Cody and Waite (1980).
 */

#define X_BIG	(0.5 * (LN_MAXDOUBLE + M_LN2))
#define LN_3_2	0.54930614433405484570

double
tanh(x)
register double x;
{
	register int neg = 0;

	if (x < 0) {
		x = -x;
		neg++;
	}
	if (x > X_BIG)
		x = 1.0;
	else if (x > LN_3_2) {
		x = 0.5 - 1.0/(exp(x + x) + 1.0); /* two steps recommended */
		x += x; /* for wobbling-precision machines (like IBM) */
	} else if (x > X_EPS) { /* skip for efficiency and to prevent underflow */
		static double p[] = {
			-0.96437492777225469787e0,
			-0.99225929672236083313e2,
			-0.16134119023996228053e4,
		}, q[] = {
			 1.0,
			 0.11274474380534949335e3,
			 0.22337720718962312926e4,
			 0.48402357071988688686e4,
		};
		register double y = x * x;

		x += x * y * _POLY2(y, p)/_POLY3(y, q);
	}
	return (neg ? -x : x);
}
/*	exp.c		*/
/*
 *	exp returns the exponential function of its double-precision argument.
 *	Returns ERANGE error and value 0 if argument too small,
 *	   value HUGE if argument too large.
 *	Algorithm and coefficients from Cody and Waite (1980).
 *	Calls ldexp.
 */

double
exp(x)
register double x;
{
	static double p[] = {
		0.31555192765684646356e-4,
	        0.75753180159422776666e-2,
	        0.25000000000000000000e0,
	}, q[] = {
		0.75104028399870046114e-6,
	        0.63121894374398503557e-3,
	        0.56817302698551221787e-1,
	        0.50000000000000000000e0,
	};
	register double y;
	register int n;
	struct exception exc;

	exc.arg1 = x;
	if (x < 0)
		x = -x;
	if (x < X_EPS) /* use first term of Taylor series expansion */
		return (1.0 + exc.arg1);
	exc.name = "exp";
	if (exc.arg1 <= LN_MINDOUBLE) {
		if (exc.arg1 == LN_MINDOUBLE) /* protect against roundoff */
			return (MINDOUBLE); /* causing ldexp to underflow */
		exc.type = UNDERFLOW;
		exc.retval = 0.0;
		if (!matherr(&exc))
			errno = ERANGE;
		return (exc.retval);
	}
	if (exc.arg1 >= LN_MAXDOUBLE) {
		if (exc.arg1 == LN_MAXDOUBLE) /* protect against roundoff */
			return (MAXDOUBLE); /* causing ldexp to overflow */
		exc.type = OVERFLOW;
		exc.retval = HUGE;
		if (!matherr(&exc)) 
			errno = ERANGE;
		return (exc.retval);
	}
	n = (int)(x * M_LOG2E + 0.5);
	y = (double)n;
	_REDUCE(int, x, y, 0.693359375, -2.1219444005469058277e-4);
	if (exc.arg1 < 0) {
		x = -x;
		n = -n;
	}
	y = x * x;
	x *= _POLY2(y, p);
	return (ldexp(0.5 + x/(_POLY3(y, q) - x), n + 1));
}

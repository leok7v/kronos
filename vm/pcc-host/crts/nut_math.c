/* User supplied matherr() function used with NUT programming system */

/* Revision log:
 * 07-Jan-88 SS created
 */

#include <math.h>

#ifdef NUT
#define printf wdprint
extern void wdprint();
#endif

int
matherr(x)
register struct exception *x;
 {
       switch (x->type)	{
       case DOMAIN:
	    printf("DOMAIN error in %s(%g) = %g\n",
		 x->name, x->arg1, x->retval);
	    errno = EDOM;
	    return (1);	/* take	no other action	*/
       case SING:
	    printf("SINGularity error in %s(%g) = %g\n",
		 x->name, x->arg1, x->retval);
	    errno = EDOM;
	    return (1);	/* take	no other action	*/
       case PLOSS:
	    printf("loss of significance in %s(%g) = %g\n",
		 x->name, x->arg1, x->retval);
	    errno = ERANGE;
	    return (1);	/* take	no other action	*/
       case TLOSS:
	    printf("TOTAL loss of significance in %s(%g) = %g\n",
		 x->name, x->arg1, x->retval);
	    errno = ERANGE;
	    return (1);	/* take	no other action	*/
       case OVERFLOW:
	    printf("OVERFLOW range error in %s(%g) = %g\n",
		 x->name, x->arg1, x->retval);
	    errno = ERANGE;
	    return (1);	/* take	no other action	*/
       case UNDERFLOW:
	    printf("UNDERFLOW range error in %s(%g) = %g\n",
		 x->name, x->arg1, x->retval);
	    errno = ERANGE;
	    return (1);	/* take	no other action	*/
       }
  }

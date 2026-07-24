/*	UNIX standard version of matherr()	*/

#include <math.h>

/*ARGSUSED*/
int
matherr(x)
struct exception *x;
{
	return (0);
}

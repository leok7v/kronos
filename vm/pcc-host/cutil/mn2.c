#include <stdio.h>

#define rcoord(rmin,rmax,newpts,oldpts) \
	rmin + (rmax - rmin) * ((float) newpts) / ((float) (oldpts - 1))
#define print	printf
#define printd	printf 


main(argc,argv)
int	argc;
char	*argv[];
{float	c0 = -2.0,
	c1 = 0.5;
float 
	d0 = -1.25,
	d1 = 1.25;
	print("\016");
	process(c0, c1, d0, d1);
	print("\033\\\017\n");
	return;
}


/*			P r o c e s s					*/
process(c0,c1,d0,d1)
float	c0,c1,
	d0,d1;
{
int	i;
int	nx,ny;				/* x and y resolution		*/
 int	l1,l2,lmax;			/* levels			*/
 char	line[100];
 int	mx0, mx1,			/* blow rectangle		*/
	my0,my1;			/*     coordinates		*/
 float	new_c0, new_c1,			/* new x bounds			*/
	new_d0,new_d1;			/* new y bounds			*/
 long	offset,				/* picture offset within file 	*/
	bytecount;			/* nr. of bytes in the picture	*/

	print("\33\\\33[0t\016");			/* erase picture */
	print("\33[1;104HRectangle");
	print("\33[2;102Hx -2.0 0.5\33[0K");
	print("\33[3;102Hy -1.25 1.25\33[0K");
	print("\33[12;102Hx points ( <=300 ):");
	do {
		print("\33[13;103H\33[0K");
		if (!sscanf(gets(line),"%d",&nx))
			nx = -1;
		} while ((nx < 0) || (nx > 300));
	print("\33[4;120H%3d points",nx);
	print("\33[14;102Hy points ( <=300 ):");
	do {
		print("\33[15;103H\33[0K");
		if (!sscanf(gets(line),"%d",&ny))
			ny = -1;
		} while ( (ny < 0) || (ny > 300) );
	print("\33[5;120H%3d points",ny);
	print("\33[16;102HEnter upper limit");
	do {
		print("\033[17;103H\33[0K");
		if (sscanf(gets(line),"%d",&lmax) != 1)
			lmax = -1;
		} while (lmax < 0);
	 print("\33P");			/* enter REGIS mode	*/
	fractal(c0,c1,d0,d1,nx,ny,lmax);
	 print("\33\\");
}

/* 			D r a w   p i c t u r e				*/
fractal(c0,c1,d0,d1,nx,ny,lmax)
float	c0,c1,			/* x bounds	*/
	d0,d1;			/* y bounds	*/
int	nx,			/* x dots	*/
	ny;			/* y dots	*/
int 	lmax;			/* man iter count */
{float	c,delta_c,
	d,delta_d,
	x,x2,y,y2,z;	
 register	int	ix, iy;
 register	int    count;

	delta_c = (c1 - c0) / ((float)(nx-1));
	delta_d = (d1 - d0) / ((float) (ny - 1));
	for (iy = 0, d = d1; iy < ny; iy++, d -= delta_d) {
		print("p(b)P[606,560]t'%3d'p(e)",iy);
		for (ix = 0, c = c0; ix < nx; ix++, c += delta_c) {
			x2 = x = (y = y2 = 0.0);
			for (count = 0; count < lmax; count++) {
			    z = x2 - y2 + c;
			    y = 2*x*y + d;
			    x = z;
			    if ((x2 = x*x) + (y2 = y*y) > 4.0)
				break;
			    }
			if (count < lmax) 
		     	    pnt(3 - count % 3,ix,iy);
			else 
			    pnt(0,ix,iy);
			/*putc(count,tempfile);*/
			}
		}
	pnt(0,-1,-1);
	return;
}

/*			D r a w   p o i n t				*/
pnt(color,x,y)
register
int	color,			/* 0-3 */
	x,			/* 0-299 */
	y;			/* 0-299 */
{static int 	color0 = 0,
		len = 1,
		x0 = -1,
		y0 = -1;
 char	buf[100];
	if ((color0 == color) && (y0 == y))
		len += 2;
	else {
		if (color0) {
	       	    print("P[%d,%d]w(i%d)V[+%d]",x0*2,y0*2,color0,len);
		    }
		x0 = x;
		y0 = y;
		color0 = color;
		len = 1;
		}
	return;
}


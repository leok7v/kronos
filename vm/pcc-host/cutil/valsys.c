#include <values.h>

main()
{
short s;
int i;
long l;
float f;
double d;
printf("**** system values in C runtime ***\n\n");
printf("1. Bits per byte --- %d\n",BITSPERBYTE);
printf("2. SIZEOF int --- %d\n",sizeof(int));
printf("3. SIZEOF long --- %d\n",sizeof(long));
printf("4. SIZEOF float --- %d\n",sizeof(float));
printf("5. SIZEOF double --- %d\n",sizeof(double));
printf("6. SIZEOF char --- %d\n",sizeof(char));
printf("6-a. SIZEOF short --- %d\n",sizeof(short));
printf("7. Bits in int --- %d\n",BITS(int));
printf("8. Bits in long --- %d\n",BITS(long));
printf("9. Bits in float --- %d\n",BITS(float));
printf("10. Bits in double --- %d\n",BITS(double));
printf("11. Bits in char --- %d\n",BITS(char));
printf("11-a. Bits in short --- %d\n",BITS(short));
s = (HIBITS);
printf("12. Short with only the high-order bit turned on --- %d\n",s);
dump(s);
i = HIBITI;
printf("13. Int with only the high-order bit turned on --- %d\n",i);
dump(i);
l = HIBITL;
printf("14. Long with only the high-order bit turned on --- %d\n",l);
dump(l);
s = MAXSHORT;
printf("15. Largest short --- %d\n",s);
dump(s);
i = MAXINT;
printf("16. Largest int --- %d\n",i);
dump(i);
l = MAXLONG;
printf("17. Largest long --- %d\n",l);
dump(l);
printf("_EXPBASE - the exponent base --- %d\n",_EXPBASE);
printf("DMAXEXP -  the maximum exponent of a double --- %d\n",DMAXEXP);
printf("FMAXEXP	- the maximum exponent of a float --- %d\n",FMAXEXP);
printf("DMINEXP - the minimum exponent of a double --- %d\n",DMINEXP);
printf("FMINEXP - the minimum exponent of a float --- %d\n",FMINEXP);
dump(MAXDOUBLE);
printf("MAXDOUBLE - the largest double --- %e\n",MAXDOUBLE);
dump(MAXDOUBLE);
printf("MAXFLOAT - the largest float --- %e\n",MAXFLOAT);
dump(MAXFLOAT);
printf("MINDOUBLE - the smallest double --- %e\n",MINDOUBLE);
dump(MINDOUBLE);
printf("MINFLOAT - the smallest float --- %e\n",MINFLOAT);
dump(MINFLOAT);
printf("DSIGNIF	- the number of significant bits in a double --- %d\n",DSIGNIF);
printf("FSIGNIF	- the number of significant bits in a float --- %d\n",FSIGNIF);
printf("DMAXPOWTWO - the largest power of two exactly representable as a double --- %e\n",DMAXPOWTWO);
dump(DMAXPOWTWO);
printf("FMAXPOWTWO - the largest power of two exactly representable as a float --- %e\n",FMAXPOWTWO);
dump(FMAXPOWTWO);
printf("_DEXPLEN - the number of bits for the exponent of a double --- %d\n",_DEXPLEN);
printf("_FEXPLEN - the number of bits for the exponent of a float --- %d\n",_FEXPLEN);
printf("LN_MAXDOUBLE - the natural log of the largest double --- %f\n",LN_MAXDOUBLE);
printf("LN_MINDOUBLE	- the natural log of the smallest double --- %f\n",LN_MINDOUBLE);
}
dump(x)
int x;
{
int i;
union {int z; char a[4]; } X;
	X.z = x;
	for (i = 3; i >= 0; i --) printf("%2x ", X.a[i] & 0377);
	printf ("\n");
}

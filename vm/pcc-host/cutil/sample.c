#include <stdio.h>

int col = 6;

main()
{
	register int n;
	int i,j,space;
	int aa[30],bb[30];
	char instr[80];

	n = 10;

	printf("\n\t\t\tPascal's triangle\n\n");

	space = n*col/2;
	aa[0]=0; aa[1]=1;
	for(j=1; j<=n; ++j) {
		space=space-col/2;
		for(i=0; i<=space; ++i) printf(" ");
		for(i=1; i<=j; ++i) {
			bb[i]=aa[i-1]+aa[i];
		}
		aa[j+1]=0;
		for(i=1; i<=j; ++i) {
			pr(bb[i],col);
			aa[i]=bb[i];
		}
		printf("\n");
	}
	printf("\n");
}

pr(num,ic)
 int num, ic;
{
	register int i;
	int j;

	for(i=0,j=num; j!=0; ++i)
		j /= 10;
	if(i>ic)
		for(i=1; i++<=ic; ) printf("*");
	else
		for(j=1; j++<=ic-i; ) printf(" ");

	printf("%d", num);
}



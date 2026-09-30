#include "stdio.h"

char *s [] = {
	"misha", "sasha", "hele_mai", "mari", "merike", 
	"enn", "jaan", "kaja", "ahto", "kestutis"};

extern void qsort ();
int	scmp();

main(){
int	i;

	/* NB! sizeof(char *) != 1 ! */

/*	wqsort ((char*)s, 10, sizeof(char *), scmp);  */	
	qsort ((char*)s, 10, sizeof(char *), scmp);	

	for (i = 0; i < 10; i ++)
		printf("%3d  --  %s\n", i, s[i]);
}


scmp(s1, t1)
char	*s1, *t1;
{
char  *ss, *tt, **s, **t;


	s = (char **) s1;
	t = (char **) t1;

	/* Get string pointers from those addresses: */

  	ss = * s; tt = * t;

  	printf("STRCMP -%s- -%s-\n", ss, tt); 

  	for (; *ss == *tt; tt++, ss++)
		if (*ss == '\0') 
			return (0);
  	return (*ss - *tt);
}


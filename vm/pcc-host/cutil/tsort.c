#include "stdio.h"

char *sortbuf[200];
int size;

extern void qsort ();
int	scmp();

main(argc,argv) int argc; char *argv[]; {
int i,len;
char buf[80];
FILE *fp1;

	/* NB! sizeof(char *) != 1 ! */

	if (argc < 2 ){
		fprintf(stderr, "Usage: tsort: filename\n");
		exit(0);
	}
	if( (fp1 = fopen(*++argv, "r")) == NULL ){
		fprintf(stderr, "Can't open file: %s\n", *argv);
		exit(0);
	}
	while( fgets(buf,80, fp1) ){
		sortbuf[size]=(char *)malloc(len=strlen(buf));  
		buf[len-1]='\0';
		strcpy(sortbuf[size++],buf);
	}
	qsort ((char*)sortbuf, size, sizeof(char *), scmp);	

	printf("\nSorted strings from file: %s\n", *argv);
	for (i = 0; i < size; i ++)
		printf("%3d  --  %s\n", i, sortbuf[i]);
}


scmp(s1, t1)
char	*s1, *t1;
{
char  *ss, *tt, **s, **t;


	s = (char **) s1;
	t = (char **) t1;

	/* Get string pointers from those addresses: */

  	ss = * s; tt = * t;

/*  	printf("STRCMP -%s- -%s-\n", ss, tt);   */

  	for (; *ss == *tt; tt++, ss++)
		if (*ss == '\0') 
			return (0);
  	return (*ss - *tt);
}


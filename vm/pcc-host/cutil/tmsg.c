#include <stdio.h>

extern char *msgget();
extern int  msginit(), msgterm();

main()
{
	char msg[100], c, s[3], *penv;
	int i, num;


	if( msginit ( "pccerr.msg") ){	/* not found in the current dir */
		char fname[40];
		if( (penv = getenv("CDIR")) != 0 )
			msginit ( strcat(strcpy(fname,penv),"pccerr.msg") );
		else {
			printf("Message file: pccerr.msg not found\n");
			exit(1);
		}	
	}
	for(;;){
		printf( "Select error type W)arning, U)ser, C)ompiler or Q)uit: " );
	/*	switch( c = toupper(getchar()) ){	*/
		gets(s);
		switch( c = toupper(s[0]) ){

		case 'W':
			printf( "\nWarning errors:\n" );
/*
			scanf( "%d", &num );
			printf("%s\n", msgget( num, msg));
*/
			for (i=1; i<=34; i++) printf("%s\n", msgget( i, msg));
			break;

		case 'U':
			printf( "\nUser errors:\n" );
/*
			scanf( "%d\n", &num );
			printf("%s\n", msgget( num, msg));
*/
			for (i=51; i<=131; i++) printf("%s\n", msgget( i, msg));
			break;

		case 'C':
			printf( "\nCompiler errors:\n" );
/*
			scanf( "%d\n", &num );
			printf("%s\n", msgget( num, msg));
*/
			for (i=201; i<=298; i++) printf("%s\n", msgget( i,msg));
			break;

		case 'Q':
			msgterm();
			return(1);

		default:
			continue;
		}
	}
}

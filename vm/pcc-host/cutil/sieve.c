/*  *** File SIEVE.C ***/

#define TRUE 1
#define FALSE 0
#define SIZE 8190
#define ITS 20

main(){

     int count,iter;
     char flags[SIZE+1];
     long i,prime,k;     

     printf("20 iteration \n");

	for (iter=1; iter<=ITS; iter++) { count=0;
 		for( i=0; i<=SIZE; i++) flags[i]=TRUE;
	  	for( i=0; i<=SIZE; i++){
	       		if(flags[i]){
		    		prime=i+i+3;
		    		k=i+prime;
		    		while( k<=SIZE ){
		    			flags[k]=FALSE;
		    			k+=prime;
		    		}
		    		count++;
	       		}
	  	}
     }
     printf("%d primes \n",count);
}



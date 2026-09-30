#include "stdio.h"
int main(){ int fd,n; char b[40]; fd=open("/usr1/env.cod",0); n=read(fd,b,32); close(fd); printf("READ-CLOSE OK n=%d\n",n); return 0; }

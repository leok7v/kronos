#include "stdio.h"
int main(){ int fd; fd=creat("/usr1/tc.dat",0666); write(fd,"abc\n",4); close(fd); printf("CREAT-CLOSE OK\n"); return 0; }

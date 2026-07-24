#include "stdio.h"
extern int errno;
int main()
{
    int fd;
    errno = 0; fd = creat("/usr1/mr.dat", 0666);
    printf("creat=%d errno=0x%x\n", fd, errno);
    errno = 0; fd = open("/usr1/mr.dat", 0);
    printf("open=%d errno=0x%x\n", fd, errno);
    errno = 0; fd = open("/usr1/dg.cod", 0);
    printf("open(existing dg.cod)=%d errno=0x%x\n", fd, errno);
    return 0;
}

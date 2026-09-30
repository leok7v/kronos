#include "stdio.h"
extern int errno;
int main()
{
    int fd;
    errno = 0;
    fd = open("/usr1/test.c", 0);
    printf("open=%d errno=0x%x\n", fd, errno);
    return 0;
}

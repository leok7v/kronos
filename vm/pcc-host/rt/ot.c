#include "stdio.h"
int main()
{
    int fd, n; char b[40];
    fd = open("/usr1/test.c", 0);
    printf("open(/usr1/test.c) = %d\n", fd);
    if (fd >= 0) { n = read(fd, b, 20); b[n>0?n:0]=0; printf("read %d: [%s]\n", n, b); close(fd); }
    fd = open("test.c", 0);
    printf("open(test.c relative) = %d\n", fd);
    if (fd >= 0) close(fd);
    return 0;
}

#include "stdio.h"
int main()
{
    int fd, n;
    char buf[80];
    fd = creat("/usr1/ft2.dat", 0666);
    if (fd < 0) { printf("CREAT FAIL %d\n", fd); return 1; }
    write(fd, "hello native FS!\n", 17);
    close(fd);
    fd = open("/usr1/ft2.dat", 0);
    if (fd < 0) { printf("OPEN FAIL %d\n", fd); return 1; }
    n = read(fd, buf, 78);
    close(fd);
    buf[n] = 0;
    printf("READ %d BYTES: %s", n, buf);
    return 0;
}

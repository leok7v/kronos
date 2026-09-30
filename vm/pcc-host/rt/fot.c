#include "stdio.h"
int main()
{
    int fd, n; FILE *f; char b[40];
    fd = open("/usr1/test.c", 0);
    printf("open() fd=%d\n", fd);
    if (fd >= 0) { n = read(fd, b, 30); b[n]=0; printf("  read %d: [%s]\n", n, b); close(fd); }
    f = fopen("/usr1/test.c", "r");
    printf("fopen() = %s\n", f ? "OK" : "NULL");
    if (f) { n = fread(b, 1, 30, f); b[n]=0; printf("  fread %d: [%s]\n", n, b); fclose(f); }
    return 0;
}

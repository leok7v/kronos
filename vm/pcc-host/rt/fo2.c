#include "stdio.h"
int main()
{
    FILE *f;
    printf("start\n");
    f = fopen("/usr1/test.c", "r");
    printf("fopen done: %s\n", f ? "OK" : "NULL");
    return 0;
}

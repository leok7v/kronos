#include "stdio.h"
int main(argc, argv)
char *argv[];
{
    int i;
    printf("argc=%d\n", argc);
    for (i = 0; i < argc; i++)
        printf("argv[%d]=[%s]\n", i, argv[i]);
    return 0;
}

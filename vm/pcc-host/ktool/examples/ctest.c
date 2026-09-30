#include "stdio.h"

int fib(n)
int n;
{
    if (n < 2) return n;
    return fib(n-1) + fib(n-2);
}

int main()
{
    int i;
    printf("=== Kronos native C build test ===\n");
    for (i = 0; i < 10; i++)
        printf("fib(%d) = %d\n", i, fib(i));
    printf("done.\n");
    return 0;
}

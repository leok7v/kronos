#include "stdio.h"
int main()
{
    printf("[%5d][%-5d][%05d]\n", 42, 42, 42);
    printf("oct=%o unsigned=%u pct=%%\n", 64, 4000000000);
    printf("str=[%10s][%-10s]\n", "hi", "hi");
    return 0;
}

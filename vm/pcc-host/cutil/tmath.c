#include <math.h>

int i,j;
long l,k;
float f,g;
double d,e,x;

main()
{
  printf("**************************************************************\n");
  printf("**                 WELCOME TO KRONOS C                      **\n");
  printf("**                                                          **\n");
  printf("**  This sample program tests the C compiler, linker  and   **\n");
  printf("**  libraries.  If the number in parentheses matches the    **\n");
  printf("**  number to the immediate left, each component is working **\n");
  printf("**  properly.                                               **\n");
  printf("**************************************************************\n");
  printf("\n");

  printf("Test      int math: 4567 * 10 = "); i = 4567; j = 10;
  printf("%u \t\t(45670)\n",i * j);

  printf("Test long int math: 1234 * 4567 = "); l = 1234; k = 4567;
  printf("%ld \t(5635678)\n",l * k);

  printf("Test float    math: 1.234 + 0.001 = "); f = 1.234; g = 0.001;
  printf("%g \t(1.235)\n",f + g);

  printf("Test double   math: 5635678.0 / 1234.0 = ");
  d = 5635678.0;
  e = 1234.0;
  printf("%g \t(4567)\n",d / e);

  x = 0.357;
  printf("\nTest math functions: x= %f\n",  x );
  printf("Test  SIN(x) = %f \t(0.349465)\n",  sin(x) );
  printf("Test ASIN(x) = %f \t(0.365054)\n", asin(x) );
  printf("Test SINH(x) = %f \t(0.364632)\n", sinh(x) );
  printf("Test  COS(x) = %f \t(0.936949)\n",  cos(x) );
  printf("Test ACOS(x) = %f \t(1.205742)\n", acos(x) );
  printf("Test  TAN(x) = %f \t(0.372982)\n",  tan(x) );
  printf("Test ATAN(x) = %f \t(0.342897)\n", atan(x) );
  printf("Test  EXP(x) = %f \t(1.429036)\n",  exp(x) );
  printf("Test  LOG(x) = %f \t(-1.030019)\n",  log(x) );
}


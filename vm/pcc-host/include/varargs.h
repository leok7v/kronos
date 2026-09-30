#ifdef mari
#include </usr/include/varargs.h>
#else
/*  varargs.h -- header file for Kronos C Compiler by S.Siibak 22.dec.86 */

#define va_alloc_dcl  a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15, \
a16,a17,a18,a19,a20,a21,a22,a23,a24,a25,a26,a27,a28,a29,a30

#define va_alist va_alloc_dcl
typedef int *va_list;
#define va_dcl int va_alist;
#define va_start(list) list = &a0
#define va_end(list)
#define va_arg(list, mode)  ((mode)(* (list)++) )
#endif

#include "preCompiled.h"

#ifdef _DEBUG
void _trace(const char* fmt, ...)
{
//  int x = 0;
    char buf[1024*64];
    va_list vl;
    va_start(vl, fmt); 
    wvsprintf(buf, fmt, vl);
    ODS(buf);
}
#endif

#ifndef XDUTIME_INCLUDED
#define XDUTIME_INCLUDED

#include <windows.h>

unsigned long long time_kronos_to_windows(int ktime);
int windows_time_to_kronos(FILETIME* wtime);
void unpack_kronos_time(int ktime, int* year, int* month, int* day, int* hour, int* minute, int* second);

#endif
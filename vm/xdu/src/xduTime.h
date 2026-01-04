#ifndef XDUTIME_INCLUDED
#define XDUTIME_INCLUDED

unsigned long long time_kronos_to_windows(int ktime);
//int windows_time_to_kronos(FILETIME* wtime);
int pack_kronos_time(int year, int month, int day, int hour, int min, int sec);
void unpack_kronos_time(int ktime, int* year, int* month, int* day, int* hour, int* minute, int* second);

#endif
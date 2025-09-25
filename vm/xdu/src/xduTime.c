#include <Windows.h>
#include <stdio.h>
#include "xduTime.h"

const unsigned long long eraDiff = (unsigned long long)140618 * 24 * 3600; // seconds between 01.01.1600 and 01.01.1986

unsigned long long ktime2wtime(int ktime)
{
	return (eraDiff + (ULONGLONG)ktime) * 1000 * 10000;
}

void kt2wft(int ktime, FILETIME* res)
{
	ULARGE_INTEGER wtime;
	wtime.QuadPart = ktime2wtime(ktime);
	res->dwHighDateTime = wtime.HighPart;
	res->dwLowDateTime = wtime.LowPart;
}

void kt2wst(int ktime, SYSTEMTIME* res)
{
	FILETIME ftime;
	kt2wft(ktime, &ftime);
	FileTimeToSystemTime(&ftime, res);
}


void pKronosTime(int ktime)
{
	SYSTEMTIME st;
	kt2wst(ktime, &st);
	printf("%02d.%02d.%d %02d:%02d:%02d", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);
}


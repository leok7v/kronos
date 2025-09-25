#ifndef XDUTIME_INCLUDED
#define XDUTIME_INCLUDED

void xduTimeInit();

//void kt2wst(int ktime, SYSTEMTIME* res);
unsigned long long ktime2wtime(int ktime);
void pKronosTime(int ktime);


#endif
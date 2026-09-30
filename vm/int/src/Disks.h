//////////////////////////////////////////////////////////////////////////////
// Disks.h  Kronos disks subsystem
//
// Author:
//      Dmitry ("Leo") Kuznetsov        LeoK@myself.com
// Revision History
//      Jan 20, 1998 - originated       

#pragma once

#include <cstdio>

enum
{
  ready   = 0x01,
  floppy  = 0x02,
  wint    = 0x04,
  fmtsec  = 0x08,
  fmttrk  = 0x10,
  fmtunit = 0x20,
  wpro    = 0x40
};

struct Request
{
    int op;
    int drn;
    int res;
    int dmode;
    int dsecs; // device size in secs
    int ssc;   // 2**ssc = secsize
    int secsize;
    int cyls;
    int heads;
    int minsec;
    int maxsec;
    int ressec;  // reserved sectors (ice booter in 2.5)
    int precomp; // precompensation
    int rate; // heads stepping
};

using HANDLE = std::FILE*;

class DISKS
{
public:
    DISKS();
    virtual ~DISKS();
    
    bool AddDisk(const char* szFileName);
    int  GetCount();        // return active disks count
    bool Mount(int n);      // mount  disk n
    bool Dismount(int n);   // dismount disk n

    bool GetSize4KB(int n, int* adr);   // return disk size in 4KB blocks
    // both for 512 sectors, len in bytes
    bool Read (int diskno, int sector, byte* adr, int len);
    bool Write(int diskno, int sector, byte* adr, int len);
    bool GetSpecs(int n, Request* pRequest);
    bool SetSpecs(int n, Request* pRequest);
private:
    enum { N = 32 };
    HANDLE  fDisks[N];
    bool    bFloppy[N];
    int     nDiskCount;
    char*   fName[N];
    int     nMount[N]; // number of times this disk has been mounted
    int GetFloppySize4KB(int n, dword &SectorsPerCluster, 
                         dword &BytesPerSector, dword &TotalNumberOfClusters);
};

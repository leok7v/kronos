#include <cstdlib>
#include "preCompiled.h"
#include "Memory.h"


constexpr int MEM_COMMIT = 0x00001000;
constexpr int MEM_RESERVE = 0x00002000;
constexpr int MEM_RESET = 0x00080000;
constexpr int MEM_RELEASE = 0x00800000;
constexpr int PAGE_READWRITE = 0x0;

inline void* VirtualAlloc(void* dummy, size_t size, dword x, dword y) {
    // Windows VirtualAlloc(MEM_COMMIT) returns ZERO-initialized pages; malloc
    // does not.  The Kronos OS + C runtime rely on zeroed memory (BSS,
    // uninitialised globals, fresh heap) -- host-port 2026-07: use calloc so
    // the port matches Windows semantics (was `malloc`, giving intermittent
    // garbage-pointer memory-violation traps).
    return calloc(size, 1);
}

inline void VirtualFree(void* p, dword x, dword y) {
    free(p);
}


MEMORY::MEMORY(int nMemorySizeBytes) :
    data(null),
    bOutOfRange(false)
{
    nMemorySize = (nMemorySizeBytes + 3) / 4;
    
    assert(nMemorySize < IGD480bitmap + IGD480size);
    int nSizeWithIGD = IGD480bitmap + IGD480size;

    // allocate none commited memory
    byte* pReservered = null;
    pReservered = (byte*)::VirtualAlloc(null, nSizeWithIGD * 4, MEM_RESERVE, PAGE_READWRITE);

    data = (int*)::VirtualAlloc(pReservered, nMemorySize*4, MEM_COMMIT,  PAGE_READWRITE);
    assert(pReservered == (byte*)data);
//  trace("Memory: %08x\n", data);

    void* pIGDregisters = ::VirtualAlloc(pReservered + IGD480base*4, 4*K, MEM_COMMIT,  PAGE_READWRITE);
    assert(pIGDregisters == pReservered  + IGD480base*4);
    (void)pIGDregisters;

    void* pIGDbitmap = ::VirtualAlloc(pReservered + IGD480bitmap*4, IGD480size * 4, MEM_COMMIT,  PAGE_READWRITE);
    assert(pIGDbitmap == pReservered  + IGD480bitmap*4);
    (void)pIGDbitmap;
}


MEMORY::~MEMORY()
{
    // we do not necesseraly need VirtualFree(data) here
    if (data != null)
        ::VirtualFree(data, 0, MEM_RELEASE);
    data = null;
}


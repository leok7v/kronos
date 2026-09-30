//////////////////////////////////////////////////////////////////////////////
// Memory.h  (there is NO correspondent Memory.cpp)
//
// Author:
//      Dmitry ("Leo") Kuznetsov
// Revision History
//      Jan 18, 1998 - originated
//      Jan 18, 2001 - moved out inline functions bodies (VC6.0 bugs)
#pragma once

class IGD480;

enum
{
    IGD480base   = 0x1F0000,    // 8MB
    IGD480offset = 0x008000,    // bitmap offset
    IGD480bitmap = 0x1F8000,    // bitmap base
    IGD480size   = 512 * 512 / 8
};


class MEMORY
{
public: // IGD480 starts at 8MB. Don't make MemorySize 8MB!
    MEMORY(int nMemorySizeBytes);
    virtual ~MEMORY();

    inline int GetSize() const { return nMemorySize; }

    private: class reference; public:

    inline
    reference operator[](int n);
                                        
    inline
    bool OutOfRange() 
    { 
        bool bWasOutOfRange = bOutOfRange; 
        bOutOfRange = false; 
        return bWasOutOfRange; 
    }

private:
    class reference // see notes below
    {
        public:
            inline reference(int* ptr, MEMORY* m);
            inline void operator=(int i);
            inline void operator=(const reference& source);
            inline operator int() const;
            // addressing as in &mem[i]
            inline byte* operator&() const;
        private:
            int* p;
            MEMORY& mem;

    };
    friend class reference;
    friend class IGD480;
    int* data;
    int  nMemorySize;
    bool bOutOfRange;
public:
    int  lastBadAddr;
};


inline MEMORY::reference::reference(int* ptr, MEMORY* m) : 
    mem(*m),
    p(ptr)
{
}


inline
MEMORY::reference MEMORY::operator[](int n)
{
    n &= ~0xC0000000; // clear bits {31,30} Kronos feature
    if (n >= 0 && n < nMemorySize)
        return reference(&data[n], this);
    else if (n >= IGD480base && n <= IGD480base + IGD480offset + IGD480size)
    {
        return reference(&data[n], this);  // IGD address
    }
    else
    {
//      trace("MEMORY.OutOfRange 0x%08x\n", n);
        bOutOfRange = true;
        lastBadAddr = n;
        return reference(null, this);
    }
}


inline void MEMORY::reference::operator=(int i)
{
    if (p == null)
        return;
    *p = i;
}


inline void MEMORY::reference::operator=(const MEMORY::reference& source)
{
    if (p == null)
        return;
    *p = int(source);
}


inline MEMORY::reference::operator int() const
{
    if (p != null)
        return *p; 
    else
        return 0;
}


// addressing as in &mem[i]
inline byte* MEMORY::reference::operator&() const
{
    return (byte*)p;
}


//////////////////////////////////////////////////////////////////////////////
// You may or may not believe me but there is no trace of "reference"
// class in optimized code.
// I tried 2 alternative approaches: 1) SEH, 2) Read/Write functions
//  1)  SEH --  I do not want RTL to be present and inline assembler
//              does not allow me to define _except_link equ 0
//  2) Read/Write functions failed to inline in huge switch statment.
//
// If you are still skeptical here is the assembler code I found convincing:
//
//  void Test()
//  {
//      const int MemorySize = 1024*1024; // 1MW = 4MB
//      MEMORY mem(MemorySize*4);
//      for (int i = -1; i < MemorySize+2; i++)
//      {
//          mem[i] = mem[i] + 0x153;
//          if (mem.OutOfRange())
//              OutputDebugString("out of range\n");
//      }
//  }
//
// 
//63:   void Test()
//64:   {
//00401300   push        ebx
//00401301   push        ebp
//00401302   push        esi
//00401303   push        edi
//65:       const int MemorySize = 1024*1024; // 1MW = 4MB
//66:       MEMORY mem(MemorySize*4);
//00401304   push        4
//00401306   push        1000h
//0040130B   push        400004h
//00401310   push        0
//00401312   xor         bl,bl
//00401314   call        dword ptr [__imp__VirtualAlloc@16(0x004090e0)]
//0040131A   lea         edi,dword ptr [eax+400000h]
//67:       for (int i = -1; i < MemorySize+2; i++)
//00401320   or          esi,0FFh
//00401323   lea         ebp,dword ptr [eax-4]
//00401326   mov         dword ptr [edi],12345678h
//68:       {
//69:           mem[i] = mem[i] + 0x153;
//0040132C   test        esi,esi
//0040132E   jl          Test(0x0040133c)+3Ch
//00401330   cmp         esi,100000h
//00401336   jge         Test(0x0040133c)+3Ch
//00401338   mov         eax,ebp
//0040133A   jmp         Test(0x00401340)+40h
//0040133C   mov         bl,1
//0040133E   mov         eax,edi
//00401340   mov         ecx,dword ptr [eax]
//00401342   test        esi,esi
//00401344   jl          Test(0x00401352)+52h
//00401346   cmp         esi,100000h
//0040134C   jge         Test(0x00401352)+52h
//0040134E   mov         eax,ebp
//00401350   jmp         Test(0x00401356)+56h
//00401352   mov         bl,1
//00401354   mov         eax,edi
//00401356   add         ecx,153h
//0040135C   mov         dword ptr [eax],ecx
//70:           if (mem.OutOfRange())
//0040135E   mov         al,bl
//00401360   xor         bl,bl
//00401362   test        al,al
//00401364   je          Test(0x00401371)+71h
//71:               OutputDebugString("out of range\n");
//00401366   push        offset ??_C@_0O@CKPC@out?5of?5range?6?$AA@(0x00408038)
//0040136B   call        dword ptr [__imp__OutputDebugStringA@4(0x004090dc)]
//00401371   inc         esi
//00401372   add         ebp,4
//00401375   cmp         esi,100002h
//0040137B   jl          Test(0x0040132c)+2Ch
//72:       }
//73:   }
//0040137D   pop         edi
//0040137E   pop         esi
//0040137F   pop         ebp
//00401380   pop         ebx
//00401381   ret

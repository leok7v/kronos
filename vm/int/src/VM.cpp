
#include <cstdarg>
#include <ctime>
#include <iostream>

#include "preCompiled.h"
#include "Disks.h"
#include "Memory.h"
//#include "IGD480.h"
#include "VM.h"

// Rev. 0
// On Pentium 133MHz
// system reboots with 6 timer interrupt lost which totals to 6*20msec = 120msec
// Dry Stone shows:
//  00:00 sys ! time dry
//  Drystone time for 100,000 passes =  20.76
//  This machine benchmarks at 4816 drystones/second
//  time = 21.560 secs

ULONG ThreadProc(void* pParam)
{
//  static int nTotal = 0;
    VM* pVM = (VM*)pParam;
    while (pVM != NULL)
    {   // This one is going to time-out always
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (pVM->bTimer)
        {
//          static int nLost = 0;
//          trace("Timer ipts total %d lost %d\n", nTotal, ++nLost);
        }
        pVM->bTimer = true;
//      nTotal++;
    }
    return 0;
}


static dword lnFF[64]; // enough for width=1920


VM::VM(int nMemorySizeBytes, SioMouse* mouse, Console* con) : 
    mem(nMemorySizeBytes)
    //, igd(&mem, mouse, con)
{
    F = 0;
    G = 0;
    L = 0;
    S = 0;
    H = 0;
    P = 0;
    M = 0;
    sp = 0;
    PC = 0;
    IR = 0;
    PCs = 0;
    Ipt = 0;
    code = (byte*)&mem[0];
    memset(&AStack, 0, sizeof AStack);
    bTimer = false;
    hTimerThread = NULL;

    diskno = 0;
    
    dword id = 0;
    //hTimerThread = CreateThread(null,  0, ThreadProc, this, 0, &id);
    //SetThreadPriority(hTimerThread, THREAD_PRIORITY_TIME_CRITICAL);
    hTimerThread = std::make_unique<std::thread>(ThreadProc, this);
    memset(lnFF, 0xFFFFFFFF, sizeof lnFF);
}


VM::~VM()
{
    //TerminateThread(hTimerThread, 0);
    hTimerThread.release();
}


byte* VM::GetCode(int f)
{
    if (f < 0 || f > mem.GetSize())
        Ipt = 3;
    return (byte*)&mem[f % mem.GetSize()];
}


inline int VM::Next()
{
    return ((byte*)code)[PC++];
}


inline int VM::Next2()
{
    int pc = PC;
    PC += 2;
    return *((word*)&((byte*)code)[pc]);
}

inline int VM::Next4()
{
    int pc = PC;
    PC += 4;
    return *((int*)&((byte*)code)[pc]);
}

/* host-port debug: ring buffer of recent instructions, dumped on a 4Ch E-stack trap */
static int  _rbPC[64], _rbIR[64], _rbSP[64], _rbL[64], _rbS[64], _rbH[64], _rbG[64];
static long _rbN = 0;

inline
void VM::Push(int w)
{
    if (sp >= 0 && sp < AStackSize)
        AStack[sp++]=w;
    else
        Ipt = 0x4C;
}


inline 
int VM::Pop()
{
    if (sp > 0)
        return AStack[--sp];
    Ipt = 0x4C;
    return 0;
}


void VM::RestoreAStack()
{
    int i = mem[--S];
    if (i > AStackSize)
    {
        Ipt = 0x4C;
        i = AStackSize;
    }
    while (i-- > 0)
        Push(mem[--S]);
}


void VM::SaveAStack()
{
    int i = S;
    while (sp != 0) mem[S++] = Pop();
    mem[S] = S - i;
    S++;
}


void VM::Mark(int x, bool Extern)
{
    int i = S;
    mem[S++] = x;
    mem[S++] = L;
    if (Extern)
        mem[S] = PC | (1U << ExternalBit);
    else
        mem[S] = PC;
    S += 2; 
    L = i;
}


void VM::SaveRegisters()
{
    mem[1] = P;
    SaveAStack();
    mem[P + 0] = G;
    mem[P + 1] = L;
    mem[P + 2] = PC;
    mem[P + 3] = M;
    mem[P + 4] = S;
}


void VM::RestoreRegisters()
{
    mem[0] = P;
    G = mem[P];
    F = mem[G];
    code = GetCode(F);
    L  = mem[P+1];
    PC = mem[P+2];
    M  = mem[P+3];
    S  = mem[P+4];
    H  = mem[P+5];
    H -= AStackSize + 1;
    RestoreAStack();
}


void VM::Transfer(int p_to, int p_from)
{
//  trace("Transfer from %08X to %08X\n", P, mem[p_to]);
    int i = mem[p_to];
    mem[p_from] = P;
    SaveRegisters();
    P = i; 
    RestoreRegisters();
}


void VM::Trap(int no)
{
//  trace("Trap %02.2X\n", no);
//  xxx: (only for debuging emulator itself.
    #ifdef _DEBUG
        if (no == 7)
            bDebug = true;
    #endif

    if (no >= 0x3F)
    {
        mem[P + 6] = no;
        no = 0x3F;
    }
    if (no == 0)
    {
        Trap(6); 
        return;
    }
    if (no >= 0xC && no < 0x3F && (M & 0x1) == 0)
        return;
    if (no >= 2 && no < 0xC)
    {
        mem[P + 6] = no;
        if ((M & (1U << no)) == 0)
        {
            if (no != 3) // booter use Ipt 3 to determine memory size
            {
                trace("Unexpected interrupt %02x.\n", no);
                bDebug = true;
            }
            return;
        }
    }
    if (no == 1  && (M & 0x2) == 0)
        return;
    if (no == 0x3F && (M & (1U << 31)) == 0)
        return;
    Transfer(no*2, mem[no * 2 + 1]);
}


void VM::bitBlt(dword* dst, int dofs, dword* src, int sofs, int bits)
{
    assert(dofs >= 0 && dofs < 32);
    assert(sofs >= 0 && sofs < 32);
    assert(bits >= 0);

    if (bits == 0)
        return;

    if (sofs % 8 == 0 && dofs % 8 == 0 && bits % 8 == 0)
    {
        memcpy((byte*)dst + dofs / 8, (byte*)src + sofs / 8, bits / 8);
        return;
    }

    if (dofs == sofs && dofs != 0)
    {
        // copy first 32-ofs bits and advance
        int n = std::min(bits, 32 - dofs);
        dword mask = ((1U << n) - 1) << dofs;
        *dst &= ~mask;
        *dst |= *src & mask;
        bits -= n;
        if (bits <= 0)
            return;
        dofs = 0;
        sofs = 0;
        dst++;
        src++;
    }
    if (dofs == 0 && sofs == 0)
    {
        int n = bits >> 5;
        memcpy(dst, src, n * 4);
        int m = bits & 0x1F;
        if (m == 0)
            return;
        // copy last bits % 32
        dst += n;
        src += n;
        dword mask = (1U << m) - 1;
        *dst &= ~mask;
        *dst |= *src & mask;
        return;
    }

    int n = dofs - sofs;
    assert(n != 0);
    qword* pd = (qword*)dst;
    qword* ps = (qword*)src;
    while (bits >= 32)
    {
        qword mask = qword(0xFFFFFFFF) << dofs;
        qword s = *ps;
        s = n < 0 ? (s >> -n) : (s << n);
        *pd &= ~mask;
        *pd |= s & mask;
        pd = (qword*)(++dst);
        ps = (qword*)(++src);
        bits -= 32;
    }
    if (bits > 0)
    {
        qword mask = qword(((1U << bits) - 1)) << dofs;
        qword s = *ps;
        s = n < 0 ? (s >> -n) : (s << n);
        *pd &= ~mask;
        *pd |= s & mask;
    }   
}


void VM::BitBlt(dword dst, int dofs, dword src, int sofs, int bits)
{
    if (bits < 0 || dofs < 0 || sofs < 0)
    {
        Ipt = 0x4A; 
        return;
    }
    dword  test = mem[dst] 
                | mem[src]
                | mem[dst + ((dofs + bits + 31) >> 5)]
                | mem[src + ((sofs + bits + 31) >> 5)];
    unused(test);
    if (mem.OutOfRange())
    {
        Ipt = 3;
        return;
    }
    dword* pdst = (dword*)(const byte*)&mem[dst + (dofs >> 5)];
    dword* psrc = (dword*)(const byte*)&mem[src + (sofs >> 5)];
    bitBlt(pdst, dofs & 0x1F, psrc, sofs & 0x1F, bits);
}

static
void* memmove(void* _dst, void* _src, size_t count)
{
    byte* dst = (byte*)_dst;
    byte* src = (byte*)_src;
    if (dst <= src || dst >= (src + count)) // Non-Overlapping Buffers 
    {
        memcpy(dst, src, count);
        return dst;
    }         
    // Overlapping Buffers
    void* ret = dst;
    dst += count - 1;
    src += count - 1;
    while (count--)
        *dst-- = *src--;
    return ret;
}

void VM::BitMove(int dst, int _dofs, int src, int _sofs, int bits)
{
    dword  test = mem[dst] 
                | mem[src]
                | mem[dst + ((_dofs + bits + 31) >> 5)]
                | mem[src + ((_sofs + bits + 31) >> 5)];
    unused(test);
    if (mem.OutOfRange())
    {
        Ipt = 3;
        return;
    }
    dst = dst + (_dofs >> 5);
    src = src + (_sofs >> 5);
    int dofs = _dofs & 0x1F;
    int sofs = _sofs & 0x1F;
    qlong qdst = (qlong(dst) << 5) + _dofs;
    qlong qsrc = (qlong(src) << 5) + _sofs;

    if (qdst <= qsrc || qdst >= (qsrc + bits)) // Non-Overlapping Buffers 
    {
        BitBlt(dst, dofs, src, sofs, bits);
        return;
    }
    if ((sofs & 0x7) == 0 && (dofs & 0x7) == 0 && (bits & 0x7) == 0)
    {
        memmove((byte*)&mem[dst] + (dofs >> 3), (byte*)&mem[src] + (sofs >> 3), bits >> 3);
        return;
    }

    dofs += bits - 1;
    sofs += bits - 1;
    dword* pdst = (dword*)(byte*)&mem[dst];
    dword* psrc = (dword*)(byte*)&mem[src];
    for (int k = 0; k < bits; k++)
    {
        if (psrc[sofs >> 5] & (1U << (sofs & 0x1F)))
            pdst[dofs >> 5] |=  (1U << (dofs & 0x1F));
        else
            pdst[dofs >> 5] &= ~(1U << (dofs & 0x1F));
        sofs--;
        dofs--;
    }
}


void VM::_gbblt(int mode, void* des, int dofs, void* sou, int sofs, int sz)
{
    if (sz < 0 || dofs < 0 || sofs < 0)
    {
        Ipt = 0x4A;
        return;
    }
    dword* t_p = (dword*)des;
    dword* f_p = (dword*)sou;
    switch (mode % 4)
    {
        case Mode::REP: 
            while (sz > 0)
            {
                if ((1U << (sofs & 0x1F)) & f_p[sofs >> 5])
                    t_p[dofs >> 5] |= 1U << (dofs & 0x1F);
                else
                    t_p[dofs >> 5] &= ~(1U << (dofs & 0x1F));
                sofs++;  dofs++; sz--;
            }
            break;
        case Mode::XOR:
            while (sz > 0)
            {
                int s = ((1U << (sofs & 0x1F)) & f_p[sofs >> 5]) != 0;
                int d = ((1U << (dofs & 0x1F)) & t_p[dofs >> 5]) != 0;
                if (s ^ d)
                    t_p[dofs >> 5] |= 1U << (dofs & 0x1F);
                else
                    t_p[dofs >> 5] &= ~(1U << (dofs & 0x1F));
                sofs++;  dofs++; sz--;
            }
            break;
        case Mode::BIC:
            while (sz > 0)
            {
                int s = ((1U << (sofs & 0x1F)) & f_p[sofs >> 5]);
                if (s)
                    t_p[dofs >> 5] &= ~(1U << (dofs & 0x1F));
                sofs++;  dofs++; sz--;
            }
            break;
        case Mode::OR:
            while (sz > 0)
            {
                int s = ((1U << (sofs & 0x1F)) & f_p[sofs >> 5]) != 0;
                int d = ((1U << (dofs & 0x1F)) & t_p[dofs >> 5]) != 0;
                if (s | d)
                    t_p[dofs >> 5] |= 1U << (dofs & 0x1F);
                else
                    t_p[dofs >> 5] &= ~(1U << (dofs & 0x1F));
                sofs++;  dofs++; sz--;
            }
            break;
    }
}


dword VM::BBU(int adr, int i, int sz)
{
    qword q = *(qword*)(const byte*)&mem[adr + (i >> 5)];
    q = q >> (i & 0x1F);
    dword d = (dword)q;
    return d & ((1 << sz) - 1);
}


void VM::BBP(int adr, int i, int sz, int j)
{
    dword wmask = (1U << sz) - 1;
    qword  q = j & wmask;
    qword mask = wmask;
    q = q << (i & 0x1F);
    mask = mask << (i & 0x1F);
    qword* pq = (qword*)(const byte*)&mem[adr + (i >> 5)];
    *pq = (*pq & ~mask) | q;
/*
{
    char buf[33]; buf[32] = 0;
    for (int k = 0; k < 32; k++)
    {
        buf[k] = ((1U << k) & j) ? '1' : '0';
    }

    dword* p = (dword*)pq;
    char buf1[33]; buf1[32] = 0;
    for (k = 0; k < 32; k++)
    {
        buf1[k] = ((1U << k) & *p) ? '1' : '0';
    }

    p++;
    char buf2[33]; buf2[32] = 0;
    for (k = 0; k < 32; k++)
    {
        buf2[k] = ((1U << k) & *p) ? '1' : '0';
    }

    trace("bbp(%08x, %d, %d, %s) = %s %s\n", adr, i, sz, buf, buf1, buf2);
}
*/
}


static
int _idiv(qlong x, qlong y, int& rem)
{
    assert(y != 0);
    qlong z  = 0; 
    qlong bt = 1;
    while (std::abs(x) > std::abs(y))
    {
        bt = bt << 1; 
        y  = y << 1;
    };
    for (;;)
    {
        if ((x >= 0) == (y >= 0))
        {
            if (y < 0)
            {
                if (x <= y) { x = x - y;    z = z + bt; }
            }
            else
            {
                if (x >= y) { x = x - y;    z = z + bt; }
            }
        }
        else
        {
            if (y < 0)
            {
                if (x > 0) { x = x + y;     z = z - bt; }
            }
            else
            {
                if (x < 0) { x = x + y;     z = z - bt; }
            }
        }
        if (bt == 1)
            break;
        bt = bt >> 1;
        if (y > 0)
            y = (y & ~0x1) >> 1;
        else
            y = (y | 0x1) >> 1;
    }
    rem = int(x);
    return int(z);
}


static
int idiv(int x, int y)
{
    int rem = 0;
    return _idiv(x, y, rem);
}


static
int imod(int x, int y)
{
    int rem = 0;
    _idiv(x, y, rem);
    return rem;
}


/* xxx if we ever need it:
procedure iquot(x,y: integer): integer;
    var z,bt: integer;
begin
    z = 0; bt = 1;
    while abs(x)>abs(y) do
        bt = bt<<1; y = integer(bitset(y<<1)-{0});
    end;
    loop
        if x< == -abs(y) then
            if y<0 then x = x-y; z = z+bt else x = x+y; z = z-bt end;
        elsif x> == abs(y) then
            if y>0 then x = x-y; z = z+bt else x = x+y; z = z-bt end;
        end;
        if bt == 1 then exit end;
        bt = bt>>1;
        if y>0 then
            y = integer(bitset(y)-{0})>>1;
        else
            y = integer(bitset(y)+{0})>>1;
        end;
    end;
    rem = x;
    return z;
end iquot;


procedure irem(x,y: integer): integer;
begin
    x = iquot(x,y); return rem;
end irem;
*/


void VM::Run(bool init_bDebug)
{
    // let's don't eat 100% CPU:
    //::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

    int a = 0; // used for debug monitor only
    bDebug = init_bDebug;
    Ipt = 0;
    sp = 0;
    P = mem[1];
    RestoreRegisters();
    for(;;)
    {
        //std::cout << "PC: " << PC << std::endl;
        if (Ipt == 0)
        {
            if (mem.OutOfRange())
                Ipt = 3;
            else if (bTimer)
            {
                if ((M & 0x2) != 0)
                {
                    bTimer = false;
                    Ipt = 1; // timer ipt
                }
            }
            else if ((M & 0x1) != 0)
            {
                SIO *s = sios.inpReady();

                if (s != NULL)
                    Ipt = s->ipt();
                else
                {
                    s = sios.outReady();
                    if (s != NULL)
                        Ipt = s->ipt() + 1;
                }
            }
        }

        if (Ipt != 0)
        {
            if (Ipt == 0x4C || Ipt == 3 || Ipt == 0x49 || Ipt == 7) {
                fprintf(stderr, "\n=== TRAP dump (Ipt=0x%02X %s) badAddr=0x%08X (%d): last instrs ===\n",
                        Ipt, Ipt==3 ? "MEM-OUT-OF-RANGE" : "ESTACK", mem.lastBadAddr, mem.lastBadAddr);
                long _lo = _rbN>=64 ? _rbN-64 : 0;
                for (long _k=_lo; _k<_rbN; _k++)
                    fprintf(stderr, "  PC=%d IR=%02X sp=%d G=%d L=%d S=%d H=%d\n",
                            _rbPC[_k&63], _rbIR[_k&63], _rbSP[_k&63], _rbG[_k&63], _rbL[_k&63], _rbS[_k&63], _rbH[_k&63]);
            }
            Trap(Ipt);
            Ipt = 0;
        }
        if (bDebug)
        {
            if (!DebugMonitor(a))
                break;
        }
        PCs = PC;
        IR  = code[PC++];
        _rbPC[_rbN&63]=PCs; _rbIR[_rbN&63]=IR; _rbSP[_rbN&63]=sp;
        _rbL[_rbN&63]=L; _rbS[_rbN&63]=S; _rbH[_rbN&63]=H; _rbG[_rbN&63]=G; _rbN++;

#ifdef KTRACE
        { static long _n=0;
          if (_n < 60000)
            fprintf(stderr, "%ld PC=%d IR=%02X F=%d G=%d L=%d S=%d H=%d sp=%d\n",
                    _n, PCs, IR, F, G, L, S, H, sp);
          _n++; }
#endif
//      Sleep(0);
//      trace("PC = %08x IR = %02X\n", PC, IR);

        switch (IR)
        {
            case 0x0: case 0x1: case 0x2: case 0x3:
            case 0x4: case 0x5: case 0x6: case 0x7:
            case 0x8: case 0x9: case 0xA: case 0xB:
            case 0xC: case 0xD: case 0xE: case 0xF:
                    Push(IR & 0xF); break;

            case 0x10:  Push(Next());   break;
            case 0x11:  Push(Next2());  break;
            case 0x12:  Push(Next4());  break;
            case 0x13:  Push(Nil);      break;
            case 0x14:  Push(L+Next()); break;
            case 0x15:  Push(G+Next()); break;
            case 0x16:  AStack[sp-1] += Next(); break;
            case 0x17:  Push(mem[mem[G - Next() - 1]] + Next());    break;
            case 0x18:  if (Pop() == 0) PC += Next2();
                        else            PC += 2;
                        break;
            case 0x19:  PC += Next2();  break;
            case 0x1A:  if (Pop() == 0) PC += Next();
                        else            PC++;
                        break;
            case 0x1B:  PC += Next();   break;
            case 0x1C:  if (Pop() == 0) PC -= Next2();
                        else            PC += 2;
                        break;
            case 0x1D:  PC -= Next2();  break;
            case 0x1E:  if (Pop() == 0) PC -= Next();
                        else            PC++;
                        break;
            case 0x1F:  PC -= Next();   break;

            case 0x20:  Push(mem[L + Next()]);  break;
            case 0x21:  Push(mem[G + Next()]);  break;
            case 0x22:  Push(mem[mem[mem[G - Next() - 1]] + Next()]); break;
                        
            case 0x23:  Push(mem[Pop() + Next()]);  break;

            case 0x24:  case 0x25:  case 0x26:  case 0x27:
            case 0x28:  case 0x29:  case 0x2A:  case 0x2B:
            case 0x2C:  case 0x2D:  case 0x2E:  case 0x2F:
                        Push(mem[L + (IR & 0xF)]);
                        break;

            case 0x30:  mem[L + Next()] = Pop(); break;
            case 0x31:  mem[G + Next()] = Pop(); break;
            case 0x32:  mem[mem[mem[G - Next() - 1]] + Next()] = Pop(); break;
            case 0x33:  { int i = Pop(); mem[Pop() + Next()] = i; break; }

            case 0x34:  case 0x35:  case 0x36:  case 0x37:
            case 0x38:  case 0x39:  case 0x3A:  case 0x3B:
            case 0x3C:  case 0x3D:  case 0x3E:  case 0x3F:
                        mem[L + (IR & 0xF)] = Pop();
                        break;

            case 0x40:  {   int i = Pop(); 
                            int j = Pop();
                            int s = mem[j + i / 4]; 
                            Push(((byte*)&s)[i % 4]);
                            break;
                        }

            case 0x41:  Push(mem[Pop() + Pop()]);   break;

            case 0x42:  case 0x43:
            case 0x44:  case 0x45:  case 0x46:  case 0x47:
            case 0x48:  case 0x49:  case 0x4A:  case 0x4B:
            case 0x4C:  case 0x4D:  case 0x4E:  case 0x4F:
                        Push(mem[G + (IR & 0xF)]);
                        break;

            case 0x50:  
            {
                int k = Pop(); 
                int i = Pop(); 
                int j = Pop();
                int s = mem[j + i / 4]; 
                ((byte*)&s)[i % 4] = (byte)k;
                mem[j + i / 4] = s;
                break;
            }

            case 0x51:  { int i = Pop(); mem[Pop() + Pop()] = i; break; }

            case 0x52:  case 0x53:
            case 0x54:  case 0x55:  case 0x56:  case 0x57:
            case 0x58:  case 0x59:  case 0x5A:  case 0x5B:
            case 0x5C:  case 0x5D:  case 0x5E:  case 0x5F:
                        mem[G + (IR & 0xF)] = Pop();
                        break;

            case 0x60:  case 0x61:  case 0x62:  case 0x63:
            case 0x64:  case 0x65:  case 0x66:  case 0x67:
            case 0x68:  case 0x69:  case 0x6A:  case 0x6B:
            case 0x6C:  case 0x6D:  case 0x6E:  case 0x6F:
                        AStack[sp-1] = mem[AStack[sp-1] + (IR & 0xF)];
                        break;

            case 0x70:  case 0x71:  case 0x72:  case 0x73:
            case 0x74:  case 0x75:  case 0x76:  case 0x77:
            case 0x78:  case 0x79:  case 0x7A:  case 0x7B:
            case 0x7C:  case 0x7D:  case 0x7E:  case 0x7F:
            {
                int i = Pop(); mem[Pop() + (IR & 0xF)] = i; 
                break;
            }

            case 0x80: // I/O bus reset
                break;
            case 0x81: // QUIT Stop processor 
                bDebug = true;
                break;
            case 0x82: // GETM Get Mask
                Push(M);
                break;
            case 0x83: // SETM Set Mask
                M = Pop();
                break;
            case 0x84: // TRAP interrupt simulation
                Ipt = Pop();
                break;
            case 0x85: // TRA  Transfer control between processes
            {
                int i = Pop(); Transfer(i, Pop()); 
                break;
            }
            case 0x86: // TR    Test & Reset
            {
                int i = Pop(); Push(mem[i]); mem[i] = 0;
                break;
            }

            case 0x87:  // IDLE
            {
                PC--;
                Sleep(1);
                // no enabled interrupts => infinite idle
                // dsu -p uses this to shutdown computer.
                if (M == 0)
                {
                    //igd.shutdown();
                    return;
                }
                break;
            }
            
            case 0x88: // ADD
                if (sp <= 1) Ipt = 0x4C;
                else { sp--; AStack[sp-1] += AStack[sp]; }
                break;
            
            case 0x89: // sub
                if (sp <= 1) Ipt = 0x4C;
                else { sp--; AStack[sp-1] -= AStack[sp]; }
                break;
            
            case 0x8A: // mul
                if (sp <= 1) Ipt = 0x4C;
                else { sp--; AStack[sp-1] *= AStack[sp]; }
                break;
            
            case 0x8B: // div
                if (sp <= 1) 
                    Ipt = 0x4C;
                else if (AStack[sp-1] == 0)
                {
                    Ipt = 0x41; sp--; AStack[sp-1] = 0;
                }
                else 
                { 
                    sp--; AStack[sp-1] = idiv(AStack[sp-1], AStack[sp]);
                }
                break;

            case 0x8C: // SHL  integer SHift Left 
            { 
                int i = Pop() & 0x1F; Push(Pop() << i); break; 
            }

            case 0x8D: // SHR  integer SHift Right 
            {
                int i = Pop() & 0x1F; Push(Pop() >> i); break;
            }

            case 0x8E: // ROL  word ROtate Left  
            {   dword i = dword(Pop()) & 0x1F;
                if (i != 0)
                {
                    dword j = (dword)Pop();
                    Push((j << i) | (j >> (32-i)));
                }
                break;
            }

            case 0x8F: // ROR  word ROtate Right 
            {   
                dword i = dword(Pop()) & 0x1F;
                if (i != 0)
                {
                    dword j = (dword)Pop();
                    Push((j >> i) | (j << (32-i)));
                }
                break;
            }

            case 0x90:  case 0x91:  case 0x92:  case 0x93: case 0x94:   // io0..4
                    IO(IR & 0xF); 
                    break;

            case 0x95: // rcmp A.K.A. ARRCMP array compare
            {
                int sz = Pop(); 
                int adr = Pop(); 
                int adr1 = Pop();
                if (sz < 0) 
                {
                    Push(sz); Ipt = 0x4F;
                }
                else if (sz == 0)
                {
                    Push(adr1);
                    Push(adr1);
                }
                else
                {
                    for (;;)
                    {
                        if (mem[adr] != mem[adr1] || sz ==1)
                        {
                            Push(adr1); Push(adr); break;
                        }
                        sz--;
                        adr++;
                        adr1++;
                    }
                }
                break;
            }

            case 0x96: // wmv A.K.A. WM     word move
            {
                int sz = Pop(); 
                int f  = Pop(); 
                int t  = Pop();
                if (t > f)
                {
                    t = t + sz - 1; 
                    f = f + sz - 1;
                    while (sz > 0)
                    {
                        mem[t] = mem[f]; 
                        t--; f--; sz--;
                    }
                }
                else if (sz > 0)
                {
                    memcpy((byte*)&mem[t], (byte*)&mem[f], sz*4);
                }
                break;
            }

            case 0x97:  // BMV
            {
                int sz = Pop();
                int i = Pop(); int j = Pop();
                int a = Pop(); int b = Pop();
                BitMove(b, a, j, i, sz);
                break;
            }

            case 0x98:  case 0x99:  case 0x9A:  case 0x9B:
            case 0x9C:  case 0x9D:  case 0x9E:  case 0x9F:
                FPU();
                break;

            case 0xA0: // LSS  int LeSS 
                if (sp <= 1) Ipt = 0x4C;
                else { sp--; AStack[sp-1] = AStack[sp-1] < AStack[sp]; }
                break;

            case 0xA1:  // LEQ  int Less or EQual
                if (sp <= 1) 
                    Ipt = 0x4C;
                else 
                { 
                    sp--; 
                    AStack[sp-1] = AStack[sp-1] <= AStack[sp]; 
                }
                break;

            case 0xA2: // GTR  int Greater or EQual
                if (sp <= 1) 
                    Ipt = 0x4C;
                else
                {
                    sp--; 
                    AStack[sp-1] = AStack[sp-1] > AStack[sp]; 
                }
                break;

            case 0xA3:  // GEQ  int Greater or EQual
                if (sp <= 1)
                    Ipt = 0x4C;
                else
                {
                    sp--; 
                    AStack[sp-1] = AStack[sp-1] >= AStack[sp];
                }
                break;

            case 0xA4: // EQU  int EQUal    
                if (sp <= 1)
                    Ipt = 0x4C;
                else
                {
                    sp--;
                    AStack[sp-1] = AStack[sp-1] == AStack[sp];
                }
                break;

            case 0xA5:  // NEQ  int Not EQual 
                if (sp <= 1)
                    Ipt = 0x4C;
                else
                {
                    sp--; AStack[sp-1] = AStack[sp-1] != AStack[sp];
                }
                break;

            case 0xA6:  // ABS  int ABSolute value 
            {
                int i = Pop(); 
                Push(i < 0 ? -i : i); 
                break;
            }
            case 0xA7:  Push(-Pop()); break;
            case 0xA8:  Push(Pop() | Pop()); break;
            case 0xA9:  Push(Pop() & Pop()); break;
            case 0xAA:  Push(Pop() ^ Pop()); break;
            case 0xAB:  
            { 
                int i = Pop(); 
                Push(Pop() & ~i);
                break; 
            }
            case 0xAC:  // IN   membership to bitset 
            {   int i = Pop(); 
                int j = Pop(); 
                Push(j >= 0 && j < 32 ? ((1U << j) & i) != 0 : 0);
                break; 
            }
            case 0xAD:  // BIT  setBIT 
            {
                int i = Pop(); 
                if (i < 0 || i >= 32)
                    Ipt = 0x4A;
                else
                    Push(1U << i); 
                break;
            }
            case 0xAE:  // NOT  boolean NOT (not bit per bit!) 
                Push(Pop() == 0); 
                break;
            case 0xAF:  // MOD  integer MODulo
            {
                if (sp <= 1) 
                    Ipt = 0x4C;
                else if (AStack[sp-1] == 0)
                {
                    Ipt = 0x41; sp--; AStack[sp-1] = 0;
                }
                else 
                { 
                    sp--; AStack[sp-1] = imod(AStack[sp-1], AStack[sp]);
                }
                break;
            }

            case 0xB0:  // DECS  DECriment S register (reverse to ALLOC) 
            {
                S -= Pop(); 
                break;
            }
            
            case 0xB1: // DROP
                Pop(); 
                break;
            
            case 0xB2: // LODF  reLOaD expr. stack after Function return
            {
                int i = Pop();
                RestoreAStack();
                Push(i);
                break;
            }
            
            case 0xB3: // STORE STORE expr. stack before function call
                if (S + 8 > H)
                {
                    PC--; Ipt = 0x40;
                } 
                else
                    SaveAStack();
                break;
            
            case 0xB4:  // STOFV STOre expr. stack with Formal function Value
                        // on top before function call (see: CF)
                if (S + 8 > H) { PC--; Ipt = 0x40; } 
                else
                {
                    int i = Pop();
                    SaveAStack();
                    mem[S++] = i;
                }
                break;
            
            case 0xB5: // COPT  COPy Top of expr. stack
            {
                int i = Pop();
                Push(i);
                Push(i);
                break;
            }

            case 0xB6:  // CPCOP Character array Parameter COPy
            {
                int i = Pop();
                int j = i / 4 + 1;
                if (j > H - S) { Push(i); PC--; Ipt = 0x40; }
                else if (j < 0)
                    Ipt = 0x4A;
                else
                {
                    mem[L + Next()] = S; 
                    i = Pop();
                    while (j-- > 0)
                        mem[S++] = mem[i++];
                }
                break;
            }

            case 0xB7:  // PCOP  structure Parameter allocate and COPy
            {
                int i = Pop(); 
                int j = i + 1;
                if (j > H - S) { Push(i); PC--; Ipt = 0x40; }
                else if (j < 0)
                    Ipt = 0x4A;
                else
                {
                    mem[L + Next()] = S; 
                    i = Pop();
                    while (j-- > 0)
                        mem[S++] = mem[i++];
                }
                break;
            }

            case 0xB8: // FOR1  enter  FOR statment
            {
                if (S + 2 > H) { PC--; Ipt = 0x40; }
                else
                {
                    int i = Next(); 
                    int hi = Pop(); 
                    int low = Pop(); 
                    int adr = Pop();
                    int j = Next2() + PC;
                    if (i == 0 && low <= hi || i != 0 && low >= hi)
                    {
                        mem[adr] = low;
                        mem[S++] = adr;
                        mem[S++] = hi;
                    }
                    else
                        PC = j;
                }
                break;
            }

            case 0xB9: // FOR2  end of FOR statment
            {
                int hi  = mem[S-1]; 
                int adr = mem[S-2];
                int sz  = Next(); 
                int j   = -Next2() + PC;
                if (0x80 & sz) 
                    sz -= 256;
                int i = mem[adr]; 
                i += sz;
                if (sz >=0 && i > hi || sz <= 0 && i < hi)
                    S -= 2;
                else
                {
                    mem[adr] = i; 
                    PC = j;
                }
                break;
            }

            case 0xBA: // ENTC Enter CASE
            {
                if (S + 1 > H)
                {
                    PC--; Ipt = 0x40;
                }
                else
                {
                    PC += Next2();
                    int j = Pop();
                    int low = Next2();
                    int hi = Next2();
                    int i = PC + 2 * (hi - low) + 4;
                    mem[S++] = i;
                    if (j >= low && j <= hi) PC += (j-low+1)*2;
                    PC -= Next2();
                }
                break;
            }

            case 0xBB:  // XIT  eXIT from case or control structure 
                S--; 
                PC = mem[S]; 
                break;

            case 0xBC:  // ADDPC  add to program counter 
                Push(Pop() + PC);
                break;

            case 0xBD: // JMP
                PC = Pop(); 
                break;
            
            case 0xBE: // ORJP   short circuit OR  JumP 
                if (Pop() != 0)
                {
                    Push(1);
                    PC = Next() + PC;
                }
                else
                    PC++;
                break;
            
            case 0xBF: // ANDJP  short circuit AND JumP 
                if (Pop() == 0)
                {
                    Push(0);
                    PC = Next() + PC;
                }
                else
                    PC++;
                break;

            case 0xC0: // MOVE   MOVE block
            {
                int sz = Pop();
                int j = Pop() & ~0xC0000000; // -{30,31}
                int i = Pop() & ~0xC0000000; // -{30,31}
                if (sz < 0)
                    Ipt = 0x4A;
                else
                {
                    while (sz > 0 && Ipt != 3)
                    {
                        mem[i++] = mem[j++];
                        sz--;
                        if (mem.OutOfRange())
                            Ipt = 3;
                    }
                }
                break;
            }

            case 0xC1: // CHKNIL check address for NIL
            {   
                int i = AStack[sp-1];
                if (i == Nil) 
                    Ipt = 3; // original doc says: 0x41 - I think 3 is better
                break;
            }

            case 0xC2: // LSTA  Load STring Address
                Push(mem[G + 1] + Next2());
                break;

            case 0xC3: // COMP  COMPare strings
            {
                int i = Pop();
                int j = Pop();
                byte* pa = (byte*)&mem[i];
                byte* pb = (byte*)&mem[j];
                byte a = *pa++;
                byte b = *pb++;
                while (a == b && b != 0 && a != 0)
                {
                    a = *pa++;
                    b = *pb++;
                }
                Push(b); Push(a); // bug in docs!!!
                break;
            }

            case 0xC4: // GB  Get procedure Base n level down
            {
                int i = L;
                int j = Next();
                while (j-- > 0) 
                    i = mem[i];
                Push(i);
                break;
            }

            case 0xC5: // GB1
                Push(mem[L]);
                break;

            case 0xC6: // CHK  array boundary CHecK 
                if (sp < 3)
                    Ipt = 0x4C;
                else
                {
                    int i = AStack[sp-3];
                    if (i < AStack[sp-2] || i > AStack[sp-1])
                        Ipt=0x4A;
                    else
                        sp -= 2;
                }
                break;

            case 0xC7: // CHKZ  array boundary CHecK (low=Zero)
                if (sp < 2)
                    Ipt = 0x4C;
                else
                {
                    int i = AStack[sp-2];
                    if (i < 0 || i > AStack[sp-1]) 
                        Ipt=0x4A;
                    else sp--;
                }
                break;

            case 0xC8: // ALLOC ALLOCate block
            {
                int sz = Pop();
                if ( S + sz > H) { Push(sz); PC--; Ipt = 0x40; }
                else { Push(S); S += sz; }
                break;
            }

            case 0xC9: // ENTR  ENTeR procedure 
            {
                int sz = Next();
                if (S + sz > H)
                {
                    PC -= 2; Ipt = 0x40;
                }
                else
                    S += sz;
                break;
            }

            case 0xCA: // RTN   ReTurN from procedure
            {
                S = L;
                L = mem[S + 1]; 
                int i = mem[S + 2];
                PC = i & 0xFFFF;
                if ((1U << ExternalBit) & i) 
                {
                    G = mem[S]; 
                    F = mem[G]; 
                    code = GetCode(F);
                }
                break;
            }

            case 0xCB: // NOP
                break;

            case 0xCC: // CX    Call eXternal 
                if (S + 4 > H)
                {
                    PC--;  Ipt = 0x40;
                }
                else
                {
                    int k = mem[G - Next() - 1];
                    int j = k & 0x3FFFFF; // *{0..21}
                    int i = Next(); 
                    Mark(G, true);
                    G = mem[j];
                    F = mem[G]; 
                    code = GetCode(F);
                    PC = mem[F+i];
                }
                break;

            case 0xCD: // CI    Call procedure at Intermediate level
                if (S + 4 > H)
                {
                    PC--; Ipt = 0x40;
                }
                else { int i = Next(); Mark(Pop(), false); PC = mem[F+i]; }
                break;

            case 0xCE: // CF    Call Formal procedure
                if (S + 3 > H)
                {
                    PC--; Ipt = 0x40;
                }
                else
                {
                    S--;
                    int i = mem[S];
                    Mark(G, true);
                    int j = ((byte*)&i)[3];
                    i = i & 0xFFFFFF; // *{0..23};
                    G = mem[i];
                    F = mem[G];
                    code = GetCode(F);
                    PC = mem[F + j];
                }
                break;

            case 0xCF: // CL    Call Local procedure
                if (S + 4 > H)
                {
                    PC--; Ipt = 0x40;
                }
                else
                {
                    int i = Next(); Mark(L, false); PC = mem[F + i];
                };
                break;

            case 0xD0:  case 0xD1:  case 0xD2:  case 0xD3:
            case 0xD4:  case 0xD5:  case 0xD6:  case 0xD7:
            case 0xD8:  case 0xD9:  case 0xDA:  case 0xDB:
            case 0xDC:  case 0xDD:  case 0xDE:  case 0xDF:
                if (S + 4 > H)
                {
                    PC--;
                    Ipt = 0x40;
                }
                else
                {
                    Mark(L, false); 
                    PC = mem[F + (IR & 0xF)];
                }
                break;

            case 0xE0:  // INCL
            {
                int i = Pop();
                int j = Pop() + (i >> 5);
                i = i & 0x1F;
                mem[j] = mem[j] | (1U << i);
                break;
            }

            case 0xE1:  // EXCL
            {   
                int i = Pop();
                int j = Pop() + (i >> 5);
                i = i & 0x1F;
                mem[j] = mem[j] & ~(1U << i);
                break;
            }
            
            case 0xE2:  // INL  membership IN Long set
            {
                int k = Pop();
                int j = Pop();
                int i = Pop();
                if (i < 0 || i >= k)
                    Push(0);
                else
                    Push( ((1U << (i & 0x1F)) & mem[j + (i >> 5)]) != 0 );
                break;
            }

            case 0xE3:  // QUOT
            {
                Quote(Next());
                break;
            }


            case 0xE4: // INC1  INCrement by 1
            {   int i = Pop(); mem[i] = mem[i] + 1;
                break;
            }

            case 0xE5: // DEC1  DECrement by 1
            {   int i = Pop(); mem[i] = mem[i] - 1;
                break;
            }

            case 0xE6: // INC   INCrement
            {   int i = Pop(); int j = Pop(); mem[j] = mem[j] + i;
                break;
            }

            case 0xE7: // DEC   DECrement
            {
                int i = Pop(); int j = Pop(); mem[j] = mem[j] - i;
                break;
            }

            case 0xE8: // STOT  STOre Top on proc stack
                if (S + 1 > H)
                {
                    PC--; Ipt = 0x40;
                }
                else 
                    mem[S++] = Pop();
                break;

            case 0xE9: // LODT  LOaD   Top of proc stack
                Push(mem[--S]); 
                break;

            case 0xEA: // LXA   Load indeXed Address
            {
                int sz = Pop();
                int i = Pop();
                Push(Pop() + i * sz);
                break;
            }

            case 0xEB:  // LPC   Load Procedure Constant
            {   
                int i = Next(); 
                int j = Next(); 
                i = mem[G - i - 1]; 
                ((byte*)&i)[3] = (byte)j; 
                Push(i);
                break;
            }

            case 0xEC: // BBU  Bit Block Unpack
            {
                int sz = Pop();
                if (sz < 1 || sz > 32)
                {
                    Push(sz); 
                    PC--; 
                    Ipt = 0x4A;
                }
                int i = Pop(); 
                int adr = Pop();
                Push(BBU(adr, i, sz));
                break;
            }

            case 0xED: // BBP  Bit Block Pack
            {
                int j  = Pop(); 
                int sz = Pop();
                if (sz < 1 || sz > 32)
                {
                    Push(sz); 
                    PC--; 
                    Ipt = 0x4A;
                }
                int i = Pop(); 
                int adr = Pop();
                BBP(adr, i, sz, j);
                break;
            }

            case 0xEE: // BBLT Bit BLock Transfer
            {
                int sz = Pop();
                int i = Pop(); 
                int j = Pop();
                int a = Pop(); 
                int b = Pop();
                BitBlt(b, a, j, i, sz);
                break;
            }

            case 0xEF: // PDX Prepare Dynamic indeX 
            {
                int i = Pop(); /* index */
                int j = Pop(); /* desc. address */
                int k = mem[j];  /* address */
                j = mem[j + 1]; /* length  */
                Push(k);
                Push(i);
                if (i < 0 || i > j)
                    Ipt = 0x4A;
                break;
            }

            case 0xF0: // SWAP
            {   int i = Pop(); int j = Pop(); Push(i); Push(j); 
                break;
            }

            case 0xF1: // LPA Load Parameter Address
                Push(L - Next() - 1);
                break;

            case 0xF2: // LPW Load Parameter WORD
                Push(mem[L - Next() - 1]);
                break;

            case 0xF3: // SPW Store Parameter WORD
                mem[L - Next() - 1] = Pop();
                break;

            case 0xF4: // SSWU Store Stack Word Undestructive
            {   int i = Pop(); 
                mem[Pop()] = i; Push(i);
                break;
            }

            case 0xF5: // RCHK  range CHecK 
            {
                int i = Pop(); int j = Pop(); int k = Pop();
                if (k >= j && k <= i) 
                    Push(1); 
                else 
                    Push(0);
                break;
            }

            case 0xF6: // RCHZ range check (low=Zero)
            {   
                int i = Pop(); int k = Pop();
                if (k >= 0 && k <= i) Push(1); else Push(0);
                break;
            }


            case 0xF7: // CM Call procedure from dynamic Module
            {
                if (S + 4 <= H)
                {
                    int i = Next();
                    S--; 
                    int j = mem[S];
                    Mark(G, true);
                    G = j; 
                    F = mem[G]; 
                    PC = mem[F + i];
                }
                else
                {
                    PC--; 
                    Ipt = 0x40;
                }
                break;
            }

            case 0xF8: // CHKBX  CHecK BoX
            {
                int p0 = Pop(); 
                int p1 = Pop();
                int x00 =  mem[p0]      % 0x10000;
                int y00 = (mem[p0]<<16) % 0x10000;
                p0++;
                int x01 =  mem[p0]      % 0x10000;
                int y01 = (mem[p0]<<16) % 0x10000;
                int x10 =  mem[p1]      % 0x10000;
                int y10 = (mem[p1]<<16) % 0x10000;
                p1++;
                int x11 =  mem[p1]      % 0x10000;
                int y11 = (mem[p1]<<16) % 0x10000;
                Push(x10 <= x01 && y10 <= y01 && x00 <= x11 && y00 <= y11);
                break;
            }

            case 0xF9:  // bmg
                BMG(Next());
                break;

            case 0xFA:  // active
                Push(P); 
                break;


            case 0xFB: // USR User defined functions
            {
                int op = Next();
                switch (op)
                {
                    case 0: 
                    {
                        // str: ARRAY OF CHAR
                        int len = Pop();
                        unused(len);
                        const byte* psz = (const byte*)&mem[Pop()];
                        printf("%s\n", psz);
                        trace("%s\n", psz);
                        break;
                    }
                    default:
                        Ipt = 0x7;
                        break;
                }
                break;
            }

            case 0xFC:  
                switch (Next())
                {
                    case 0x0: // cpu vers. */ 
                              Push(7); break;
                    case 0x1: printf("\n%08X\n", Pop()); break;
                    case 0x2: // microcode vers.
                              Push(2); break;
                    default:
                        PC--; Ipt = 7;
                }
                break;

            case 0xFD: // NII Never Implemented Instruction
                Ipt = 0x7;
                break;

            case 0xFE: 
            {
                int i = Pop();
                printf("%08X\n", i);
                trace("%08X\n", i);
                break;
            }

            case 0xFF:
                Ipt = 0x49;
                break;

            default:
                Ipt = 0x7;
                break;
        }
        if (Ipt == 0 && S > H || S == 0)
        {
            // WTF??...
            //_asm int 3
            __asm__("int3");
        }
    }
    SaveRegisters();
}


void VM::IO(int no)
{
    switch (no)
    {
        case 0: // INP
        {
//          trace("io 0x90\n");
            int adr = Pop();
            int ioAddr = adr & 0xFFC;

            SIO *s = sios.find(ioAddr);

            if (s != NULL)
                Push(s->inp(adr));
            else
            {
                trace("INP %03X ???\n", adr);
                Ipt = 3; Push(0);
            }
            break;
        }

        case 1: // OUT
        {
//          trace("io 0x91\n");
            int i = Pop(); 
            int adr = Pop();
            int ioAddr = adr & 0xFFC;

            SIO *s = sios.find(ioAddr);

            if (s != NULL)
                s->out(adr, i);
            else
            {
                trace("OUT %03X %08X ???\n", adr, i); 
                Ipt = 3;
            }
            break;
        }

        case 0x2: // 0x92 io2  -- "new" disk subsystem
        {
//              trace("io 0x92\n");
                int len = Pop();    // bytes
                int adr = Pop();    // address
                int sec = Pop();    // sector
                int dsk = Pop();    // disk
                int op  = Pop();    // operation
                Push(DiskOperation(op, dsk, sec, adr, len));
                break;
        }

        case 3:
            {
                printf("%c", Pop());
                break;
            }

        case 0x4:
            {
                trace("io 0x94\n");
                Ipt = 7;  PC -= 2;
                break;
            }
        default:
            {
                trace("unsupported i/o function %03X\n", no);
                Ipt = 7;  PC -= 2;
                break;
            }
    }
}


void VM::Quote(int op)
{
    // X MOD N = X - (X QOU N) * N
    switch (op)
    {
        case 0: // SHRQ ??? (not tested) probably used by Portable C Compiler only
                if (sp <= 1) 
                    Ipt = 0x4C;
                else
                {
                    sp--; AStack[sp-1] = dword(AStack[sp-1]) >> dword(AStack[sp]);
                }
                break;
        case 1: // QUOT
                if (sp <= 1) 
                    Ipt = 0x4C;
                else
                {
                    sp--; AStack[sp-1] /= AStack[sp]; 
                }
                break;
        case 2: // ANDQ ??? (not tested) probably used by Portable C Compiler only
                if (sp <= 1) 
                    Ipt = 0x4C;
                else
                {
                    sp--; AStack[sp-1] &= ((1 << AStack[sp]) - 1); 
                }
                break;
        case 3: // REM
                if (sp <= 1) 
                    Ipt = 0x4C;
                else
                {
                    sp--; AStack[sp-1] %= AStack[sp]; 
                }
                break;
        default:
            Ipt = 7;  
            PC -= 2;
            break;
    }
}



void VM::FPU()
{
    typedef struct { union {int i; float f;} u; } fi;
    fi x, y;
    x.u.f = 0;
    y.u.f = 0;
    switch (IR)
    {
        case 0x98:  case 0x99:  case 0x9A:  case 0x9B:  case 0x9C:
            y.u.i = Pop();
            x.u.i = Pop();
            break;
        case 0x9D:  case 0x9E:  
            x.u.i = Pop();
            break;
    }
    switch (IR)
    {
        case 0x98:  x.u.f = x.u.f + y.u.f; Push(x.u.i); break;
        case 0x99:  x.u.f = x.u.f - y.u.f; Push(x.u.i); break;
        case 0x9A:  x.u.f = x.u.f * y.u.f; Push(x.u.i); break;
        case 0x9B:  x.u.f = x.u.f / y.u.f; Push(x.u.i); break;
        case 0x9C:  
            if      (x.u.f > y.u.f) { Push(1); Push(0); }
            else if (x.u.f < y.u.f) { Push(0); Push(1); }
            else { Push(0); Push(0); }
            break;

        case 0x9D:  
            if (x.u.f < 0) 
                x.u.f = -x.u.f;
            Push(x.u.i); 
            break;
        case 0x9E:  
            x.u.f = -x.u.f;
            Push(x.u.i); 
            break;
        case 0x9F:
        {
            switch (Next())
            {
                case 0x0:   x.u.f = (float)Pop(); Push(x.u.i); break;
                case 0x1:   x.u.i = Pop(); Push((int)x.u.f); break;
                default:    Ipt = 7; PC--; break;
            }
        }
    }
}


void VM::digits(int& a, char ch)
{
    int v = 0;
    for (;;)
    {
        if (ch >= '0' && ch <= '9')
        {
            printf("%c",ch);
            v = v << 4 | (ch - '0');
        }
        else if (ch >= 'a' && ch <= 'f')
        {
            printf("%c", ch);
            v = v << 4 | (ch - 'a' + 10);
        }
        else if (ch == '/')
        {
            a = v; 
            printf(" /\n\n");
            return;
        }
        else if (ch == '=')
        {
            mem[a] = v;
            printf("\n");
            return;
        }
        else 
        {
            printf(" ???\n"); 
            return;
        }
        do
        {
            Sleep(100);
            ch = char(busyRead());
        }
        while (ch == 0);
    }
}


void VM::ShowRegisters()
{
//  mCode.VisCommand(IR, cmd0);
//  mCode.VisCommand(code[PC], cmd);
//  i = mCode.CmdLen[code^[PC]];
    printf("P=%08X L=%08X S=%08X H=%08X G=%08X\n", P, L, S, H, G);
    printf("F=%08X PC=%08X IR=%02X (PC'=%08X)\nM=%08X", F, PC, IR, PCs, M);
//  FOR i = 1 TO i DO print('%02.2X ',code^[PC+i]) END;
    printf("\nA-Stack: ");
    if (sp == 0)
        printf("empty\n");
    else
    {
        printf("[%d] ", sp);
        for (int i = 0; i < sp; i++)
            printf("%08X ", AStack[i]);
    }
}


bool VM::DebugMonitor(int& a)
{
    ShowRegisters();
    for(;;)
    {
        printf("%08X %08X ", a, (int)mem[a]);
        char ch = char(busyRead());
        while (ch == 0)
            ch = char(busyRead());
        if      (ch == 'g') 
        {
            bDebug = false; 
            printf("\nbDebug OFF\n");
            break;
        }
        else if (ch == 'i') 
        {
            bDebug = true; 
            printf("\nbDebug ON\n");
        }
        else if (ch == '\r')
        {
            a++; 
            printf("\n");
        }
        else if (ch=='^')
        {
            if (a > 0) a--;
            printf("^\n");
        }
        else if (ch == 3) // ctrl+C
        {
            printf("\n");
            return false;
        }
        else if (ch == 'p')
            break;
        else if (ch == '/')
        {
            printf(" /\n\n");
            a = mem[a];
        }
        else if (ch >= '0' && ch <= '9' || ch >= 'a' && ch <= 'f') 
            digits(a, ch);
        else
            printf("%c ???\n", ch);
    }
    return true;
}


void VM::setConsole(SIO *ps)
{
    con = ps;
}


void VM::printf(const char* fmt, ...)
{
    char buf[1024];
    va_list vl;
    va_start(vl, fmt); 
    vsprintf(buf, fmt, vl);

    con->write(buf, strlen(buf));
}

int VM::busyRead()
{
    return con->busyRead();
}


int VM::DiskOperation(int op, int dsk, int sec, int adr, int len)
{
    switch (op)
    {
        case 1: return (int)Disks.Mount(dsk);
        case 2: return (int)Disks.Dismount(dsk);
        case 3: return Disks.GetSize4KB(dsk, (int*)&mem[adr]);
        case 4: return Disks.Read (dsk, sec, &mem[adr], len);
        case 5: return Disks.Write(dsk, sec, &mem[adr], len);
        case 6: 
                {
                    std::time_t t = std::time(0);   // get time now
                    std::tm* now = std::localtime(&t);
                    // Year is pinned to 2017: the SHIPPED xd1 Time.pack rejects anything later
                    // (see sd_disk_controller.vhdl RTC_YEAR), so a real full year (2026) would be
                    // refused and the clock would fall back to 1986 -- too early for mx's ".sym not
                    // from the future" check (the OS .sym are dated ~2009). Month/day/time run live.
                    mem[adr++] = 2017;                // st.wYear  (clamped to the shipped Time.pack max)
                    mem[adr++] = now->tm_mon + 1;     // st.wMonth (1..12; was 0-indexed -> pack rejected m=0)
                    mem[adr++] = now->tm_mday;        // st.wDay   (day-of-month; was tm_wday=weekday)
                    mem[adr++] = now->tm_hour; // st.wHour;
                    mem[adr++] = now->tm_min;  // st.wMinute;
                    mem[adr++] = now->tm_sec;  // st.wSecond;
                }
                return 1;
        case 8: // getspecs
                return Disks.GetSpecs(dsk, (Request*)(byte*)&mem[adr]);
        case 9: // setspecs
                return Disks.SetSpecs(dsk, (Request*)(byte*)&mem[adr]);
        default:
                trace("invalid disk operation: %d\n", op);
                return 0;
    }
}


/////////////////////////////////////////////////////////////////
// Bitmap graphics


void VM::BMG(int op)
{
    switch (op)
    {
        case 0: { // in rectangle
                    int h = Pop();
                    int w = Pop();
                    int y = Pop();
                    int x = Pop();
                    Push(inrect(x, y, w, h));
                    break;
                }

        case 1: { // vertical line
                    int len = Pop();
                    int y = Pop();
                    int x = Pop();
                    Bitmap* bmp = (Bitmap*)(byte*)&mem[Pop()];
                    int mode = Pop();
                    vline(mode, bmp, x, y, len);
                    break;
                }

        case 2: { // bitblit
                    int nobits = Pop();
                    int sofs   = Pop();
                    int sou    = Pop();
                    int dofs   = Pop();
                    int dst    = Pop();
                    int mode = Pop();
                    gbblt(mode, dst, dofs, sou, sofs, nobits);
                    break;
                }

        case 3: { // display character
                    int ch  = Pop();
                    Font* font = (Font*)(byte*)&mem[Pop()];
                    int y = Pop();
                    int x = Pop();
                    Bitmap* bmp = (Bitmap*)(byte*)&mem[Pop()];
                    int mode = Pop();
                    dch(mode, bmp, x, y, font, ch);
                    break;
                }

        case 4: { // clip
                    int h = Pop();
                    int w = Pop();
                    Clip* clp = (Clip*)(byte*)&mem[Pop()];
                    Push(clip(clp, w, h));
                    break;
                }


        case 5: { // line
                    int y1 = Pop();
                    int x1 = Pop();
                    int y = Pop();
                    int x = Pop();
                    Bitmap* bmp = (Bitmap*)(byte*)&mem[Pop()];
                    int mode = Pop();
                    line(mode, bmp, x, y, x1, y1);
                    break;
                }

        case 6: { // circle
                    int y = Pop();
                    int x = Pop();
                    Circle* ctx = (Circle*)(byte*)&mem[Pop()];
                    Bitmap* bmp = (Bitmap*)(byte*)&mem[Pop()];
                    int mode = Pop();
                    circle(mode, bmp, ctx, x, y);
                    break;
                }

        case 7: { // arc
                    ArcCtx* ctx = (ArcCtx*)(byte*)&mem[Pop()];
                    Bitmap* bmp = (Bitmap*)(byte*)&mem[Pop()];
                    int mode = Pop();
                    arc(mode, bmp, ctx);
                    break;
                }


        case 8: { // filled triangle
                    TriangleFilled* ctx = (TriangleFilled*)(byte*)&mem[Pop()];
                    trif(ctx);
                    break;
                }
        case 9: { // filled circle
                    CircleFilled* ctx = (CircleFilled*)(byte*)&mem[Pop()];
                    circlef(ctx);
                    break;
                }
        default:
            PC--; Ipt = 7;
    }
}


void VM::dch(int mode, Bitmap* bmp, int x, int y, Font* font, int ch)
{
    ch = ch % 256;
    int w = font->w % 32;
    int h = font->h % 64;
    dword test = mem[font->base + ch * h]
            ||   mem[font->base + ch + 1 * h];
    unused(test);
    dword* F = (dword*)(byte*)&mem[font->base + ch * h];
    dword* L = (dword*)(byte*)&mem[bmp->base + y * bmp->wpl + (x >> 5)];

    int x32 = x % 32;
//  trace(">dch(%d,%d ch=%d font+h*ch=0x%08X) w=%d, h=%d base=0x%08X\n", x, y, ch, F, w, h, font->base);
    dword wMask = ((1U << w) - 1);
    qword qMaskX32 = qword(wMask) << x32;
    dword  qFInverse = mode & 4 ? dword(-1) : 0;
    mode &= 3;
    for (int i = 0; i < h; i++)
    {
        qword* Q = (qword*)L;
        qword  qL = *Q;
        qword  qF = (*F ^ qFInverse) & wMask;
        qF = qF << x32;
        switch (mode)
        {
            case Mode::REP: 
                *Q = (qL & ~qMaskX32) | qF;
                break;
            case Mode::XOR:
                *Q = qL ^ qF;
                break;
            case Mode::BIC:
                *Q = qL & ~qF;
                break;
            case Mode::OR:
                *Q = qL | qF;
                break;
        }
        L += bmp->wpl;
        F += 1;
    }
//  trace("\n");
//  trace("<dch()\n");
}


int VM::inrect(int x, int y, int w, int h)
{
    // x <= w and y <= h is not a bug. This is how ucode was written!
    // BMG.m takes this into account
//  trace("inrect(%d, %d, %d, %d)=%d\n", x, y, w, h, x >= 0 && x <= w && y >= 0 && y <= h);
    return x >= 0 && x <= w && y >= 0 && y <= h;
} 


void VM::vline(int mode, Bitmap* bmp, int x, int y0, int len)
{
    if (x < 0 || x >= bmp->w)
        return;
    int ofs   = y0 * bmp->wpl + (x >> 5);
    int a = bmp->base + ofs;
    int bit   = 1 << (x % 32);
    for (int y = y0; y < y0 + len; y++)
    {
        switch (mode)
        {
            case Mode::REP: 
                mem[a] = (mem[a] & ~bit) | bit;
                break;
            case Mode::XOR:
                mem[a] = (mem[a] ^ bit);
                break;
            case Mode::BIC:
                mem[a] = (mem[a] & ~bit);
                break;
            case Mode::OR:
                mem[a] = (mem[a] | bit);
                break;
        }
//      trace("a=%08X\n", a);
        a += bmp->wpl;
    }
}


void VM::dot(int mode, Bitmap* bmp, int x, int y)
{
//  trace("dot(%d, %d)\n", x, y);
    vline(mode, bmp, x, y, 1);
}


void VM::hline(int mode, Bitmap* bmp, int x, int y, int x1)
{
    mode = mode % 4;
    if (y < 0 || y >= bmp->h)
        return;
    if (x > x1)
    {
        int d = x1;  x1 = x;  x = d;
    }
    int len = x1 - x + 1;
    if (len <= 0) // whole line out of screen
        return;
    if (x < 0)
        x = 0;
    if (x1 >= bmp->w)
        x1 = bmp->w - 1;
    if (x == x1)
    {
        dot(mode, bmp, x, y); 
        return;
    }
    _gbblt(mode, (byte*)&mem[bmp->base + y * bmp->wpl], x, &lnFF, x, len);
}


void VM::gbblt(int mode, int des, int dofs, int sou, int sofs, int bits)
{
    dword  test = mem[des] 
                | mem[sou]
                | mem[des + ((dofs + bits + 31) >> 5)]
                | mem[sou + ((sofs + bits + 31) >> 5)];
    unused(test);
    if (mem.OutOfRange())
    {
        Ipt = 3;
        return;
    }
    _gbblt(mode, (byte*)&mem[des], dofs, (byte*)&mem[sou], sofs, bits);
}


// It is important that clip() and line()
// both start cliping and drawing left to right (x0 <= x1).
// Otherwise line drawn from left to right and right to left
// may be draw differently at the endpoints. This is what
// swap below is for.

int VM::clip(Clip* ctx, int w, int h)
{
    int x0 = ctx->x0;
    int y0 = ctx->y0;
    int x1 = ctx->x1;
    int y1 = ctx->y1;
    if (x1 < x0) // swap (see not above)
    {
        int i = x1; x1 = x0; x0 = i;  
            i = y1; y1 = y0; y0 = i;
    }


    int x2 = x0; 
    int y2 = y0;
    if (inrect(x1, y1, w-1, h-1))
    {
        x2 = x1; 
        y2 = y1;
    }
    if (!inrect(x2, y2, w-1, h-1))
    {
        for (;;)
        {
            x2 = (x2 + x1) / 2; 
            y2 = (y2 + y1) / 2;
            if (inrect(x2, y2, w-1, h-1))
                break;
            // check out of clipping area:
            if (x0 == x2 && y0 == y2 || x1 == x2 && y1==y2) 
                return false;
            if ((x2 < 0) == (x0 < 0) || (y2 < 0) == (y0 < 0)) // !!!!!! ERROR!
            {
                x0 = x2;
                y0 = y2;
            }
            else
            {
                x1 = x2;
                y1 = y2;
            }
        }
    }
    assert(inrect(x2, y2, w-1, h-1));
    if (!inrect(x0, y0, w-1, h-1))
    {
        int x = x2; 
        int y = y2;
        for (;;)
        {
            int xC = (x0 + x) / 2; 
            int yC = (y0 + y) / 2;
            if  (xC == x0 && yC == y0 || xC == x && yC == y)
                break;
            if (inrect(xC, yC, w-1, h-1))
            {
                x = xC; 
                y = yC;
            }
            else
            {
                x0 = xC; 
                y0 = yC;
            }
        }
        x0 = x;
        y0 = y;
    }
    if (!inrect(x1, y1, w-1, h-1))
    {
        int x = x2; 
        int y = y2;
        for (;;)
        {
            int xC = (x1 + x) / 2; 
            int yC = (y1 + y) / 2;
            if (xC == x1 && yC ==y1 || xC == x && yC == y)
                break;
            if (inrect(xC, yC, w-1, h-1))
            {
                x = xC; 
                y = yC;
            }
            else
            {
                x1 = xC; 
                y1 = yC;
            }
        }
        x1 = x; 
        y1 = y;
    }
    ctx->x0 = x0;
    ctx->y0 = y0;
    ctx->x1 = x1;
    ctx->y1 = y1;
    return true;
}


void VM::drawline(int mode, Bitmap* bmp, int x, int y, int x1, int y1)
{
    int k;
    int p;
    int q;
    int d;

    int i = x <= x1 ? 1 : -1;
    int j = y <= y1 ? 1 : -1;
    int dx = abs(x1 - x); 
    int dy = abs(y1 - y);

    if (dx > dy)
    {
        int p = 2 * dy; 
        int q = 2 * (dy-dx); 
        int d = dy-dx; 
        int k = dx;
        do
        { 
            dot(mode, bmp, x, y);
            if (d <= dy)
                d = d + p;
            else
            {
                d = d + q; 
                y = y + j;
            }
            x = x + i; 
            k = k - 1;
        }
        while (k >= 0);
    }
    else if (dx < dy)
    {
        p = 2 * dx; 
        q = 2 * (dx-dy); 
        d = dx-dy; 
        k = dy;
        do
        {
            dot(mode, bmp, x,y);
            if (d <= dx)
                d = d + p;
            else
            {
                d = d + q; 
                x = x + i;
            }
            y = y + j; 
            k = k - 1;
        }
        while (k >= 0);
    }
    else
    {
        assert(dx == dy);
        k = dx;
        do
        {
            dot(mode, bmp, x,y);
            y = y + j; 
            x = x + i; 
            k = k - 1;
        }
        while (k >= 0);
    }
}


void VM::drawclipedline(int mode, Bitmap* bmp, int x0, int y0, int x1, int y1, int w, int h)
{
    int x2 = x0; 
    int y2 = y0;
    if (inrect(x1, y1, w-1, h-1))
    {
        x2 = x1; 
        y2 = y1;
    }
    if (!inrect(x2, y2, w-1, h-1))
    {
        for (;;)
        {
            x2 = (x2 + x1) / 2; 
            y2 = (y2 + y1) / 2;
            if (inrect(x2, y2, w-1, h-1))
                break;
            // check out of clipping area:
            if (x0 == x2 && y0 == y2 || x1 == x2 && y1==y2) 
                return;
            if ( (x2<0) == (x0<0) || (y2<0) == (y0<0)) // !!!!!! ERROR!
            {
                x0 = x2; 
                y0 = y2;
            }
            else
            {
                x1 = x2;
                y1 = y2;
            }
        }
    }
    assert(inrect(x2, y2, w-1, h-1));
    if (!inrect(x0, y0, w-1, h-1))
    {
        int x = x2; 
        int y = y2;
        for (;;)
        {
            int xC = (x0 + x) / 2; 
            int yC = (y0 + y) / 2;
            if  (xC == x0 && yC == y0 || xC == x && yC == y)
                break;
            if (inrect(xC, yC, w-1, h-1))
            {
                x = xC; 
                y = yC;
            }
            else
            {
                x0 = xC; 
                y0 = yC;
            }
        }
        x0 = x;
        y0 = y;
    }
    if (!inrect(x1, y1, w-1, h-1))
    {
        int x = x2; 
        int y = y2;
        for (;;)
        {
            int xC = (x1 + x) / 2; 
            int yC = (y1 + y) / 2;
            if (xC == x1 && yC ==y1 || xC == x && yC == y)
                break;
            if (inrect(xC, yC, w-1, h-1))
            {
                x = xC; 
                y = yC;
            }
            else
            {
                x1 = xC; 
                y1 = yC;
            }
        }
        x1 = x; 
        y1 = y;
    }
    drawline(mode, bmp, x0, y0, x1, y1);
}


void VM::line(int mode, Bitmap* bmp, int x, int y, int x1, int y1)
{
    //const char* smode[] = { "rep", "or", "xor", "bic" };
    //trace("line(mode %s %d,%d, %d,%d)\n", smode[mode % 4], x, y, x1, y1);
    int h = bmp->h;
    int w = bmp->w;
    mode = mode % 4;
    if (x == x1)
    {
        if (y > y1)
        {
            int d = y1;  y1 = y;  y = d;
        }
        vline(mode, bmp, x, y, y1 - y + 1); 
        return;
    }
    if (y == y1) 
    {
        hline(mode, bmp, x, y, x1); 
    }
    if (x1 < x) // swap (see not above by clip())
    {
        int i = x1; x1 = x; x = i;  
            i = y1; y1 = y; y = i;
    }
    if (inrect(x, y, w-1, h-1) && inrect(x1, y1, w-1, h-1))
        drawline(mode, bmp, x, y, x1, y1);
    else
        drawclipedline(mode, bmp, x, y, x1, y1, w, h);
}


void VM::trif(TriangleFilled *ptf)
{
    if (ptf->Case)
    {
        if (ptf->Gx)
        {
            do 
            {
                ptf->co += ptf->Dy;
                ptf->x  += ptf->dx;

                if (ptf->co >= ptf->Dx)
                {
                    ptf->co -= ptf->Dx;
                    ptf->y  += ptf->dy;
                    ptf->xn  = ptf->x - ptf->dx;
                    ptf->yn  = ptf->y - ptf->dy;
                    return;
                }
            } while (ptf->x != ptf->xl);

            ptf->xn = ptf->x;
            ptf->yn = ptf->y;
        }
        else
        {
            ptf->co += ptf->Dx;
            ptf->y  += ptf->dy;

            ptf->xn  = ptf->x;
            ptf->yn  = ptf->y - ptf->dy;

            if (ptf->co >= ptf->Dy)
            {
                ptf->co -= ptf->Dy;
                ptf->x  += ptf->dx;

                ptf->xn  = ptf->x - ptf->dx;
            }
        }
    }
    else 
    {
        if (ptf->Gx)
        {
            do 
            {
                ptf->co += ptf->Dy;
                ptf->x  += ptf->dx;

                if (ptf->co >= ptf->Dx)
                {
                    ptf->co -= ptf->Dx;
                    ptf->y  += ptf->dy;
                    ptf->xn  = ptf->x;
                    ptf->yn  = ptf->y;
                    return;
                }
            } while (ptf->x != ptf->xl);
        }
        else
        {
            ptf->co += ptf->Dx;
            ptf->y  += ptf->dy;

            if (ptf->co >= ptf->Dy)
            {
                ptf->co -= ptf->Dy;
                ptf->x  += ptf->dx;
            }
        }

        ptf->xn = ptf->x;
        ptf->yn = ptf->y;
    }
}

void VM::circle(int mode, Bitmap* bmp, Circle* pCtx, int X, int Y)
{
    for (int i = 0; i < 8; ++i)
    {
        int x, y;

        if (i & 0x4)
        {
            y = ((i & 0x1) ? (-pCtx->x) : (pCtx->x));
            x = ((i & 0x2) ? (-pCtx->y) : (pCtx->y));
        }
        else 
        {
            x = ((i & 0x1) ? (-pCtx->x) : (pCtx->x));
            y = ((i & 0x2) ? (-pCtx->y) : (pCtx->y));
        }

        x += X;
        y += Y;

        if (inrect(x, y, bmp->w, bmp->h))
            vline(mode, bmp, x, y, 1);
    }

    pCtx->co += pCtx->y;
    pCtx->y++;

    if (pCtx->co >= pCtx->x)
    {
        pCtx->co -= pCtx->x;
        pCtx->x--;
    }
}

void VM::circlef(CircleFilled *pCtx)
{
    pCtx->co += pCtx->y;
    pCtx->y++;
    pCtx->Do = 0;
    
    if (pCtx->co >= pCtx->x)
    {
        pCtx->co -= pCtx->x;
        pCtx->xn  = pCtx->x;
        pCtx->x--;
        pCtx->yn  = pCtx->y;
        pCtx->yn--;
        pCtx->Do  = 1;
    }
}

// xxx - delete me when circle, circlef and arc are implemented
#pragma warning(disable: 4100) // unreferenced formal parameter

void VM::arc(int mode, Bitmap* bmp, ArcCtx* ctx)
{
    // xxx
}

// xxx - delete me when circle, circlef and arc are implemented
#pragma warning(default: 4100) // unreferenced formal parameter


//
/////////////////////////////////////////////////////////////////

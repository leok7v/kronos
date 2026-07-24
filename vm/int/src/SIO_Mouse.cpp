///////////////////////////////////////////////////////////////////
// IGD480.cpp
#include <atomic>
#include "preCompiled.h"
#include "resource.h"
#include "Memory.h"
#include "SIO_Mouse.h"

template<typename T>
inline T InterlockedExchange(T* Target, T Value) {
    T ret = *Target;
    *Target = Value;
    return ret;
    //return std::atomic_exchange_explicit<LONG>(std::atomic_exchange_explicit, Value);
}


#pragma warning(disable: 4355) // 'this' : used in base member initializer list

SioMouse::SioMouse(int addr, int ipt) :
    i(new cI(addr, ipt, this)), nIn(0), nOut(0)
{
    memset(buf, 0, sizeof buf);
}

#pragma warning(default: 4355) // 'this' : used in base member initializer list

SioMouse::~SioMouse()
{
    delete i;
}


int  SioMouse::addr()
{
    return i->addr();
}


int  SioMouse::ipt()
{
    return i->ipt();
}


int  SioMouse::inpIpt()
{
    return i->inpIpt();
}


int  SioMouse::outIpt()
{
    return i->outIpt();
}


int  SioMouse::inp(int addr)
{
    return i->inp(addr);
}


void SioMouse::out(int addr, int data)
{
    i->out(addr, data);
}


int SioMouse::busyRead()
{
    long in = nIn;
    long out = nOut;
    if (in == out)
        return EMPTY;
    else
    {
        char ch = buf[out];
        out = (out+1) % (sizeof buf);
        ::InterlockedExchange(&nOut, out);
//      trace("SioMouse::busyRead()(%02X)\n", ch);
        return ch;
    }
}


void SioMouse::write(char*, int)
{
    // nothing
}


void SioMouse::writeChar(char)
{
    // nothing
}

void SioMouse::changeState(dword dwKeys, int dx, int dy)
{
//  trace("mouse.changeState(%d %d)\n", dx, dy);
    if (dx < -512)
        dx = -512;
    if (dy < -512)
        dy = -512;
    if (dx >  511)
        dx =  511;
    if (dy >  511)
        dy =  511;
//  trace("SioMouse::changeState(%0X, %d, %d)\n", dwKeys, dx, dy);
    long j = nIn;
    long nNewIn = (nIn + 5) % (sizeof buf);
    buf[j++] = byte((dwKeys & 0x7) + 0200);
    buf[j++] = byte(dx & 0xFF);  dx >>= 8;
    buf[j++] = byte(dy & 0xFF);  dy >>= 8;
    buf[j++] = byte(dx & 0xFF);
    buf[j++] = byte(dy & 0xFF);
    assert(j <= sizeof buf);
    ::InterlockedExchange(&nIn, nNewIn);
}


//
/////////////////////////////////////////////////////////////////

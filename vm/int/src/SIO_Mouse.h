///////////////////////////////////////////////////////////////////
// IGD480.h
#pragma once

#include "SIO.h"
#include "vmConsole.h"

/////////////////////////////////////////////////////////////////
// mouse:

class SioMouse : public SIO
{
public:
    SioMouse(int addr, int ipt);
    virtual ~SioMouse();

    // SIOInbound implementation:
    int  addr();
    int  ipt();
    int  inpIpt();
    int  outIpt();
    int  inp(int addr);
    void out(int addr, int data);

    // SIOOutbound implementation:
    virtual int  busyRead();
    virtual void write(char *ptr, int bytes);
    virtual void writeChar(char ch);
    virtual void onKey(bool, int, int, int) { }
    
    // IGD480 calls changeState:
    void changeState(dword dwKeys, int dx, int dy);
private:
    long nIn;
    long nOut;
    byte buf[5*1024];
    cI*  i;
};



//
/////////////////////////////////////////////////////////////////

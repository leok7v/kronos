#pragma once

#include "SIO.h"

class Console : public SIO
{
public:
    Console(int addr, int ipt);     // Local machine console ctor
    Console();

    virtual ~Console();

    // SIOInbound implementation:
    virtual int  addr();
    virtual int  ipt();
    virtual int  inpIpt();
    virtual int  outIpt();
    virtual int  inp(int addr);
    virtual void out(int addr, int data);

    // SIOOutbound implementation:
    virtual int  busyRead();
    virtual void write(char *ptr, int bytes);
    virtual void writeChar(char ch);
    virtual void onKey(bool bDown, int nVirtKey, int lKeyData, int ch);

private:
    SIOInbound *i;
    SIOOutbound *o;
};

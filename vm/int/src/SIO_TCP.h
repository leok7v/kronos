#pragma once

#include "SIO.h"

class SioTcp : public SIO
{
public:
    SioTcp(int addr, int ipt);
    virtual ~SioTcp();

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
    virtual void onKey(bool, int, int, int) {}

    int connect(dword socket);
    int connected();

private:
    SIOInbound *i;
    SIOOutbound *o;
};


class SioTcps
{
public:
    SioTcps(word port);
    virtual ~SioTcps();

    int addClient(SioTcp *p);
    int start();

    dword Worker();

private:
    enum { max_tcp = 9 };
    SioTcp *rgtcp[max_tcp];
    int N;

    word port;
    dword thread;

    SioTcp *find();
    void serve(dword so);
};
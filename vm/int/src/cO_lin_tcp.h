#pragma once
#include "preCompiled.h"
#include "SIO.h"

class cO_tcp : public SIOOutbound
{
public:
    // SIOOutbound methods:
    virtual int  busyRead();
    virtual void write(char *ptr, int bytes);
    virtual void writeChar(char ch);
    virtual void onKey(bool, int, int, int) {}

    int connect(dword socket);
    int connected();

    cO_tcp();
    virtual ~cO_tcp();
private:
    dword so;
    char chInput;

    // network reader thread:-
    dword tcpReader();
    //HANDLE thread;
    //HANDLE go;
    bool reading;

    static dword tcpWorker(void *pv);
};



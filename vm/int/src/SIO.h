#pragma once


enum { EMPTY = 512 }; // to allow 0x00 to pass through

struct SIOInbound
{
// Serial line IO Kronos side:-
    virtual int  addr() = 0;    // Q-BUS IO base addr, one serial line uses 4 consecutive shorts
                                // i.e. [0177560..0177570)
                                // Note that Kronos IO uses halved values:
                                // 0177560 = 0xFF70 = 0x7FB8 * 2
                                // Kronos uses 0x7FB8..0x7FBB (or 0x0FB8,,0x0FBB, since high bits of 
                                // IO bus are const 0x7000 :-)

    virtual int  ipt() = 0;     // We use two consecutive ipt numbers for serial line
                                // i.e. for default Q-BUS serial line it would be 0x0C for input and
                                // 0x0D for output

    virtual int  inpIpt() = 0;  // interrupt request from input line
    virtual int  outIpt() = 0;  // interrupt request from output line

    virtual int  inp(int addr) = 0;
    virtual void out(int arrd, int data) = 0;
};


struct SIOOutbound
{
//  Serial line IO VM side:-
    virtual int  busyRead() = 0;  // should return EMPTY if not ready
    virtual void write(char *ptr, int bytes) = 0;
    virtual void writeChar(char ch) = 0;
    virtual void onKey(bool bDown, int nVirtKey, int lKeyData, int ch) = 0;
};


struct SIO : public SIOInbound, SIOOutbound
{
};


class SIOs
{
public:
    SIOs();
    virtual ~SIOs();

    void addSIO(SIO *s);

    SIO *inpReady();
    SIO *outReady();

    SIO *find(int ioAddr);

private:
    enum { max_sio = 10 };
    SIO *rgsio[max_sio];
    int  N;
    int  lastIpted;
};


class cI : public SIOInbound
{
public:
    // SIOInbound methods:
    int  addr();
    int  ipt();
    int  inpIpt();
    int  outIpt();

    int  inp(int addr);
    void out(int arrd, int data);

    cI(int addr, int ipt, SIOOutbound *p);
protected:
    SIOOutbound *po;
    int  sioAddr;
    int  sioIpt;
    bool inpIptEnabled;
    bool outIptEnabled;
    int  inChar;
private:
    void _read();
};

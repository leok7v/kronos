#pragma once

#include <thread>
#include <mutex>
#include <deque>

#include "SIO.h"


class cO_lin : public SIOOutbound
{
public:
    // SIOOutbound methods:
    virtual int  busyRead();
    virtual void write(char *ptr, int bytes);
    virtual void writeChar(char ch);
    virtual void onKey(bool bDown, int nVirtKey, int lKeyData, int ch);

    cO_lin();
    virtual ~cO_lin();

private:
    std::deque<char> in;
    void kbdReader();
    void decode(char ch);
    std::unique_ptr<std::thread> kbdThread;
    std::mutex input_m;
};

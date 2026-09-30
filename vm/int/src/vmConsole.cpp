#include "preCompiled.h"
#include "vmConsole.h"
#include "cO_lin.h"

///////////////////////////////////////////////////////////////////////////////
// Console.

Console::Console(int addr, int ipt) : i(0), o(0) {
    o = new cO_lin;
    i = new cI(addr, ipt, o);
    if (i == null || o == null)
        delete this;
}

Console::~Console() {
    if (i != null) {
        delete i;
        i = null;
    }
    if (o != null) {
        delete o;
        o = null;
    }
}

int  Console::addr() { return i->addr(); }
int  Console::ipt() { return i->ipt(); }
int  Console::inpIpt() { return i->inpIpt(); }
int  Console::outIpt() { return i->outIpt(); }
int  Console::inp(int addr) { return i->inp(addr); }
void Console::out(int addr, int data) { i->out(addr, data); }

int  Console::busyRead() { return o->busyRead(); }
void Console::write(char *ptr, int bytes) { o->write(ptr, bytes); }
void Console::writeChar(char ch) { o->writeChar(ch); }
void Console::onKey(bool bDown, int nVirtKey, int lKeyData, int ch) {
    o->onKey(bDown, nVirtKey, lKeyData, ch);
}

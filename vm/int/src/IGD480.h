///////////////////////////////////////////////////////////////////
// IGD480.h
#pragma once

#include "SIO.h"
#include "vmConsole.h"
#include "preCompiled.h"

//#define XWRAP_IMPLEMENTATION
//#define XWRAP_AUTO_LINK
//#include "xwrap.h"

class MEMORY;
class SioMouse;

struct Bitmap
{
    int w;
    int h;
    int wpl;
    int base;
    int pattern;
    int mask;
    int layers[8];
};


struct Font
{
    int w;
    int h;
    int base;
    int rfe;
};


struct Clip
{
    int x0;
    int y0;
    int x1;
    int y1;
};


struct ArcCtx // Arc is taken by GDI
{
    int X;
    int Y;
    int x0;
    int y0;
    int x1;
    int y1;
    int r;
    int x;
    int y;
    int co;
    int xy0;
    int xx0;
    int xy1;
    int xx1;
    int yx0;
    int yy0;
    int yx1;
    int yy1;
    int Case;
};


struct Circle
{
    int x;
    int y;
    int co;
};


struct CircleFilled
{
    int x;
    int y;
    int co;
    int xn;
    int yn;
    int Do;
};


struct TriangleFilled
{
    int x;
    int y;
    int co;
    int xn;
    int yn;
          
    int Dx;
    int Dy;
    int dx;
    int dy;
    int xl;  
    int Gx;
    int Case;
};

enum Mode {
    REP = 0, // destinator := source
    OR  = 1, // destinator := destinator OR  source
    XOR = 2, // destinator := destinator XOR source
    BIC = 3  // destinator := destinator AND NOT source
};

#pragma warning (disable: 4512) // assignment operator could not be generated

using HWND = NON_TYPE;
using HDC = NON_TYPE;
using HBITMAP = NON_TYPE;
using LPARAM = NON_TYPE;
using WPARAM = NON_TYPE;
using LRESULT = NON_TYPE;

class IGD480
{
public:
    IGD480(MEMORY* mem, SioMouse*, Console*);
    virtual ~IGD480();
    void shutdown();

private:
    MEMORY&   mem;
    SioMouse& mouse;
    Console&  console;

    void* thread;
    bool  bRun;

    dword dwShift;
    dword dwLock;
    int   nCursor;

    HWND  hWnd;
    HWND  hStaticWnd;
    long  lResult;
    BYTE* pBits;
    HDC   mdc;
    HDC   bdc;
    HBITMAP hBitmap;
    HBITMAP hbmpScreen;
    int   mx;   // mouse x
    int   my;   // mouse y

    void  refresh();
    dword displayThread();
    bool  wndProc(UINT msg, WPARAM wParam, LPARAM lParam);
    void  onPaint();
    void  createWindow();
    void  createBitmap();
    void  copyBitmap();

    static dword rawDisplayThread(void*);
    static LRESULT rawWndProc(HWND, UINT, WPARAM, LPARAM);
};


//
/////////////////////////////////////////////////////////////////

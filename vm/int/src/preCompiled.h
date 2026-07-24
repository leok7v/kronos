///////////////////////////////////////////////////////////////////
// preCompiled.h
#pragma once
#pragma warning(disable:4100) // unreferenced formal parameter
#pragma warning(disable:4201) // nonstandard extension used : nameless struct/union
#pragma warning(disable:4514) // unreferenced inline function has been removed
#pragma warning(disable:4214) // nonstandard extension used : bit field types other than int
#pragma warning(disable:4115) // named type definition in parentheses
#pragma warning(disable:4711) // function selected for automatic inline expansion
#pragma warning(disable:4710) // function not expanded

#define STRICT
//#define NOGDI
//#define WIN32_LEAN_AND_MEAN
//#define _CRT_SECURE_NO_WARNINGS

//#include <Windows.h>
//#include <WindowsX.h>
//#include <WinIOctl.h>

#include <cstdint>
#include <thread>
#include <cstring>
#include <cmath>

#pragma warning(default:4100) // unreferenced formal parameter
#pragma warning(default:4201) // nonstandard extension used : nameless struct/union
#pragma warning(default:4214) // nonstandard extension used : bit field types other than int

#pragma intrinsic(memcmp, memcpy, memset, strcmp, strcpy, strlen)

#define unused(x) ((void)x)

using qword = uint64_t;
using qlong = int64_t;
using byte = uint8_t;
using dword = uint32_t;
using word = uint16_t;

using ULONG = uint32_t;
using LONG = int32_t;
using BYTE = uint8_t;
using UINT = uint32_t;
using INT = int32_t;

using NON_TYPE = std::nullptr_t;

constexpr std::nullptr_t null = nullptr;


//inline 
//int   abs(int x) { return x >= 0 ? x : -x; }
//inline 
//qlong qabs(qlong x) { return x >= 0 ? x : -x; }

#ifdef _DEBUG
    #define ODS(x) OutputDebugString(x)
    #define trace  _trace
    void   _trace(const char* fmt, ...);
    #define assert(exp) (void)( (exp) || (__assert(#exp, __FILE__, __LINE__), 0) )
    inline int __assert(const char* exp, const char* file, int line)
    {
        trace("\nassert(%s) failed in %s.%d\n", exp, file, line);
        _asm int 3;
        return 0;
    }
#else
    #define assert(x)
    #define ODS(x) {}
    #define trace  (void)

#endif

enum
{
    K = 1024
};

#ifndef _DEBUG
#if _MSC_VER < 1300
#pragma optimize("awsgy", on)
// a - assume no aliasing (e.g.  int x; int* p = x;  x = 1; *p = 2; x++; x==?)
// w - assume no aliasing accross functions borders;
// s - favor small size
// g - global opt OK
// y - optimize frame pointer
#endif
#endif

inline void Sleep(int n) {
    std::this_thread::sleep_for(std::chrono::milliseconds(n));
    //std::this_thread::sleep_for(std::chrono::microseconds(n));
}


//
///////////////////////////////////////////////////////////////////

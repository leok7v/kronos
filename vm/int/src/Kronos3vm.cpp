#include "preCompiled.h"
#include "Disks.h"
#include "Memory.h"
#include "Memory.h"
#include "vmConsole.h"
//#include "IGD480.h"
#include "SIO_TCP.h"
#include "VM.h"
#include <string>

char* skipword(const char* pStr)
{
    while (pStr != null && *pStr != 0 && *pStr != ' ')
    {
        if (*pStr == '"')
        {
            pStr++;
            while (*pStr != 0 && *pStr != '"') pStr++;
            if (*pStr != '"')
                return null;
            pStr++;
        }
        else
            pStr++;
    }
    return (char*)pStr;
}


char* skipspaces(const char* pStr)
{
    while (pStr != null && *pStr == ' ')
        pStr++;
    return (char*)pStr;
}


void AddDisks(VM& vm, int argc, const char* argv[])
{
    int i = 1;
    while (i < argc) {
        std::string arg(argv[i++]);
        char *p = arg.data();
        p = skipspaces(p);
        while (p != null && *p != 0)
        {
            char* q = skipword(p);
            if (q != null && *q != 0) { *q = 0; q++; }
            if (*p == '"') p++;
            if (strlen(p) > 0 && p[strlen(p)-1] == '"') p[strlen(p)-1] = 0;
            if (strlen(p) > 0)
            {
                if (!vm.Disks.AddDisk(p))
                    vm.printf("failed to add disk \"%s\"\n", p);
                else
                    vm.printf("disk%d \"%s\"\n", vm.Disks.GetCount()-1, p);
            }
            if (q != null && *q != 0) p = skipspaces(q);
            else p = null;
        }
    }
}

Console con(0xFB8, 0x0C);

SioTcp sio1(0xFBC, 0x0E);   // 1 177570b  70b
SioTcp sio2(0xFC0, 0x10);   // 2 177600b 100b
SioTcp sio3(0xFC4, 0x12);   // 3 177610b 110b
SioTcp sio4(0xFC8, 0x14);   // 4 177620b 120b
SioTcp sio5(0xFCC, 0x16);   // 5 177630b 130b
SioTcp sio6(0xFD0, 0x18);   // 6 177640b 140b
SioTcp sio7(0xFD4, 0x1A);   // 7 177650b 150b
SioTcp sio8(0xFD8, 0x1C);   // 8 177660b 160b

SioMouse mouse(0xFDC, 0x1E);    // 0 177670b 170b

SioTcps server(8086);

void AddConsole(VM &vm)
{
    vm.setConsole(&con);
    vm.sios.addSIO(&con);

    vm.sios.addSIO(&sio1);
    vm.sios.addSIO(&sio2);
    vm.sios.addSIO(&sio3);
    vm.sios.addSIO(&sio4);
    vm.sios.addSIO(&sio5);
    vm.sios.addSIO(&sio6);
    vm.sios.addSIO(&sio7);
    vm.sios.addSIO(&sio8);
    vm.sios.addSIO(&mouse);

    server.addClient(&sio1);
    server.addClient(&sio2);
    server.addClient(&sio3);
    server.addClient(&sio4);
    server.addClient(&sio5);
    server.addClient(&sio6);
    server.addClient(&sio7);
    server.addClient(&sio8);

    server.start();
}


bool ReadBooter(VM& vm)
{
    vm.Disks.Mount(1);
    return vm.Disks.Read(1, 0, &vm.mem[0], 4096);
}


int main(int argc, const char* argv[])
{
    // we do not want Abort|Retry|Ignore - do we?
    //::SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
    
    const int MemorySize = 0x1E0000; // host-port: 1.875M dword = 7.5MB (was 1M=4MB;
                                     // too small to natively clink 10 pcc modules).
                                     // Capped just below the IGD480 at word 0x1F0000.
    VM vm(MemorySize*4, &mouse, &con);

    AddConsole(vm);
    AddDisks(vm, argc, argv);

    if (vm.Disks.GetCount() == 0) {
        vm.printf("Kronos3vm.exe \"XD0.dsk\" \"XD1.dsk\" ...\n");
        while (vm.busyRead() == 0) {
            Sleep(10);
        }
        return 1;
    }
    if (!ReadBooter(vm)) {
        vm.printf("failed to read booter\n");
        while (vm.busyRead() == 0) {
            Sleep(10);
        }
        return 2;
    }
    vm.Run();
    vm.printf("Kronos stopped\n");
    while (vm.busyRead() == 0) {
        Sleep(10);
    }
    return 0;
}

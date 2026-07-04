#include <tchar.h>
#include <windows.h>
#include "crtinit.c"

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD dwReason, LPVOID lpReserved);

BOOL WINAPI _dllstart(HINSTANCE hDll, DWORD dwReason, LPVOID lpReserved) {
    BOOL bRet;
    if (dwReason == DLL_PROCESS_ATTACH)
        run_ctors(0, 0, 0);
    bRet = DllMain(hDll, dwReason, lpReserved);
    if (dwReason == DLL_PROCESS_DETACH)
        run_dtors();
    return bRet;
}

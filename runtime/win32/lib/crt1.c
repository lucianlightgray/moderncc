#include <tchar.h>

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#define _UNKNOWN_APP 0
#define _CONSOLE_APP 1
#define _GUI_APP 2

#define _MCW_PC 0x00030000
#define _PC_24 0x00020000
#define _PC_53 0x00010000
#define _PC_64 0x00000000

#ifdef _UNICODE
#define __tgetmainargs __wgetmainargs
#define _tstart _wstart
#define _tmain wmain
#define _runtmain _runwmain
#else
#define __tgetmainargs __getmainargs
#define _tstart _start
#define _tmain main
#define _runtmain _runmain
#endif

typedef struct {
    int newmode;
} _startupinfo;
int __cdecl __tgetmainargs(int *pargc, _TCHAR ***pargv, _TCHAR ***penv, int globb, _startupinfo *);
void __cdecl __set_app_type(int apptype);
unsigned int __cdecl _controlfp(unsigned int new_value, unsigned int mask);
extern int _tmain(int argc, _TCHAR *argv[], _TCHAR *env[]);

#include "crtinit.c"

int _dowildcard;

static LONG WINAPI catch_sig(EXCEPTION_POINTERS *ex) {
    return _XcptFilter(ex->ExceptionRecord->ExceptionCode, ex);
}

void _tstart(void) {
    int ret;

    _startupinfo start_info = {0};
    SetUnhandledExceptionFilter(catch_sig);
    __set_app_type(_CONSOLE_APP);

#if defined __i386__ || defined __x86_64__
    _controlfp(_PC_53, _MCW_PC);
#endif

    __tgetmainargs(&__argc, &__targv, &_tenviron, _dowildcard, &start_info);
    run_ctors(__argc, __targv, _tenviron);
    ret = _tmain(__argc, __targv, _tenviron);
    run_dtors();
    exit(ret);
}

__attribute__((weak)) extern int __run_on_exit();

int _runtmain(int argc, char **argv) {
    int ret;
#if defined UNICODE || defined __aarch64__
    _startupinfo start_info = {0};
    __tgetmainargs(&__argc, &__targv, &_tenviron, 0, &start_info);
#endif
#ifdef UNICODE
    if (argc < __argc) {
        __targv += __argc - argc;
        __argc = argc;
    }
#else
    __argc = argc;
    __targv = argv;
#endif
#if defined __i386__ || defined __x86_64__
    _controlfp(_PC_53, _MCW_PC);
#endif
    run_ctors(__argc, __targv, _tenviron);
    ret = _tmain(__argc, __targv, _tenviron);
    fflush(stdout);
    fflush(stderr);
    run_dtors();
    __run_on_exit(ret);
    return ret;
}

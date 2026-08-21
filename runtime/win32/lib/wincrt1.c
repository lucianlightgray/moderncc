#include <tchar.h>

#include <windows.h>
#include <stdlib.h>
#include <signal.h>

#define __UNKNOWN_APP 0
#define __CONSOLE_APP 1
#define __GUI_APP 2
void __set_app_type(int);
void _controlfp(unsigned a, unsigned b);

#ifdef _UNICODE
#define __tgetmainargs __wgetmainargs
#define _twinstart _wwinstart
#define _runtwinmain _runwwinmain
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int);
#else
#define __tgetmainargs __getmainargs
#define _twinstart _winstart
#define _runtwinmain _runwinmain
#endif

typedef struct
{
	int newmode;
} _startupinfo;

int __cdecl __tgetmainargs(int *pargc, _TCHAR ***pargv, _TCHAR ***penv, int globb, _startupinfo *);

#include "crtinit.c"

static int go_winmain(TCHAR *arg1) {
	STARTUPINFO si;
	_TCHAR *szCmd, *p;
	int fShow;
	int retval;

	GetStartupInfo(&si);
	if (si.dwFlags & STARTF_USESHOWWINDOW)
		fShow = si.wShowWindow;
	else
		fShow = SW_SHOWDEFAULT;

	szCmd = NULL, p = GetCommandLine();
	if (arg1)
		szCmd = _tcsstr(p, arg1);
	if (NULL == szCmd)
		szCmd = _tcsdup(__T(""));
	else if (szCmd > p && szCmd[-1] == __T('"'))
		--szCmd;
#if defined __i386__ || defined __x86_64__
	_controlfp(0x10000, 0x30000);
#endif
	run_ctors(__argc, __targv, _tenviron);
	retval = _tWinMain(GetModuleHandle(NULL), NULL, szCmd, fShow);
	run_dtors();
	return retval;
}

static LONG WINAPI catch_sig(EXCEPTION_POINTERS *ex) {
	int sig;
	switch (ex->ExceptionRecord->ExceptionCode) {
	case EXCEPTION_ACCESS_VIOLATION:
	case EXCEPTION_IN_PAGE_ERROR:
	case EXCEPTION_DATATYPE_MISALIGNMENT:
	case EXCEPTION_STACK_OVERFLOW:
		sig = SIGSEGV;
		break;
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
	case EXCEPTION_INT_OVERFLOW:
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	case EXCEPTION_FLT_OVERFLOW:
	case EXCEPTION_FLT_UNDERFLOW:
	case EXCEPTION_FLT_INEXACT_RESULT:
	case EXCEPTION_FLT_INVALID_OPERATION:
	case EXCEPTION_FLT_DENORMAL_OPERAND:
	case EXCEPTION_FLT_STACK_CHECK:
		sig = SIGFPE;
		break;
	case EXCEPTION_ILLEGAL_INSTRUCTION:
	case EXCEPTION_PRIV_INSTRUCTION:
		sig = SIGILL;
		break;
	default:
		return EXCEPTION_CONTINUE_SEARCH;
	}
	{
		__p_sig_fn_t _h = signal(sig, SIG_DFL);
		if (_h != SIG_DFL && _h != SIG_ERR) {
			signal(sig, _h);
			raise(sig);
		}
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

static void __cdecl _mcc_iph(const wchar_t *_e, const wchar_t *_f, const wchar_t *_fl, unsigned int _l, uintptr_t _r) {
	(void)_e;
	(void)_f;
	(void)_fl;
	(void)_l;
	(void)_r;
}

int _twinstart(void) {
	_startupinfo start_info_con = {0};
	SetUnhandledExceptionFilter(catch_sig);
	__set_app_type(__GUI_APP);
	_set_invalid_parameter_handler(_mcc_iph);
	__tgetmainargs(&__argc, &__targv, &_tenviron, 0, &start_info_con);
	exit(go_winmain(__argc > 1 ? __targv[1] : NULL));
}

int _runtwinmain(int argc, char **argv) {
	_set_invalid_parameter_handler(_mcc_iph);
#ifdef UNICODE
	_startupinfo start_info = {0};
	__tgetmainargs(&__argc, &__targv, &_tenviron, 0, &start_info);
	if (argc < __argc)
		__targv += __argc - argc, __argc = argc;
#else
	__argc = argc, __targv = argv;
#endif
	return go_winmain(__argc > 1 ? __targv[1] : NULL);
}

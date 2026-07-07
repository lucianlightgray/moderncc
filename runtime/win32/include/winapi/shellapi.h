#ifndef _INC_SHELLAPI
#define _INC_SHELLAPI

#ifndef WINSHELLAPI
#if !defined(_SHELL32_)
#define WINSHELLAPI DECLSPEC_IMPORT
#else
#define WINSHELLAPI
#endif
#endif

#ifndef SHSTDAPI
#if !defined(_SHELL32_)
#define SHSTDAPI EXTERN_C DECLSPEC_IMPORT HRESULT WINAPI
#define SHSTDAPI_(type) EXTERN_C DECLSPEC_IMPORT type WINAPI
#else
#define SHSTDAPI STDAPI
#define SHSTDAPI_(type) STDAPI_(type)
#endif
#endif

#if !defined(_WIN64)
#include <pshpack1.h>
#endif

#ifdef __cplusplus
extern "C" {

#endif

#ifdef UNICODE
#define ShellExecute ShellExecuteW
#define FindExecutable FindExecutableW
#else
#define ShellExecute ShellExecuteA
#define FindExecutable FindExecutableA
#endif

SHSTDAPI_(HINSTANCE)
ShellExecuteA(HWND hwnd, LPCSTR lpOperation, LPCSTR lpFile, LPCSTR lpParameters, LPCSTR lpDirectory, INT nShowCmd);
SHSTDAPI_(HINSTANCE)
ShellExecuteW(HWND hwnd, LPCWSTR lpOperation, LPCWSTR lpFile, LPCWSTR lpParameters, LPCWSTR lpDirectory, INT nShowCmd);
SHSTDAPI_(HINSTANCE)
FindExecutableA(LPCSTR lpFile, LPCSTR lpDirectory, LPSTR lpResult);
SHSTDAPI_(HINSTANCE)
FindExecutableW(LPCWSTR lpFile, LPCWSTR lpDirectory, LPWSTR lpResult);
SHSTDAPI_(LPWSTR *)
CommandLineToArgvW(LPCWSTR lpCmdLine, int *pNumArgs);

#ifdef __cplusplus
}
#endif

#if !defined(_WIN64)
#include <poppack.h>
#endif
#endif

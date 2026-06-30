

#ifndef _COMPAT__SIMPLE_H
#define _COMPAT__SIMPLE_H
#include <stdarg.h>
#include <sys/types.h>
#ifndef __printflike
#define __printflike(a, b)
#endif
typedef void *_SIMPLE_STRING;
typedef const char *_esc_func(unsigned char);
_SIMPLE_STRING _simple_salloc(void);
int  _simple_vsprintf(_SIMPLE_STRING, const char *, va_list) __printflike(2, 0);
int  _simple_sprintf(_SIMPLE_STRING, const char *, ...) __printflike(2, 3);
int  _simple_vesprintf(_SIMPLE_STRING, _esc_func *, const char *, va_list) __printflike(3, 0);
int  _simple_esprintf(_SIMPLE_STRING, _esc_func *, const char *, ...) __printflike(3, 4);
char *_simple_string(_SIMPLE_STRING);
void _simple_sresize(_SIMPLE_STRING);
int  _simple_sappend(_SIMPLE_STRING, const char *);
int  _simple_esappend(_SIMPLE_STRING, _esc_func *, const char *);
void _simple_put(_SIMPLE_STRING, int);
void _simple_putline(_SIMPLE_STRING, int);
void _simple_sfree(_SIMPLE_STRING);
int  _simple_vsnprintf(char *, size_t, const char *, va_list) __printflike(3, 0);
int  _simple_snprintf(char *, size_t, const char *, ...) __printflike(3, 4);
#endif

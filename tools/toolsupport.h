#ifndef MCC_TOOLSUPPORT_H
#define MCC_TOOLSUPPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "../src/mcchost.h"

#define TS_SKIP_CODE 77

int ts_run(const char *const *argv);

#define TS_ARGV_MAX 256
typedef struct {
    const char *a[TS_ARGV_MAX];
    int n;
} Argv;
void ts_arg(Argv *v, const char *s);
void ts_args(Argv *v, const char *const *set);
const char *const *ts_argz(Argv *v);

int ts_path(char *dst, size_t n, const char *dir, const char *fmt, ...);

int ts_fnmatch(const char *pat, const char *str);

int ts_glob(const char *dir, const char *pat, int recursive, char **out, int max);

int ts_file_equal(const char *a, const char *b);

char *ts_read_file(const char *path, long *len);

char *ts_first_error_line(const char *text,
                          const char *const *needles,
                          const char *const *skips);

int ts_cc_probe(const char *cc, char *machine, int msz, char *version, int vsz);

int ts_resolve_reference_cc(char *buf, int size);

void ts_skip(const char *fmt, ...);

int ts_git_stamp(char *buf, int size);

#endif

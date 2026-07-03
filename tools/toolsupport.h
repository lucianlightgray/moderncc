/*
 *  toolsupport.h - shared helpers for in-tree build/test tools
 *
 *  Every tool includes this header (never toolhost.h) and links against
 *  toolsupport.o.  It exposes the host-axis primitives (spawn/path/fs, via
 *  mcchost.h) plus tool-only helpers - glob, file compare, reference-compiler
 *  resolution, cc probe, stderr-line extraction, the exit-77 skip convention -
 *  each living here exactly once rather than copied per tool (PLAN 0.5).
 */
#ifndef MCC_TOOLSUPPORT_H
#define MCC_TOOLSUPPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "../src/mcchost.h"   /* host_* prototypes, HostSpawnOpts, MCC_HOST_* */

/* ctest skip convention: a driver exits 77 to mark the test skipped. */
#define TS_SKIP_CODE 77

/* echo argv (like build.c's trace) then host_spawn_wait; returns exit code. */
int   ts_run(const char *const *argv);
/* printf-style: split a command string on spaces is error-prone, so build
   argv explicitly; ts_runf is a convenience for the common echo+spawn. */

/* shell-style match of one path segment: '*', '?', '[...]' (no '/'). */
int   ts_fnmatch(const char *pat, const char *str);

/* collect paths under `dir` whose basename matches glob `pat`; up to `max`,
   each strdup'd into out[] (caller frees).  Returns the count, or -1. */
int   ts_glob(const char *dir, const char *pat, int recursive, char **out, int max);

/* byte-compare two files: 1 equal, 0 differ, -1 on open error. */
int   ts_file_equal(const char *a, const char *b);

/* read a whole file into a malloc'd NUL-terminated buffer; *len (optional)
   receives the byte length.  NULL on error. */
char *ts_read_file(const char *path, long *len);

/* first "meaningful" line of captured stderr: skip blank lines and any line
   containing a `skips` substring; if `needles` is non-NULL, prefer the first
   line containing a needle.  Returns a malloc'd copy (newline-trimmed) or
   NULL.  needles/skips are NULL-terminated arrays, or NULL to ignore. */
char *ts_first_error_line(const char *text,
                          const char *const *needles,
                          const char *const *skips);

/* probe a C compiler: machine (`-dumpmachine`) and first `--version` line,
   into caller buffers (either may be NULL).  Returns 0 on success, -1 else. */
int   ts_cc_probe(const char *cc, char *machine, int msz, char *version, int vsz);

/* resolve a genuine GNU gcc on PATH, rejecting a clang that answers to "gcc".
   Fills `buf` with the resolved program name/path.  Returns 1 if found. */
int   ts_resolve_reference_cc(char *buf, int size);

/* print "SKIP: <msg>" to stdout and exit(77) - the ctest skip convention. */
void  ts_skip(const char *fmt, ...);

/* git version stamp "<date> <branch>@<short>[*]" (the '*' = dirty tree), matching
   the CMake githash define.  Returns 0 (buf filled) or -1 if not a git repo. */
int   ts_git_stamp(char *buf, int size);

#endif /* MCC_TOOLSUPPORT_H */

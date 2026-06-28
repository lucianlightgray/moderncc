/* vlog.h -- opt-in verbose logging for the mcc test corpus.
 *
 * Any test may  #include "vlog.h"  and sprinkle VLOG()/VLOG_IF() calls to emit
 * diagnostic traces.  Logging is GATED ON A DEFINE: it is a no-op unless the
 * test is compiled with -DVLOG_ENABLE.  When enabled, output goes to STDERR
 * (never stdout), so:
 *   - golden "run" comparisons (which check program stdout) are unaffected, and
 *   - the gcc/clang/mcc differential (which compares stdout) is unaffected,
 * whether or not the define is set.  This lets you re-run a failing case with
 *   mcc -DVLOG_ENABLE ...   (or gcc/clang -DVLOG_ENABLE ...)
 * to see the trace without perturbing the checked output.
 *
 * Macros:
 *   VLOG(fmt, ...)          printf-style trace, prefixed with [vlog file:line]
 *   VLOG_IF(cond, fmt, ...) trace only when cond is true
 *   VLOG_ENTER(name)        trace "enter <name>"
 *   VLOG_VALUE(fmt, expr)   trace "<expr> = <fmt>" (expr stringized once)
 * All expand to nothing (a harmless empty statement / (void)0) without the define.
 */
#ifndef MCC_TEST_VLOG_H
#define MCC_TEST_VLOG_H

#ifdef VLOG_ENABLE
#  include <stdio.h>

/* Which compiler is building this TU -- handy in verbose traces. */
#  if defined(__MCC__)
#    define VLOG_CC "mcc"
#  elif defined(__clang__)
#    define VLOG_CC "clang"
#  elif defined(__GNUC__)
#    define VLOG_CC "gcc"
#  else
#    define VLOG_CC "cc"
#  endif

#  define VLOG(...) \
      do { \
          fprintf(stderr, "[vlog %s %s:%d] ", VLOG_CC, __FILE__, __LINE__); \
          fprintf(stderr, __VA_ARGS__); \
          fputc('\n', stderr); \
      } while (0)

#  define VLOG_IF(cond, ...) \
      do { if (cond) VLOG(__VA_ARGS__); } while (0)

#  define VLOG_ENTER(name) VLOG("enter %s", (name))

#  define VLOG_VALUE(fmt, expr) VLOG(#expr " = " fmt, (expr))

#else /* !VLOG_ENABLE -- compile everything out */

#  define VLOG(...)            ((void)0)
#  define VLOG_IF(cond, ...)   ((void)0)
#  define VLOG_ENTER(name)     ((void)0)
#  define VLOG_VALUE(fmt, expr) ((void)0)

#endif /* VLOG_ENABLE */

#endif /* MCC_TEST_VLOG_H */

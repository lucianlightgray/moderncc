#ifndef MCCLOG_H
#define MCCLOG_H

/*
 * Leveled diagnostic logging over the verbosity BITMASK (the mcc_log_verbose
 * global, an unsigned char). Each category is an independent power-of-two bit, so categories
 * can be enabled/disabled independently; a message tagged with category C is
 * emitted to stderr only when that bit is set in verbose.
 *
 *   -v / -vv / -vvv    set the low tier bits cumulatively (MCC_V1 / MCC_V2 / MCC_V3);
 *                      these first bits are the usual command / search-path / include
 *                      trace that -v has always controlled.
 *   -v<N>              OR an arbitrary bitmask (e.g. -v64 enables [DEBUG] alone,
 *                      -v128 enables [TRACE]).
 *
 * Eight categories (bit index -> tag): 0 [CMD] 1 [PATHS] 2 [INCL] 3 [INFO]
 * 4 [NOTE] 5 [STATUS] 6 [DEBUG] 7 [TRACE] (the 8th / high bit).
 *
 *   mcc_logf(MCC_LOG_DEBUG, "value=%d\n", v);   // "[DEBUG] value=%d" if that bit set
 *   MCC_DEBUG("value=%d\n", v);                 // shorthand
 *   MCC_TRACE("enter %s\n", name);              // "[TRACE] file:line func: ..." when
 *                                               // the TRACE bit is set; compiled out
 *                                               // unless the build sets MCC_CONFIG_TRACE.
 *
 * The plain macros read the mcc_log_verbose global (mirrored from the active
 * state's verbose in mcc_enter_state). The _v variants take an explicit verbose
 * byte so tracing fires in driver/link phases that run before mcc_enter_state,
 * or against a specific state:
 *
 *   mcc_logf_v(s->verbose, MCC_LOG_DEBUG, "%d evals\n", n);
 *   MCC_TRACE_V(s->verbose, "output %s\n", file);       // pre-mcc_enter_state
 *
 * Self-contained: no MCCState dependency; the sole global (mcc_log_verbose) is
 * defined in mcchost.c (standalone builds define their own copy).
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char MccLogMask;

extern unsigned char mcc_log_verbose;

enum {
	MCC_LOG_CMD = 1u << 0,
	MCC_LOG_PATHS = 1u << 1,
	MCC_LOG_INCL = 1u << 2,
	MCC_LOG_INFO = 1u << 3,
	MCC_LOG_NOTE = 1u << 4,
	MCC_LOG_STATUS = 1u << 5,
	MCC_LOG_DEBUG = 1u << 6,
	MCC_LOG_TRACE = 1u << 7,
	MCC_LOG_CATS = 8
};

/* Cumulative -v / -vv / -vvv tier masks (the low CMD/PATHS/INCL bits). */
#define MCC_V1 ((MccLogMask)MCC_LOG_CMD)
#define MCC_V2 ((MccLogMask)(MCC_LOG_CMD | MCC_LOG_PATHS))
#define MCC_V3 ((MccLogMask)(MCC_LOG_CMD | MCC_LOG_PATHS | MCC_LOG_INCL))

/* The -v tier bits alone, ignoring the independent diagnostic categories (INFO..
 * TRACE). Compare against MCC_V1/V2/V3 so a diagnostic bit such as TRACE can be
 * enabled at the same time without silencing the regular -v/-vv/-vvv output. */
#define MCC_VTIER(v) ((MccLogMask)((v) & MCC_V3))

static const char *const mcc_log_tags[MCC_LOG_CATS] = {
		"[CMD] ",  "[PATHS] ",  "[INCL] ",   "[INFO] ",
		"[NOTE] ", "[STATUS] ", "[DEBUG] ",  "[TRACE] ",
};

static inline const char *mcc_log_tag(MccLogMask bit) {
	int i;
	for (i = 0; i < MCC_LOG_CATS; i++)
		if (bit & (1u << i))
			return mcc_log_tags[i];
	return "";
}

static inline unsigned char mcc_log_floor(void) {
	static int mcc_log_floor_init;
	static unsigned char mcc_log_floor_mask;

	if (!mcc_log_floor_init) {
		const char *e = getenv("MCC_LOG");
		mcc_log_floor_init = 1;
		mcc_log_floor_mask = e ? (unsigned char)strtoul(e, NULL, 0) : 0;
	}
	return mcc_log_floor_mask;
}

static inline int mcc_log_enabled(MccLogMask bit) {
	return ((mcc_log_verbose | mcc_log_floor()) & bit) != 0;
}

static inline int mcc_log_enabled_v(unsigned char verbose, MccLogMask bit) {
	return (verbose & bit) != 0;
}

static inline void mcc_logf(MccLogMask bit, const char *fmt, ...) {
	va_list ap;
	if (!mcc_log_enabled(bit))
		return;
	fputs(mcc_log_tag(bit), stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static inline void mcc_logf_v(unsigned char verbose, MccLogMask bit,
															 const char *fmt, ...) {
	va_list ap;
	if (!mcc_log_enabled_v(verbose, bit))
		return;
	fputs(mcc_log_tag(bit), stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static inline int mcc_trace_tok(const char *list, const char *s, int exact) {
	const char *p = list;
	while (*p) {
		const char *e = p;
		size_t n;
		while (*e && *e != ',')
			e++;
		n = (size_t)(e - p);
		if (n) {
			if (exact) {
				if (!strncmp(s, p, n) && s[n] == 0)
					return 1;
			} else {
				const char *q;
				for (q = s; *q; q++)
					if (!strncmp(q, p, n))
						return 1;
			}
		}
		p = *e ? e + 1 : e;
	}
	return 0;
}

static inline int mcc_trace_want(const char *file, const char *func) {
	static int mcc_trace_filt_init;
	static const char *mcc_trace_only_file;
	static const char *mcc_trace_only_func;
	static const char *mcc_trace_skip_func;

	if (!mcc_trace_filt_init) {
		mcc_trace_filt_init = 1;
		mcc_trace_only_file = getenv("MCC_TRACE_FILE");
		mcc_trace_only_func = getenv("MCC_TRACE_FUNC");
		mcc_trace_skip_func = getenv("MCC_TRACE_SKIP");
	}
	if (mcc_trace_only_file && !mcc_trace_tok(mcc_trace_only_file, file, 0))
		return 0;
	if (mcc_trace_only_func && !mcc_trace_tok(mcc_trace_only_func, func, 0))
		return 0;
	if (mcc_trace_skip_func && mcc_trace_tok(mcc_trace_skip_func, func, 1))
		return 0;
	return 1;
}

static inline void mcc_trace_at(const char *file, int line, const char *func,
																const char *fmt, ...) {
	va_list ap;
	if (!mcc_log_enabled(MCC_LOG_TRACE))
		return;
	if (!mcc_trace_want(file, func))
		return;
	fprintf(stderr, "%s%s:%d %s: ", mcc_log_tag(MCC_LOG_TRACE), file, line, func);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static inline void mcc_trace_at_v(unsigned char verbose, const char *file, int line,
																	 const char *func, const char *fmt, ...) {
	va_list ap;
	if (!mcc_log_enabled_v(verbose, MCC_LOG_TRACE))
		return;
	if (!mcc_trace_want(file, func))
		return;
	fprintf(stderr, "%s%s:%d %s: ", mcc_log_tag(MCC_LOG_TRACE), file, line, func);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#define MCC_DEBUG(...) mcc_logf(MCC_LOG_DEBUG, __VA_ARGS__)
#define MCC_DEBUG_V(verbose, ...) mcc_logf_v(verbose, MCC_LOG_DEBUG, __VA_ARGS__)

#if defined(MCC_CONFIG_TRACE) && MCC_CONFIG_TRACE
#define MCC_TRACE(...) mcc_trace_at(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define MCC_TRACE_V(verbose, ...)                                              \
	mcc_trace_at_v(verbose, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define MCC_TRACE_IF(...)                                                      \
	do {                                                                         \
		if (mcc_log_enabled(MCC_LOG_TRACE))                                        \
			mcc_trace_at(__FILE__, __LINE__, __func__, __VA_ARGS__);                 \
	} while (0)
#else
#define MCC_TRACE(...) ((void)0)
#define MCC_TRACE_V(verbose, ...) ((void)0)
#define MCC_TRACE_IF(...) ((void)0)
#endif

#endif

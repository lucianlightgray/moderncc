#ifndef MCCLOG_H
#define MCCLOG_H

/*
 * Leveled diagnostic logging over the verbosity BITMASK (MCCState.verbose, an
 * unsigned char). Each category is an independent power-of-two bit, so categories
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
 * The plain macros read the mcc_state global, so they are silent in driver/link
 * phases that run before mcc_enter_state (mcc_state == NULL there). The _st
 * variants take an explicit MCCState* so tracing fires in those phases too:
 *
 *   mcc_logf_st(s, MCC_LOG_DEBUG, "%d evals\n", n);   // reads s->verbose
 *   MCC_TRACE_ST(s, "output %s\n", file);             // fires pre-mcc_enter_state
 *
 * Requires MCCState (for ->verbose) and the mcc_state global; include after mcc.h.
 */

#include <stdarg.h>
#include <stdio.h>

typedef unsigned char MccLogMask;

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

static inline int mcc_log_enabled(MccLogMask bit) {
	return mcc_state && (mcc_state->verbose & bit) != 0;
}

static inline int mcc_log_enabled_st(const MCCState *st, MccLogMask bit) {
	return st && (st->verbose & bit) != 0;
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

static inline void mcc_logf_st(const MCCState *st, MccLogMask bit,
															 const char *fmt, ...) {
	va_list ap;
	if (!mcc_log_enabled_st(st, bit))
		return;
	fputs(mcc_log_tag(bit), stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static inline void mcc_trace_at(const char *file, int line, const char *func,
																const char *fmt, ...) {
	va_list ap;
	if (!mcc_log_enabled(MCC_LOG_TRACE))
		return;
	fprintf(stderr, "%s%s:%d %s: ", mcc_log_tag(MCC_LOG_TRACE), file, line, func);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static inline void mcc_trace_at_st(const MCCState *st, const char *file, int line,
																	 const char *func, const char *fmt, ...) {
	va_list ap;
	if (!mcc_log_enabled_st(st, MCC_LOG_TRACE))
		return;
	fprintf(stderr, "%s%s:%d %s: ", mcc_log_tag(MCC_LOG_TRACE), file, line, func);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#define MCC_DEBUG(...) mcc_logf(MCC_LOG_DEBUG, __VA_ARGS__)
#define MCC_DEBUG_ST(st, ...) mcc_logf_st(st, MCC_LOG_DEBUG, __VA_ARGS__)

#if defined(MCC_CONFIG_TRACE) && MCC_CONFIG_TRACE
#define MCC_TRACE(...) mcc_trace_at(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define MCC_TRACE_ST(st, ...)                                                  \
	mcc_trace_at_st(st, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
#define MCC_TRACE(...) ((void)0)
#define MCC_TRACE_ST(st, ...) ((void)0)
#endif

#endif

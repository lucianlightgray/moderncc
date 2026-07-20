#include <stdio.h>
#include <stdlib.h>

#undef __attribute__

static int mcc_ubsan_seen;
static unsigned long mcc_ubsan_count;
static int mcc_ubsan_atexit_registered;
static int mcc_ubsan_summary_done;

static int mcc_ubsan_parse_int(const char *s, int *out) {
	int sign = 1;
	long acc = 0;
	if (!s || !*s)
		return 0;
	if (*s == '+') {
		s++;
	} else if (*s == '-') {
		sign = -1;
		s++;
	}
	if (!*s)
		return 0;
	for (; *s && *s != ':' && *s != ' '; s++) {
		if (*s < '0' || *s > '9')
			return 0;
		acc = acc * 10 + (*s - '0');
	}
	*out = (int)(sign * acc);
	return 1;
}

static int mcc_ubsan_options_exitcode(int *out) {
	const char *opts = getenv("UBSAN_OPTIONS");
	const char *key = "exitcode=";
	const char *p;
	if (!opts)
		return 0;
	for (p = opts; *p; p++) {
		const char *a = p;
		const char *b = key;
		while (*b && *a == *b) {
			a++;
			b++;
		}
		if (!*b)
			return mcc_ubsan_parse_int(a, out);
	}
	return 0;
}

static int mcc_ubsan_forced_exitcode(int *out) {
	const char *v = getenv("MCC_UBSAN_EXITCODE");
	if (v && *v)
		return mcc_ubsan_parse_int(v, out);
	return mcc_ubsan_options_exitcode(out);
}

static void mcc_ubsan_atexit(void) {
	int code = 0;
	if (mcc_ubsan_summary_done)
		return;
	mcc_ubsan_summary_done = 1;
	if (!mcc_ubsan_seen)
		return;
	fprintf(stderr,
			"%s: %lu recovered undefined-behavior event%s (mcc UBSan summary)\n",
			"UndefinedBehaviorSanitizer", mcc_ubsan_count,
			mcc_ubsan_count == 1UL ? "" : "s");
	fflush(stderr);
	if (mcc_ubsan_forced_exitcode(&code) && code != 0) {
		fflush(NULL);
		_Exit(code);
	}
}

static void mcc_ubsan_register(void) {
	if (mcc_ubsan_atexit_registered)
		return;
	mcc_ubsan_atexit_registered = 1;
	atexit(mcc_ubsan_atexit);
}

static void mcc_ubsan_report(const char *what) {
	fprintf(stderr,
			"%s: runtime error: %s (mcc UBSan, recovering)\n",
			"UndefinedBehaviorSanitizer", what);
	fflush(stderr);
	mcc_ubsan_seen = 1;
	mcc_ubsan_count++;
	mcc_ubsan_register();
}

static void mcc_ubsan_report_abort(const char *what) {
	mcc_ubsan_report(what);
	abort();
}

#define MCC_UBSAN_MINIMAL(sym, msg)                                            \
	void __ubsan_handle_##sym##_minimal(void) { mcc_ubsan_report(msg); }        \
	void __ubsan_handle_##sym##_minimal_abort(void) {                           \
		mcc_ubsan_report_abort(msg);                                              \
	}

MCC_UBSAN_MINIMAL(add_overflow, "signed integer overflow")
MCC_UBSAN_MINIMAL(sub_overflow, "signed integer subtraction overflow")
MCC_UBSAN_MINIMAL(mul_overflow, "signed integer multiplication overflow")
MCC_UBSAN_MINIMAL(negate_overflow, "negation overflow")
MCC_UBSAN_MINIMAL(divrem_overflow, "division overflow or divide by zero")
MCC_UBSAN_MINIMAL(shift_out_of_bounds, "shift out of bounds")
MCC_UBSAN_MINIMAL(out_of_bounds, "array index out of bounds")
MCC_UBSAN_MINIMAL(type_mismatch_v1, "null pointer or misaligned/undersized access")
MCC_UBSAN_MINIMAL(builtin_unreachable, "reached __builtin_unreachable")
MCC_UBSAN_MINIMAL(load_invalid_value, "load of invalid value")
MCC_UBSAN_MINIMAL(pointer_overflow, "pointer overflow")

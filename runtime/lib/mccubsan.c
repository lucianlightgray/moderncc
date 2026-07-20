#include <stdio.h>
#include <stdlib.h>

#undef __attribute__

static int mcc_ubsan_seen;

static void mcc_ubsan_report(const char *what) {
	fprintf(stderr,
			"%s: runtime error: %s (mcc UBSan, recovering)\n",
			"UndefinedBehaviorSanitizer", what);
	fflush(stderr);
	mcc_ubsan_seen = 1;
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

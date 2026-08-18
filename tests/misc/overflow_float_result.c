/* T-mac-30166: __builtin_*_overflow with a non-integer (float*) result must be
 * rejected — gcc/clang error ("must be pointer to non-const integer"); mcc
 * silently accepted it and miscomputed (o=0, *f=sum). This file must be
 * REJECTED (the integer-only check in the mccdefs.h macro fires). */
int main(void) {
	float f;
	return __builtin_add_overflow(3, 4, &f);
}

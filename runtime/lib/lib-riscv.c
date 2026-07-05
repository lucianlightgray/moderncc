void __clear_cache(void *beg, void *end) {
#ifdef __MCC__
	/* mcc intrinsic (TOK___riscv64_clear_cache): emits the fence.i sequence. */
	__riscv64_clear_cache(beg, end);
#else
	/* Host cc builds the native runtime for the embed/host-cc path; gcc and
	 * clang provide the cache-flush as a builtin (no __riscv64_clear_cache
	 * symbol exists outside mcc). */
	__builtin___clear_cache(beg, end);
#endif
}

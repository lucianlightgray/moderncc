void __clear_cache(void *beg, void *end) {
#ifdef __MCC__
	__riscv64_clear_cache(beg, end);
#else
	__builtin___clear_cache(beg, end);
#endif
}

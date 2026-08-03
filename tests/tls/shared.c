static _Alignas(64) __thread unsigned char slab[65536];
static __thread long local_scalar = 3;
__thread int exported_scalar = 7;

unsigned char *slab_base(void) { return slab; }

long read_local(void) { return local_scalar; }

int read_exported(void) { return exported_scalar; }

long slab_tpoff(void) {
	unsigned long tp = 0;
#if defined(__aarch64__)
	__asm__ volatile("mrs %0, tpidr_el0" : "=r"(tp));
#endif
	return (long)((unsigned long)slab - tp);
}

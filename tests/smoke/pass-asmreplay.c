extern int printf(const char *, ...);

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define SMP_ASM_DEFINES_LABEL "jmp .+6\nsmp_asm_label: .long 0\n"
#else
#define SMP_ASM_DEFINES_LABEL \
	".pushsection .rodata\nsmp_asm_label: .long 0\n.popsection\n"
#endif

int smp_asm_emit(void)
{
	__asm__ volatile(SMP_ASM_DEFINES_LABEL);
	return 0;
}

int smp_after(int x) { return x + 1; }

struct smp_tag_after_asm {
	int a;
	int b;
};

int smp_sizeof_after(void) { return (int)sizeof(struct smp_tag_after_asm); }

int main(void)
{
	printf("asmreplay %d %d %d\n", smp_asm_emit(), smp_after(41),
				 smp_sizeof_after());
	return 0;
}

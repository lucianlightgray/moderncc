#if defined(__i386__) || defined(__x86_64__)

void a_named_label(void) { asm volatile("asmobj_named: .long 0"); }

void a_local_static(void)
{
	static int asmobj_lstat = 41;
	asm("incl %0" : "+m"(asmobj_lstat));
}

void a_forward_ref(void)
{
	asm(".text; jmp asmobj_p0");
	asm(".text; asmobj_p0=.; nop");
}

void a_other_section(void) { asm(".data; .int 0x11223344; .text"); }

void a_pushsection(void)
{
	asm(".pushsection .rodata; .int 0xdeadbeef; .popsection");
}

#ifdef ASMOBJ_KP
void a_kp_extra(void) { asm(".data; .int 0x55667788; .text"); }
#endif

#else
int asmobj_not_x86;
#endif

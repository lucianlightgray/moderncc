#if defined(__APPLE__) && (!defined(__MCC__) || defined(__leading_underscore))
#define U(x) _##x
#else
#define U(x) x
#endif
	.data
	.globl U(al_a)
U(al_a):
	.byte 0x11
	.align 2
	.globl U(al_b)
U(al_b):
	.byte 0x22
	.align 3
	.globl U(al_c)
U(al_c):
	.byte 0x33

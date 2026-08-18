#if defined(__APPLE__) && (!defined(__MCC__) || defined(__leading_underscore))
#define U(x) _##x
#else
#define U(x) x
#endif
	.data
	.globl U(w0)
U(w0):
	.word 0x11223344
	.globl U(w1)
U(w1):
	.short 0xAABB
	.globl U(w2)
U(w2):
	.byte 0x55
	.globl U(dtgt)
U(dtgt):
	.byte 0xCD
	.globl U(dwrd)
U(dwrd):
	.word U(dtgt)-U(w0)
	.globl U(dend)
U(dend):
	.byte 0x99

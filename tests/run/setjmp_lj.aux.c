#include <setjmp.h>
#include <stdint.h>

extern jmp_buf shared_jb;
extern volatile int32_t shared_depth;

void aux_deep(int32_t n) {
	shared_depth++;
	if (n == 0)
		longjmp(shared_jb, 42);
	aux_deep(n - 1);
	shared_depth += 1000;
}

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>

jmp_buf shared_jb;
volatile int32_t shared_depth;

extern void aux_deep(int32_t n);

static jmp_buf local_jb;

static void thrower(int32_t code) { longjmp(local_jb, (int)code); }

int main(void) {
	int r = setjmp(shared_jb);
	if (r == 0) {
		printf("setjmp=0\n");
		aux_deep(5);
		printf("unreachable\n");
		return 1;
	}
	printf("longjmp=%d\n", r);
	printf("depth=%d\n", (int)shared_depth);

	{
		int v = setjmp(local_jb);
		if (v == 0) {
			thrower(7);
			printf("unreachable2\n");
			return 1;
		}
		printf("local=%d\n", v);
	}
	{
		volatile int32_t hits = 0;
		int v;
		v = setjmp(local_jb);
		if (v < 3) {
			hits++;
			longjmp(local_jb, v + 1);
		}
		printf("hits=%d\n", (int)hits);
		printf("final=%d\n", v);
	}
	return 0;
}

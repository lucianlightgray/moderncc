/* Real libmalloc: the nano/magazine/large tiers, malloc_size, alignment, and
   realloc's copy. tests/qemu/apple-libc documents why none of this can run
   off-Darwin -- libmalloc is fused to os_unfair_lock, Darwin TSD and mach_vm. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc/malloc.h>

static int fails;

#define CHECK(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		fails++; \
	} \
} while (0)

/* Each tier has a different allocator inside libmalloc: <=256 nano, then the
   tiny/small magazines, then a mach_vm_allocate-backed large block. */
static const size_t SIZES[] = {1, 16, 33, 256, 1024, 4096, 65536, 1 << 20};

int main(void) {
	for (unsigned i = 0; i < sizeof SIZES / sizeof SIZES[0]; i++) {
		size_t n = SIZES[i];
		unsigned char *p = malloc(n);
		CHECK(p != NULL);
		if (!p)
			continue;
		CHECK(malloc_size(p) >= n);
		CHECK(((uintptr_t)p & 15) == 0);
		memset(p, 0xA5, n);
		CHECK(p[0] == 0xA5 && p[n - 1] == 0xA5);
		free(p);
	}

	unsigned char *c = calloc(4096, 4);
	CHECK(c != NULL);
	if (c) {
		size_t zeros = 0;
		for (size_t i = 0; i < 4096 * 4; i++)
			zeros += (c[i] == 0);
		CHECK(zeros == 4096 * 4);
		free(c);
	}

	/* realloc across the tier boundary has to copy through a different
	   allocator, which is where a wrong malloc_size shows up. */
	unsigned char *r = malloc(64);
	CHECK(r != NULL);
	if (r) {
		for (int i = 0; i < 64; i++)
			r[i] = (unsigned char)i;
		r = realloc(r, 1 << 20);
		CHECK(r != NULL);
		if (r) {
			int ok = 1;
			for (int i = 0; i < 64; i++)
				ok &= (r[i] == (unsigned char)i);
			CHECK(ok);
			r = realloc(r, 32);
			CHECK(r != NULL);
			if (r) {
				int ok2 = 1;
				for (int i = 0; i < 32; i++)
					ok2 &= (r[i] == (unsigned char)i);
				CHECK(ok2);
			}
			free(r);
		}
	}

	void *a = NULL;
	CHECK(posix_memalign(&a, 256, 1000) == 0);
	CHECK(a != NULL && ((uintptr_t)a & 255) == 0);
	free(a);

	char *s = strdup("libmalloc");
	CHECK(s != NULL && !strcmp(s, "libmalloc"));
	free(s);

	/* Churn: a straight free-list bug shows up as a repeat address handed out
	   while still live, or a crash inside the magazine. */
	void *live[64];
	for (int round = 0; round < 200; round++) {
		for (int i = 0; i < 64; i++) {
			live[i] = malloc((size_t)(i * 37 + 1));
			CHECK(live[i] != NULL);
		}
		for (int i = 0; i < 64; i++)
			for (int j = i + 1; j < 64; j++)
				if (live[i] == live[j])
					fails++;
		for (int i = 0; i < 64; i++)
			free(live[i]);
	}

	if (fails) {
		fprintf(stderr, "libsystem_malloc: %d failure(s)\n", fails);
		return 1;
	}
	printf("libsystem_malloc: OK\n");
	return 0;
}

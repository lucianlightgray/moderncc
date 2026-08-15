#include <stddef.h>
#include <stdio.h>

typedef int v32 __attribute__((vector_size(32)));
typedef int v64 __attribute__((vector_size(64)));

#ifdef VECABI_MUTATE
#define VECABI_PERTURB __attribute__((packed))
#else
#define VECABI_PERTURB
#endif

struct v32box {
	char c;
	v32 v;
} VECABI_PERTURB;

struct v64box {
	char c;
	v64 v;
} VECABI_PERTURB;

int main(void)
{
	printf("v32 %zu %zu\n", offsetof(struct v32box, v), sizeof(struct v32box));
	printf("v64 %zu %zu\n", offsetof(struct v64box, v), sizeof(struct v64box));
	return 0;
}

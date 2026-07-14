#include <string.h>

extern int mccjit_selftest_fparg(const char *libpath, const char *incpath);

int main(int argc, char **argv) {
	const char *B = 0, *I = 0;
	int i;
	for (i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "-B", 2))
			B = argv[i] + 2;
		else if (!strncmp(argv[i], "-I", 2))
			I = argv[i] + 2;
	}
	return mccjit_selftest_fparg(B, I);
}

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
	int i;
	int total = 0;
	printf("argc=%d\n", argc);
	for (i = 1; i < argc; i++) {
		printf("arg%d=%s len=%d\n", i, argv[i], (int)strlen(argv[i]));
		total += (int)strlen(argv[i]);
	}
	printf("total=%d\n", total);
	printf("last=%s\n", argc > 1 ? argv[argc - 1] : "(none)");
	printf("nullterm=%d\n", argv[argc] == 0);
	return 0;
}

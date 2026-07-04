#include <stdio.h>
#include <string.h>

int main() {
	char a[10];
	strcpy(a, "abcdef");
	printf("%s\n", &a[1]);

	return 0;
}

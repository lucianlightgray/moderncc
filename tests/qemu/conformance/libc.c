#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(void) {
	char *p = malloc(64);
	if (!p)
		return 1;
	memset(p, 'A', 63);
	p[63] = '\0';
	if (strlen(p) != 63)
		return 2;

	char buf[32];
	int n = snprintf(buf, sizeof buf, "%d-%s-%x", 42, "ok", 255);
	if (n != 8 || strcmp(buf, "42-ok-ff") != 0)
		return 3;

	char dst[8];
	strcpy(dst, "hello");
	if (strcmp(dst, "hello") != 0)
		return 4;

	if (memcmp("abc", "abd", 3) >= 0)
		return 5;

	char *q = malloc(16);
	if (!q)
		return 6;
	memcpy(q, "0123456789", 11);
	if (q[10] != '\0' || q[5] != '5')
		return 7;

	free(q);
	free(p);
	return 0;
}

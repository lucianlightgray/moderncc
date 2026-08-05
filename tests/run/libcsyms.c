#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
	const char *src = "resolve-against-host";
	char *p = (char *)malloc(64);
	int32_t *a;
	int i;
	int32_t s = 0;
	char buf[64];

	if (!p) {
		printf("nomalloc\n");
		return 1;
	}
	memcpy(p, src, strlen(src) + 1);
	printf("copy=%s\n", p);
	printf("len=%d\n", (int)strlen(p));
	memset(p, 'x', 8);
	p[8] = 0;
	printf("set=%s\n", p);
	printf("cmp=%d\n", strcmp(p, "xxxxxxxx") == 0);
	free(p);

	a = (int32_t *)calloc(16, sizeof(int32_t));
	if (!a) {
		printf("nocalloc\n");
		return 1;
	}
	for (i = 0; i < 16; i++)
		s += a[i];
	printf("callocz=%d\n", s);
	for (i = 0; i < 16; i++)
		a[i] = i * i;
	s = 0;
	for (i = 0; i < 16; i++)
		s += a[i];
	printf("calloc=%d\n", s);
	a = (int32_t *)realloc(a, 32 * sizeof(int32_t));
	if (!a) {
		printf("norealloc\n");
		return 1;
	}
	for (i = 16; i < 32; i++)
		a[i] = 1;
	s = 0;
	for (i = 0; i < 32; i++)
		s += a[i];
	printf("realloc=%d\n", s);
	free(a);

	snprintf(buf, sizeof buf, "%s-%d", "fmt", 17);
	printf("snprintf=%s\n", buf);
	memmove(buf + 4, buf, 4);
	buf[8] = 0;
	printf("memmove=%s\n", buf);
	printf("memcmp=%d\n", memcmp("abc", "abd", 3) < 0);
	{
		static const char hay[] = "hello";
		printf("strchr=%d\n", (int)(strchr(hay, 'l') - hay));
	}
	printf("abs=%d\n", abs(-7));
	return 0;
}

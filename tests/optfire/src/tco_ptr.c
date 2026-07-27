#include <stdio.h>

static char *seek(char *p, int c)
{
	if (*p == 0 || *p == c)
		return p;
	return seek(p + 1, c);
}

static char buf[] = "abcdefgh";

int main(void)
{
	char *p = seek(buf, 'e');
	printf("tco_ptr=%d\n", (int)(p - buf));
	return 0;
}

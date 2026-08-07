#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char buf[1 << 18];
static char out[1 << 18];
static char word[64][32];

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 200;
	int r, i, w = 0;
	long acc = 0;
	size_t len;
	for (i = 0; i < (1 << 18) - 1; i++) {
		int k = (i * 7 + 13) % 27;
		buf[i] = (char)(k == 26 ? ' ' : 'a' + k);
	}
	buf[(1 << 18) - 1] = 0;
	for (r = 0; r < n; r++) {
		char *p = buf, *q = out;
		int inword = 0, start = 0;
		w = 0;
		while (*p) {
			char c = *p;
			if (c == ' ') {
				if (inword) {
					int l = (int)(p - buf) - start;
					if (l > 31)
						l = 31;
					if (w < 64) {
						memcpy(word[w], buf + start, (size_t)l);
						word[w][l] = 0;
						w++;
					}
					inword = 0;
				}
			} else {
				if (!inword) {
					start = (int)(p - buf);
					inword = 1;
				}
				*q++ = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
				acc += c;
			}
			p++;
		}
		*q = 0;
		len = strlen(out);
		acc += (long)len + w;
		for (i = 0; i < w; i++)
			acc += (long)strlen(word[i]);
		acc += strcmp(out, buf) != 0 ? 1 : 0;
	}
	printf("strproc %ld %d %s\n", acc, w, word[3]);
	return 0;
}

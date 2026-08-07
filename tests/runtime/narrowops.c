#include <stdio.h>
#include <stdlib.h>

static unsigned char b8[65536];
static short s16[65536];

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 400;
	int r, i;
	long acc = 0;
	unsigned uacc = 0;
	for (i = 0; i < 65536; i++) {
		b8[i] = (unsigned char)(i * 7 + 3);
		s16[i] = (short)(i * 13 - 4000);
	}
	for (r = 0; r < n; r++) {
		unsigned char c = (unsigned char)r;
		short h = (short)(r * 3);
		for (i = 0; i < 65536; i++) {
			unsigned char v = b8[i];
			short w = s16[i];
			int iv = (int)v + (int)w;
			unsigned uv = (unsigned)(v ^ c);
			b8[i] = (unsigned char)(v + c + (unsigned char)(w & 0xff));
			s16[i] = (short)(w + h - (short)v);
			acc += (long)(signed char)v + (long)(unsigned short)w;
			acc += (int)(short)(iv & 0xffff);
			uacc += uv * 31u + (unsigned)(iv >> 3);
			uacc ^= (unsigned)(unsigned char)(uv >> 5);
		}
	}
	printf("narrowops %ld %u %d %d\n", acc, uacc, (int)b8[1000], (int)s16[1000]);
	return 0;
}

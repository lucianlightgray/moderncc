#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *g;
static size_t glen, gcap;

static void put(const void *p, size_t n) {
	if (glen + n > gcap) {
		gcap = (glen + n) * 2 + 64;
		g = realloc(g, gcap);
	}
	memcpy(g + glen, p, n);
	glen += n;
}

static void put_be32(unsigned v) {
	unsigned char b[4];
	b[0] = v >> 24; b[1] = v >> 16; b[2] = v >> 8; b[3] = v;
	put(b, 4);
}

static void put_le16(unsigned v) {
	unsigned char b[2];
	b[0] = v; b[1] = v >> 8;
	put(b, 2);
}

static void put_le32(unsigned v) {
	unsigned char b[4];
	b[0] = v; b[1] = v >> 8; b[2] = v >> 16; b[3] = v >> 24;
	put(b, 4);
}

static void put_hdr(const char *name, unsigned size) {
	char h[60];
	memset(h, ' ', sizeof h);
	memcpy(h, name, strlen(name));
	{
		char s[16];
		int n = sprintf(s, "%u", size);
		memcpy(h + 48, s, n);
	}
	h[58] = '`'; h[59] = '\n';
	put(h, sizeof h);
}

static void pad_even(void) {
	if (glen & 1) { unsigned char z = '\n'; put(&z, 1); }
}

int main(int argc, char **argv) {
	FILE *f;
	int nsym, i;
	unsigned machine;
	const char *out, *dll;
	char **syms;
	unsigned *memoff;
	unsigned nentry, armap_size, cur, header_off;

	if (argc < 5) {
		fprintf(stderr, "usage: %s <out.lib> <machine_hex> <dll> <sym>...\n", argv[0]);
		return 2;
	}
	out = argv[1];
	machine = (unsigned)strtoul(argv[2], NULL, 16);
	dll = argv[3];
	syms = argv + 4;
	nsym = argc - 4;

	nentry = (unsigned)nsym * 2;

	armap_size = 4 + nentry * 4;
	for (i = 0; i < nsym; i++)
		armap_size += (unsigned)strlen(syms[i]) + 1
			+ 6 + (unsigned)strlen(syms[i]) + 1;

	memoff = malloc((size_t)nsym * sizeof *memoff);
	cur = 8 + 60 + armap_size + (armap_size & 1);
	for (i = 0; i < nsym; i++) {
		unsigned msize = 20 + (unsigned)strlen(syms[i]) + 1 + (unsigned)strlen(dll) + 1;
		memoff[i] = cur;
		cur += 60 + msize + (msize & 1);
	}

	put("!<arch>\n", 8);

	put_hdr("/", armap_size);
	put_be32(nentry);
	for (i = 0; i < nsym; i++) { put_be32(memoff[i]); put_be32(memoff[i]); }
	for (i = 0; i < nsym; i++) {
		put(syms[i], strlen(syms[i]) + 1);
		put("__imp_", 6);
		put(syms[i], strlen(syms[i]) + 1);
	}
	pad_even();

	for (i = 0; i < nsym; i++) {
		unsigned msize = 20 + (unsigned)strlen(syms[i]) + 1 + (unsigned)strlen(dll) + 1;
		header_off = (unsigned)glen;
		put_hdr("imp/", msize);
		if (header_off != memoff[i]) {
			fprintf(stderr, "internal offset mismatch\n");
			return 3;
		}
		put_le16(0);
		put_le16(0xFFFF);
		put_le16(0);
		put_le16(machine);
		put_le32(0);
		put_le32((unsigned)strlen(syms[i]) + 1 + (unsigned)strlen(dll) + 1);
		put_le16(0);
		put_le16(1 << 2);
		put(syms[i], strlen(syms[i]) + 1);
		put(dll, strlen(dll) + 1);
		pad_even();
	}

	f = fopen(out, "wb");
	if (!f) { perror(out); return 1; }
	fwrite(g, 1, glen, f);
	fclose(f);
	return 0;
}

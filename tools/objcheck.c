#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int u32;
typedef unsigned long long u64;

static unsigned char *slurp(const char *fn, long *n) {
	unsigned char *b;
	FILE *f = fopen(fn, "rb");
	long sz;
	if (!f) {
		fprintf(stderr, "objcheck: cannot open %s\n", fn);
		exit(2);
	}
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 0 || !(b = malloc(sz + 1)))
		exit(2);
	if (fread(b, 1, sz, f) != (size_t)sz)
		exit(2);
	fclose(f);
	*n = sz;
	return b;
}

static u32 le32(const unsigned char *p) {
	return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24;
}
static u32 be32(const unsigned char *p) {
	return (u32)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}
static u32 le16(const unsigned char *p) {
	return p[0] | p[1] << 8;
}

#define MH_MAGIC_64 0xfeedfacfu
#define LC_BUILD_VERSION 0x32u
/* Legacy minimum-version load commands (pre-LC_BUILD_VERSION). The packed
   X.Y.Z version lives at a different offset (8) than LC_BUILD_VERSION's minos (12). */
#define LC_VERSION_MIN_MACOSX 0x24u
#define LC_VERSION_MIN_IPHONEOS 0x25u
#define LC_VERSION_MIN_TVOS 0x2fu
#define LC_VERSION_MIN_WATCHOS 0x30u

static const char *macho_arch(u32 ct) {
	switch (ct) {
	case 0x01000007:
		return "x86_64";
	case 0x0100000c:
		return "arm64";
	case 7:
		return "i386";
	case 12:
		return "arm";
	default:
		return "?";
	}
}
static const char *macho_type(u32 ft) {
	switch (ft) {
	case 1:
		return "object";
	case 2:
		return "execute";
	case 6:
		return "dylib";
	default:
		return "?";
	}
}

static int macho_slice(const unsigned char *p, long n, u32 *ct, u32 *ft, u32 *minos) {
	u32 ncmds, sizeofcmds, i;
	const unsigned char *lc, *end;
	int have_build = 0;
	if (n < 32 || le32(p) != MH_MAGIC_64)
		return -1;
	*ct = le32(p + 4);
	*ft = le32(p + 12);
	ncmds = le32(p + 16);
	sizeofcmds = le32(p + 20);
	*minos = 0;
	if ((long)(32 + sizeofcmds) > n)
		return -1;
	lc = p + 32;
	end = lc + sizeofcmds;
	for (i = 0; i < ncmds; i++) {
		u32 cmd, cmdsize;
		if (lc + 8 > end)
			return -1;
		cmd = le32(lc);
		cmdsize = le32(lc + 4);
		if (cmdsize < 8 || lc + cmdsize > end)
			return -1;
		if (cmd == LC_BUILD_VERSION && cmdsize >= 16) {
			*minos = le32(lc + 12); /* LC_BUILD_VERSION wins when both present */
			have_build = 1;
		} else if (!have_build && cmdsize >= 12 &&
			   (cmd == LC_VERSION_MIN_MACOSX || cmd == LC_VERSION_MIN_IPHONEOS ||
			    cmd == LC_VERSION_MIN_TVOS || cmd == LC_VERSION_MIN_WATCHOS)) {
			*minos = le32(lc + 8); /* legacy version_min_command: version at +8 */
		}
		lc += cmdsize;
	}
	return 0;
}

static int macho_parse(const unsigned char *d, long n, u32 *ct, u32 *ft, u32 *minos) {
	if (n >= 8 && d[0] == 0xca && d[1] == 0xfe && d[2] == 0xba && d[3] == 0xbe) {
		u32 nfat = be32(d + 4), i;
		for (i = 0; i < nfat; i++) {
			const unsigned char *fa = d + 8 + (u64)i * 20;
			u32 off;
			if (fa + 20 > d + n)
				break;
			off = be32(fa + 8);
			if (off < (u32)n && macho_slice(d + off, n - off, ct, ft, minos) == 0)
				return 0;
		}
		return -1;
	}
	return macho_slice(d, n, ct, ft, minos);
}

static int pe_parse(const unsigned char *d, long n, u32 *machine, u32 *subsystem) {
	u32 lfanew, optoff;
	if (n < 0x40 || d[0] != 'M' || d[1] != 'Z')
		return -1;
	lfanew = le32(d + 0x3c);
	if ((long)(lfanew + 24) > n)
		return -1;
	if (memcmp(d + lfanew, "PE\0\0", 4))
		return -1;
	*machine = le16(d + lfanew + 4);
	optoff = lfanew + 24;
	*subsystem = ((long)(optoff + 70) <= n) ? le16(d + optoff + 68) : 0;
	return 0;
}

static int is_elf(const unsigned char *d, long n) {
	return n >= 4 && !memcmp(d, "\177ELF", 4);
}
static int is_macho(const unsigned char *d, long n) {
	return n >= 4 && ((le32(d) == MH_MAGIC_64) ||
					  (d[0] == 0xca && d[1] == 0xfe && d[2] == 0xba && d[3] == 0xbe));
}
static int is_pe(const unsigned char *d, long n) {
	return n >= 2 && d[0] == 'M' && d[1] == 'Z';
}

int main(int argc, char **argv) {
	const char *mode, *file, *expect = NULL;
	unsigned char *d;
	long n;
	int i;

	if (argc < 3) {
		fprintf(stderr, "usage: objcheck <type|macho|minos|pe> <file> [--expect X.Y.Z]\n");
		return 2;
	}
	mode = argv[1];
	file = argv[2];
	for (i = 3; i < argc; i++)
		if (!strcmp(argv[i], "--expect") && i + 1 < argc)
			expect = argv[++i];
	d = slurp(file, &n);

	if (!strcmp(mode, "type")) {
		const char *t = is_elf(d, n) ? "elf" : is_macho(d, n) ? "mach-o"
										   : is_pe(d, n)	  ? "pe"
															  : "unknown";
		printf("%s\n", t);
		return strcmp(t, "unknown") ? 0 : 1;
	}
	if (!strcmp(mode, "macho") || !strcmp(mode, "minos")) {
		u32 ct = 0, ft = 0, minos = 0;
		if (macho_parse(d, n, &ct, &ft, &minos) != 0) {
			printf("FAIL %s: not a valid Mach-O\n", file);
			return 1;
		}
		printf("Mach-O %s %s\n", macho_arch(ct), macho_type(ft));
		if (!strcmp(mode, "minos")) {
			char v[32];
			snprintf(v, sizeof v, "%u.%u.%u", minos >> 16, (minos >> 8) & 0xff, minos & 0xff);
			printf("minos %s\n", v);
			if (expect && strcmp(expect, v)) {
				printf("FAIL %s: minos %s != expected %s\n", file, v, expect);
				return 1;
			}
		}
		return 0;
	}
	if (!strcmp(mode, "pe")) {
		u32 machine = 0, subsystem = 0;
		if (pe_parse(d, n, &machine, &subsystem) != 0) {
			printf("FAIL %s: not a valid PE\n", file);
			return 1;
		}
		printf("PE machine 0x%04x subsystem %u\n", machine, subsystem);
		return 0;
	}
	fprintf(stderr, "objcheck: unknown mode '%s'\n", mode);
	return 2;
}

/* machofat — combine thin Mach-O files into a universal ("fat") binary.
 *
 *   machofat <out> <in1> [<in2> ...]
 *
 * mcc targets one CPU per binary — the Mach-O writer's cputype is a compile-time
 * choice (MCC_TARGET_ARM64 vs MCC_TARGET_X86_64), baked into separate
 * mcc-<arch>-osx binaries — so a single mcc invocation cannot emit a fat binary.
 * This host tool is the post-link combiner: it reads N already-linked thin
 * Mach-O files (each from a different mcc-<arch>-osx) and wraps them in a
 * big-endian fat header. A self-contained `lipo -create` needing no Apple tools.
 *
 * Slices are page-aligned (2^14 for arm64, 2^12 for x86_64/i386/arm) so any
 * ad-hoc code signature mcc already embedded in a slice stays valid — dyld maps
 * each slice from its aligned start. Offsets/sizes are 32-bit (FAT_MAGIC, not
 * FAT_MAGIC_64), which is ample for mcc's ~1 MB outputs.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FAT_MAGIC 0xcafebabeu /* on disk, big-endian */
#define MH_MAGIC 0xfeedfaceu
#define MH_CIGAM 0xcefaedfeu
#define MH_MAGIC_64 0xfeedfacfu
#define MH_CIGAM_64 0xcffaedfeu

#define CPU_ARCH_ABI64 0x01000000u
#define CPU_TYPE_X86 7u
#define CPU_TYPE_X86_64 (CPU_TYPE_X86 | CPU_ARCH_ABI64)
#define CPU_TYPE_ARM 12u
#define CPU_TYPE_ARM64 (CPU_TYPE_ARM | CPU_ARCH_ABI64)

struct slice {
	uint32_t cputype, cpusubtype, align, offset, size;
	unsigned char *data;
};

static uint32_t bswap32(uint32_t x) {
	return (x >> 24) | ((x >> 8) & 0xff00u) | ((x << 8) & 0xff0000u) | (x << 24);
}

/* Page-alignment exponent per CPU: arm64 pages are 16 KiB, the rest 4 KiB.
   Using at least the arch page size keeps embedded signatures/segments valid. */
static uint32_t align_exp(uint32_t cputype) {
	switch (cputype) {
	case CPU_TYPE_ARM64:
		return 14;
	case CPU_TYPE_X86_64:
	case CPU_TYPE_X86:
	case CPU_TYPE_ARM:
		return 12;
	default:
		return 14;
	}
}

static unsigned char *read_file(const char *path, uint32_t *len) {
	FILE *f = fopen(path, "rb");
	long n;
	unsigned char *buf;
	if (!f)
		return NULL;
	if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0) {
		fclose(f);
		return NULL;
	}
	rewind(f);
	buf = malloc((size_t)n ? (size_t)n : 1);
	if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
		free(buf);
		buf = NULL;
	}
	fclose(f);
	if (buf)
		*len = (uint32_t)n;
	return buf;
}

static void wr_be32(FILE *f, uint32_t v) {
	unsigned char b[4] = {(unsigned char)(v >> 24), (unsigned char)(v >> 16),
						  (unsigned char)(v >> 8), (unsigned char)v};
	fwrite(b, 1, 4, f);
}

int main(int argc, char **argv) {
	int nin, i, j, rc = 1;
	struct slice *sl;
	uint32_t off, hdr;
	FILE *out;

	if (argc < 3) {
		fprintf(stderr, "usage: machofat <out> <in1> [<in2> ...]\n");
		return 2;
	}
	nin = argc - 2;
	sl = calloc((size_t)nin, sizeof *sl);
	if (!sl)
		return 1;

	for (i = 0; i < nin; i++) {
		const char *path = argv[2 + i];
		uint32_t magic, ct, cs, len = 0;
		int swap;
		unsigned char *d = read_file(path, &len);
		if (!d) {
			fprintf(stderr, "machofat: cannot read '%s'\n", path);
			goto done;
		}
		sl[i].data = d;
		if (len < 16) {
			fprintf(stderr, "machofat: '%s' is too small to be a Mach-O file\n", path);
			goto done;
		}
		memcpy(&magic, d, 4);
		if (magic == MH_MAGIC || magic == MH_MAGIC_64)
			swap = 0;
		else if (magic == MH_CIGAM || magic == MH_CIGAM_64)
			swap = 1;
		else {
			fprintf(stderr, "machofat: '%s' is not a thin Mach-O (bad magic 0x%08x)\n",
					path, magic);
			goto done;
		}
		memcpy(&ct, d + 4, 4);
		memcpy(&cs, d + 8, 4);
		if (swap)
			ct = bswap32(ct), cs = bswap32(cs);
		for (j = 0; j < i; j++)
			if (sl[j].cputype == ct) {
				fprintf(stderr, "machofat: duplicate architecture (cputype 0x%08x)\n", ct);
				goto done;
			}
		sl[i].cputype = ct;
		sl[i].cpusubtype = cs;
		sl[i].size = len;
		sl[i].align = align_exp(ct);
	}

	/* Layout: fat_header (8 bytes) + nfat_arch * fat_arch (20 bytes), then
	   each slice at its page-aligned offset. */
	hdr = 8u + (uint32_t)nin * 20u;
	off = hdr;
	for (i = 0; i < nin; i++) {
		uint32_t a = 1u << sl[i].align;
		off = (off + a - 1u) & ~(a - 1u);
		sl[i].offset = off;
		off += sl[i].size;
	}

	out = fopen(argv[1], "wb");
	if (!out) {
		fprintf(stderr, "machofat: cannot write '%s'\n", argv[1]);
		goto done;
	}
	wr_be32(out, FAT_MAGIC);
	wr_be32(out, (uint32_t)nin);
	for (i = 0; i < nin; i++) {
		wr_be32(out, sl[i].cputype);
		wr_be32(out, sl[i].cpusubtype);
		wr_be32(out, sl[i].offset);
		wr_be32(out, sl[i].size);
		wr_be32(out, sl[i].align);
	}
	off = hdr;
	for (i = 0; i < nin; i++) {
		static const unsigned char zero[4096];
		while (off < sl[i].offset) {
			uint32_t pad = sl[i].offset - off;
			if (pad > sizeof zero)
				pad = sizeof zero;
			fwrite(zero, 1, pad, out);
			off += pad;
		}
		fwrite(sl[i].data, 1, sl[i].size, out);
		off += sl[i].size;
	}
	rc = fclose(out) ? 1 : 0;
	if (rc == 0)
		chmod(argv[1], 0755); /* a universal binary is an executable */

done:
	for (i = 0; i < nin; i++)
		free(sl[i].data);
	free(sl);
	return rc;
}

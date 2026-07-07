#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define FAT_MAGIC 0xcafebabeu
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
	unsigned char b[4] = {
			(unsigned char)(v >> 24), (unsigned char)(v >> 16),
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
	if (rc == 0) {
		chmod(argv[1], 0755);
		pid_t pid = fork();
		if (pid == 0) {
			execlp("codesign", "codesign", "-f", "-s", "-", argv[1], (char *)NULL);
			_exit(127);
		} else if (pid > 0) {
			int st;
			if (waitpid(pid, &st, 0) > 0 && WIFEXITED(st)) {
				int cs = WEXITSTATUS(st);
				if (cs != 0 && cs != 127)
					fprintf(stderr,
									"machofat: warning: codesign failed (exit %d); '%s' "
									"may be rejected by AMFI on Apple silicon\n",
									cs, argv[1]);
			}
		}
	}

done:
	for (i = 0; i < nin; i++)
		free(sl[i].data);
	free(sl);
	return rc;
}

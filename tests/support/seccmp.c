/*
 * seccmp.c - compare the loadable section contents of two relocatable ELF
 * objects (.text/.data/.rodata/.data.ro bytes, .bss sizes).
 *
 * Used by the dash-s-bytes-<arch> tests: `mcc -c` output vs `mcc -S` output
 * re-assembled by mcc's integrated assembler must be byte-identical on the
 * fixed-width targets.  Exit 0 identical, 1 different, 2 usage/parse error.
 * Host tool: parses both ELFCLASS32 and ELFCLASS64, little-endian only.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long long u64;

typedef struct {
    unsigned char *data;
    long size;
} blob;

typedef struct {
    char name[64];
    u64 off, size;
    unsigned type;
} sec;

static blob slurp(const char *fn)
{
    blob b = {0, 0};
    FILE *f = fopen(fn, "rb");
    if (!f) { fprintf(stderr, "seccmp: cannot open %s\n", fn); exit(2); }
    fseek(f, 0, SEEK_END);
    b.size = ftell(f);
    fseek(f, 0, SEEK_SET);
    b.data = malloc(b.size);
    if (fread(b.data, 1, b.size, f) != (size_t)b.size) exit(2);
    fclose(f);
    return b;
}

static u64 rd(const unsigned char *p, int n)
{
    u64 v = 0;
    int i;
    for (i = n - 1; i >= 0; i--)
        v = (v << 8) | p[i];
    return v;
}

/* Load section headers; returns count. */
static int sections(blob *b, sec *out, int max)
{
    int is64, shentsize, shnum, shstrndx, i, n = 0;
    u64 shoff, stroff;
    if (b->size < 64 || memcmp(b->data, "\177ELF", 4)) {
        fprintf(stderr, "seccmp: not an ELF file\n");
        exit(2);
    }
    is64 = b->data[4] == 2;
    if (is64) {
        shoff = rd(b->data + 0x28, 8);
        shentsize = (int)rd(b->data + 0x3a, 2);
        shnum = (int)rd(b->data + 0x3c, 2);
        shstrndx = (int)rd(b->data + 0x3e, 2);
    } else {
        shoff = rd(b->data + 0x20, 4);
        shentsize = (int)rd(b->data + 0x2e, 2);
        shnum = (int)rd(b->data + 0x30, 2);
        shstrndx = (int)rd(b->data + 0x32, 2);
    }
    stroff = is64 ? rd(b->data + shoff + (u64)shstrndx * shentsize + 0x18, 8)
                  : rd(b->data + shoff + (u64)shstrndx * shentsize + 0x10, 4);
    for (i = 0; i < shnum && n < max; i++) {
        const unsigned char *sh = b->data + shoff + (u64)i * shentsize;
        u64 nameoff = rd(sh, 4);
        const char *nm = (const char *)b->data + stroff + nameoff;
        sec *s = &out[n];
        /* the -S listing renames mcc's internal ".data.ro" to the
           conventional ".rodata"; compare them as one section */
        snprintf(s->name, sizeof s->name, "%s",
                 strcmp(nm, ".data.ro") ? nm : ".rodata");
        s->type = (unsigned)rd(sh + 4, 4);
        if (is64) {
            s->off = rd(sh + 0x18, 8);
            s->size = rd(sh + 0x20, 8);
        } else {
            s->off = rd(sh + 0x10, 4);
            s->size = rd(sh + 0x14, 4);
        }
        /* skip inert empty sections (mcc pre-creates e.g. an empty
           .data.ro even when the assembler input only fills .rodata) */
        if (s->size == 0 && s->type != 8 /* SHT_NOBITS */)
            continue;
        n++;
    }
    return n;
}

static sec *find(sec *v, int n, const char *name)
{
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(v[i].name, name))
            return &v[i];
    return NULL;
}

int main(int argc, char **argv)
{
    static const char *cmp[] = { ".text", ".data", ".rodata", 0 };
    blob a, b;
    sec sa[128], sb[128];
    int na, nb, i, rc = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: seccmp a.o b.o\n");
        return 2;
    }
    a = slurp(argv[1]);
    b = slurp(argv[2]);
    na = sections(&a, sa, 128);
    nb = sections(&b, sb, 128);

    for (i = 0; cmp[i]; i++) {
        sec *x = find(sa, na, cmp[i]), *y = find(sb, nb, cmp[i]);
        if (!x && !y)
            continue;
        if (!x || !y) {
            printf("DIFF %s: present in only one object\n", cmp[i]);
            rc = 1;
            continue;
        }
        if (x->size != y->size
         || memcmp(a.data + x->off, b.data + y->off, x->size)) {
            u64 j;
            printf("DIFF %s: %llu vs %llu bytes\n", cmp[i], x->size, y->size);
            for (j = 0; j < x->size && j < y->size; j++)
                if (a.data[x->off + j] != b.data[y->off + j]) {
                    printf("  first diff at +0x%llx: 0x%02x vs 0x%02x\n", j,
                           a.data[x->off + j], b.data[y->off + j]);
                    break;
                }
            rc = 1;
        } else {
            printf("OK   %s (%llu bytes)\n", cmp[i], x->size);
        }
    }
    {
        sec *x = find(sa, na, ".bss"), *y = find(sb, nb, ".bss");
        u64 xs = x ? x->size : 0, ys = y ? y->size : 0;
        if (xs != ys) {
            printf("DIFF .bss: %llu vs %llu bytes\n", xs, ys);
            rc = 1;
        } else if (xs) {
            printf("OK   .bss (%llu bytes)\n", xs);
        }
    }
    return rc;
}

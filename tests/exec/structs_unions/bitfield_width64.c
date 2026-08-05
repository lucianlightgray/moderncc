#include <stdio.h>
#include <string.h>

static int fail;

static void expect(const char *what, long long got, long long want) {
	if (got != want) {
		printf("FAIL %s: got %lld want %lld\n", what, got, want);
		fail = 1;
	}
}

static int little_endian(void) {
	union {
		unsigned u;
		unsigned char c[4];
	} probe;
	probe.u = 1;
	return probe.c[0] == 1;
}

static void expect_bytes(const char *what, const void *p, const unsigned char *want,
												 unsigned n) {
	unsigned i;
	if (!little_endian())
		return;
	if (!memcmp(p, want, n))
		return;
	printf("FAIL %s:", what);
	for (i = 0; i < n; i++)
		printf(" %02x", ((const unsigned char *)p)[i]);
	printf(" want");
	for (i = 0; i < n; i++)
		printf(" %02x", want[i]);
	printf("\n");
	fail = 1;
}

struct __attribute__((packed)) Lead {
	unsigned a : 1;
	unsigned long long x : 64;
};

struct __attribute__((packed)) Both {
	unsigned a : 3;
	unsigned long long x : 64;
	unsigned b : 5;
};

struct Post {
	unsigned a : 3;
	unsigned long long x : 64;
	unsigned b : 5;
} __attribute__((packed));

#pragma pack(push, 1)
struct Pragma {
	unsigned a : 1;
	unsigned long long x : 64;
};
struct PragmaSigned {
	unsigned a : 7;
	long long s : 64;
	unsigned char tail;
};
#pragma pack(pop)

struct __attribute__((packed)) Wide31 {
	unsigned a : 31;
	unsigned long long x : 64;
};

struct __attribute__((packed)) Byte {
	char c;
	unsigned long long x : 64;
};

struct Plain {
	unsigned long long x : 64;
};

struct Split {
	unsigned a : 1;
	unsigned long long x : 64;
};

static const unsigned char lead_bytes[9] = {0xdf, 0x9b, 0x57, 0x13,
																						0xcf, 0x8a, 0x46, 0x02, 0x00};
static const unsigned char both_bytes[9] = {0x7d, 0x6f, 0x5e, 0x4d,
																						0x3c, 0x2b, 0x1a, 0x09, 0xa8};

int main(void) {
	struct Lead lead;
	struct Both both;
	struct Post post;
	struct Pragma pr;
	struct PragmaSigned ps;
	struct Wide31 w31;
	struct Byte by;
	struct Plain pl;
	struct Split sp;

	expect("sizeof Lead", (long long)sizeof lead, 9);
	expect("sizeof Both", (long long)sizeof both, 9);
	expect("sizeof Post", (long long)sizeof post, 9);
	expect("sizeof Pragma", (long long)sizeof pr, 9);
	expect("sizeof PragmaSigned", (long long)sizeof ps, 10);
	expect("sizeof Wide31", (long long)sizeof w31, 12);
	expect("sizeof Byte", (long long)sizeof by, 9);
	expect("sizeof Plain", (long long)sizeof pl, 8);
	expect("alignof Lead", (long long)_Alignof(struct Lead), 1);
	expect("alignof Both", (long long)_Alignof(struct Both), 1);
	expect("alignof Pragma", (long long)_Alignof(struct Pragma), 1);

	memset(&lead, 0, sizeof lead);
	lead.a = 1;
	lead.x = 0x0123456789abcdefULL;
	expect_bytes("Lead image", &lead, lead_bytes, 9);
	expect("Lead.a", (long long)lead.a, 1);
	expect("Lead.x", (long long)lead.x, (long long)0x0123456789abcdefULL);

	memset(&both, 0, sizeof both);
	both.a = 5;
	both.x = 0x0123456789abcdefULL;
	both.b = 21;
	expect_bytes("Both image", &both, both_bytes, 9);
	expect("Both.a", (long long)both.a, 5);
	expect("Both.x", (long long)both.x, (long long)0x0123456789abcdefULL);
	expect("Both.b", (long long)both.b, 21);

	memset(&post, 0, sizeof post);
	post.a = 5;
	post.x = 0x0123456789abcdefULL;
	post.b = 21;
	expect_bytes("Post image", &post, both_bytes, 9);

	memset(&pr, 0, sizeof pr);
	pr.a = 1;
	pr.x = 0x0123456789abcdefULL;
	expect_bytes("Pragma image", &pr, lead_bytes, 9);

	memset(&ps, 0, sizeof ps);
	ps.a = 0x55;
	ps.s = -2;
	ps.tail = 0xa5;
	expect("PragmaSigned.a", (long long)ps.a, 0x55);
	expect("PragmaSigned.s", ps.s, -2);
	expect("PragmaSigned.tail", (long long)ps.tail, 0xa5);
	ps.s = 0x7fffffffffffffffLL;
	expect("PragmaSigned.s max", ps.s, 0x7fffffffffffffffLL);
	expect("PragmaSigned.a keep", (long long)ps.a, 0x55);
	expect("PragmaSigned.tail keep", (long long)ps.tail, 0xa5);

	memset(&w31, 0, sizeof w31);
	w31.a = 0x7fffffff;
	w31.x = 0xfedcba9876543210ULL;
	expect("Wide31.a", (long long)w31.a, 0x7fffffff);
	expect("Wide31.x", (long long)w31.x, (long long)0xfedcba9876543210ULL);
	w31.x = 0;
	expect("Wide31.a keep", (long long)w31.a, 0x7fffffff);

	memset(&by, 0, sizeof by);
	by.c = 0x5a;
	by.x = 0x0123456789abcdefULL;
	expect("Byte.c", (long long)by.c, 0x5a);
	expect("Byte.x", (long long)by.x, (long long)0x0123456789abcdefULL);

	memset(&pl, 0, sizeof pl);
	pl.x = 0xffffffffffffffffULL;
	expect("Plain.x", (long long)pl.x, -1);

	memset(&sp, 0, sizeof sp);
	sp.a = 1;
	sp.x = 0x0123456789abcdefULL;
	expect("Split.a", (long long)sp.a, 1);
	expect("Split.x", (long long)sp.x, (long long)0x0123456789abcdefULL);

	if (!fail)
		printf("OK\n");
	return fail;
}

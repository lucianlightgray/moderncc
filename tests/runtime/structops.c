#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	int id;
	int flags;
	long key;
	double weight;
	char name[24];
} Rec;

typedef struct {
	Rec a, b;
	int tag;
} Pair;

static Rec table[4096];
static Pair pairs[2048];

static Rec make(int i)
{
	Rec r;
	r.id = i;
	r.flags = (i * 3) & 0xff;
	r.key = (long)i * 2654435761L;
	r.weight = i * 0.5 - 100.0;
	memcpy(r.name, "record-000000000000000", 23);
	r.name[23] = 0;
	r.name[7] = (char)('0' + i % 10);
	r.name[8] = (char)('0' + (i / 10) % 10);
	return r;
}

static Pair combine(Rec x, Rec y)
{
	Pair p;
	p.a = x;
	p.b = y;
	p.tag = x.id ^ y.id;
	p.a.flags = p.a.flags | p.b.flags;
	p.b.weight = p.a.weight + p.b.weight;
	return p;
}

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 3000;
	int r, i;
	long acc = 0;
	double wacc = 0.0;
	for (r = 0; r < n; r++) {
		for (i = 0; i < 4096; i++)
			table[i] = make(i + r);
		for (i = 0; i < 2048; i++)
			pairs[i] = combine(table[i], table[i + 2048]);
		for (i = 0; i < 2048; i++) {
			acc += pairs[i].tag + pairs[i].a.key % 1021;
			wacc += pairs[i].b.weight;
		}
	}
	printf("structops %ld %.4f %s\n", acc, wacc, table[17].name);
	return 0;
}

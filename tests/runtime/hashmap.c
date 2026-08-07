#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NSLOT 8192
#define NNODE 200000

typedef struct Node {
	struct Node *next;
	unsigned key;
	int value;
	int hits;
} Node;

static Node pool[NNODE];
static Node *slot[NSLOT];
static int used;

static unsigned mix(unsigned k)
{
	k ^= k >> 16;
	k *= 2246822519u;
	k ^= k >> 13;
	k *= 3266489917u;
	k ^= k >> 16;
	return k;
}

static Node *lookup(unsigned key, int insert)
{
	unsigned h = mix(key) & (NSLOT - 1);
	Node *p = slot[h];
	while (p) {
		if (p->key == key) {
			p->hits++;
			return p;
		}
		p = p->next;
	}
	if (!insert || used >= NNODE)
		return 0;
	p = &pool[used++];
	p->key = key;
	p->value = (int)(key % 1013);
	p->hits = 0;
	p->next = slot[h];
	slot[h] = p;
	return p;
}

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 1500000;
	int i;
	long acc = 0;
	long found = 0;
	memset(slot, 0, sizeof slot);
	for (i = 0; i < n; i++) {
		unsigned k = (unsigned)(i * 2654435761u) % 120000u;
		Node *p = lookup(k, 1);
		if (p) {
			acc += p->value;
			found++;
		}
	}
	for (i = 0; i < NSLOT; i++) {
		Node *p = slot[i];
		while (p) {
			acc += p->hits;
			p = p->next;
		}
	}
	printf("hashmap %ld %ld %d\n", acc, found, used);
	return 0;
}

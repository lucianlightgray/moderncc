#include <stdio.h>
#include <stdlib.h>

enum {
	OP_NOP, OP_PUSH, OP_DUP, OP_ADD, OP_SUB, OP_MUL, OP_AND, OP_OR,
	OP_XOR, OP_SHL, OP_SHR, OP_NEG, OP_SWAP, OP_DROP, OP_JNZ, OP_HALT,
	OP_INC, OP_DEC, OP_MIN, OP_MAX, OP_ABS, OP_ROT, OP_LOAD, OP_STORE
};

static int prog[64];
static long mem[16];

static long run(long seed, int iters)
{
	long st[32];
	int sp = 0, pc, k;
	long acc = seed;

	for (k = 0; k < 16; k++)
		mem[k] = seed + k;
	for (k = 0; k < iters; k++) {
		sp = 0;
		st[sp++] = acc;
		for (pc = 0; pc < 64; pc++) {
			int op = prog[pc];
			switch (op) {
			case OP_NOP: break;
			case OP_PUSH: if (sp < 30) st[sp++] = pc + 1; break;
			case OP_DUP: if (sp > 0 && sp < 30) { st[sp] = st[sp - 1]; sp++; } break;
			case OP_ADD: if (sp > 1) { st[sp - 2] += st[sp - 1]; sp--; } break;
			case OP_SUB: if (sp > 1) { st[sp - 2] -= st[sp - 1]; sp--; } break;
			case OP_MUL: if (sp > 1) { st[sp - 2] *= (st[sp - 1] | 1) & 0xff; sp--; } break;
			case OP_AND: if (sp > 1) { st[sp - 2] &= st[sp - 1]; sp--; } break;
			case OP_OR: if (sp > 1) { st[sp - 2] |= st[sp - 1]; sp--; } break;
			case OP_XOR: if (sp > 1) { st[sp - 2] ^= st[sp - 1]; sp--; } break;
			case OP_SHL: if (sp > 0) st[sp - 1] <<= (pc & 3); break;
			case OP_SHR: if (sp > 0) st[sp - 1] >>= (pc & 3); break;
			case OP_NEG: if (sp > 0) st[sp - 1] = -st[sp - 1]; break;
			case OP_SWAP: if (sp > 1) { long t = st[sp - 1]; st[sp - 1] = st[sp - 2]; st[sp - 2] = t; } break;
			case OP_DROP: if (sp > 1) sp--; break;
			case OP_JNZ: if (sp > 0 && st[sp - 1] == 0) pc += 2; break;
			case OP_HALT: pc = 64; break;
			case OP_INC: if (sp > 0) st[sp - 1]++; break;
			case OP_DEC: if (sp > 0) st[sp - 1]--; break;
			case OP_MIN: if (sp > 1) { if (st[sp - 1] < st[sp - 2]) st[sp - 2] = st[sp - 1]; sp--; } break;
			case OP_MAX: if (sp > 1) { if (st[sp - 1] > st[sp - 2]) st[sp - 2] = st[sp - 1]; sp--; } break;
			case OP_ABS: if (sp > 0 && st[sp - 1] < 0) st[sp - 1] = -st[sp - 1]; break;
			case OP_ROT: if (sp > 2) { long t = st[sp - 3]; st[sp - 3] = st[sp - 2]; st[sp - 2] = st[sp - 1]; st[sp - 1] = t; } break;
			case OP_LOAD: if (sp > 0) st[sp - 1] = mem[st[sp - 1] & 15]; break;
			case OP_STORE: if (sp > 1) { mem[st[sp - 1] & 15] = st[sp - 2]; sp -= 2; } break;
			default: break;
			}
		}
		acc = sp > 0 ? st[sp - 1] : 0;
		acc = (acc * 1103515245L + 12345L) & 0x7fffffffL;
	}
	return acc;
}

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 60000;
	int i;
	long r;
	for (i = 0; i < 64; i++)
		prog[i] = (i * 7 + 3) % 24;
	r = run(12345L, n);
	printf("interp %ld %ld %ld\n", r, mem[0], mem[9]);
	return 0;
}

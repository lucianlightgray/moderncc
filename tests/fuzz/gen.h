#ifndef MCC_FUZZ_GEN_H
#define MCC_FUZZ_GEN_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
	uint64_t s;
} fuzz_rng;

static uint64_t fuzz_next(fuzz_rng *r) {
	uint64_t z = (r->s += 0x9e3779b97f4a7c15ull);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
	return z ^ (z >> 31);
}

static unsigned fuzz_pick(fuzz_rng *r, unsigned n) {
	return (unsigned)(fuzz_next(r) % (n ? n : 1));
}

typedef struct {
	int nvars;
	int nfuncs;
	int use_struct;
	int use_union;
	int use_array;
	int use_switch;
	int use_signed;
	int depth_limit;
} fuzz_cfg;

static void fuzz_expr(fuzz_rng *r, const fuzz_cfg *c, FILE *o, int fuel);

static void fuzz_var(fuzz_rng *r, const fuzz_cfg *c, FILE *o) {
	unsigned k = fuzz_pick(r, 6 + (c->use_array ? 1 : 0) + (c->use_struct ? 1 : 0));
	switch (k) {
	case 0:
	case 1:
		fprintf(o, "v%u", fuzz_pick(r, (unsigned)c->nvars));
		break;
	case 2:
		fprintf(o, "a");
		break;
	case 3:
		fprintf(o, "b");
		break;
	case 4:
		fprintf(o, "%luUL", (unsigned long)(fuzz_next(r) & 0xffffffffUL));
		break;
	case 5:
		fprintf(o, "%uU", (unsigned)fuzz_pick(r, 256));
		break;
	case 6:
		if (c->use_array)
			fprintf(o, "arr[%u]", fuzz_pick(r, 8));
		else
			fprintf(o, "v%u", fuzz_pick(r, (unsigned)c->nvars));
		break;
	default:
		if (c->use_struct)
			fprintf(o, "(unsigned long)g.f%u", fuzz_pick(r, 3));
		else
			fprintf(o, "v%u", fuzz_pick(r, (unsigned)c->nvars));
		break;
	}
}

static void fuzz_expr(fuzz_rng *r, const fuzz_cfg *c, FILE *o, int fuel) {
	if (fuel <= 0) {
		fuzz_var(r, c, o);
		return;
	}
	unsigned k = fuzz_pick(r, 10);
	switch (k) {
	case 0:
	case 1: {
		static const char *op[] = {"+", "-", "*", "&", "|", "^"};
		fputc('(', o);
		fuzz_expr(r, c, o, fuel - 1);
		fprintf(o, " %s ", op[fuzz_pick(r, 6)]);
		fuzz_expr(r, c, o, fuel - 1);
		fputc(')', o);
		break;
	}
	case 2: {
		fputs("((", o);
		fuzz_expr(r, c, o, fuel - 1);
		fputs(") / ((", o);
		fuzz_expr(r, c, o, fuel - 1);
		fputs(" & 0xffffUL) + 1UL))", o);
		break;
	}
	case 3: {
		fputs("((", o);
		fuzz_expr(r, c, o, fuel - 1);
		fputs(") % ((", o);
		fuzz_expr(r, c, o, fuel - 1);
		fputs(" & 0xffffUL) + 1UL))", o);
		break;
	}
	case 4: {
		const char *sh = fuzz_pick(r, 2) ? "<<" : ">>";
		fputs("((unsigned long)(", o);
		fuzz_expr(r, c, o, fuel - 1);
		fprintf(o, ") %s ((", sh);
		fuzz_expr(r, c, o, fuel - 1);
		fputs(") & 63UL))", o);
		break;
	}
	case 5: {
		static const char *cmp[] = {"<", ">", "<=", ">=", "==", "!="};
		fputs("(", o);
		fuzz_expr(r, c, o, fuel - 1);
		fprintf(o, " %s ", cmp[fuzz_pick(r, 6)]);
		fuzz_expr(r, c, o, fuel - 1);
		fputs(" ? ", o);
		fuzz_expr(r, c, o, fuel - 1);
		fputs(" : ", o);
		fuzz_expr(r, c, o, fuel - 1);
		fputs(")", o);
		break;
	}
	case 6:
		if (c->use_signed) {
			fputs("((unsigned long)((long)", o);
			fuzz_expr(r, c, o, fuel - 1);
			fputs(" >> 1))", o);
		} else {
			fputs("(~", o);
			fuzz_expr(r, c, o, fuel - 1);
			fputs(")", o);
		}
		break;
	default:
		fuzz_var(r, c, o);
		break;
	}
}

static void fuzz_stmt(fuzz_rng *r, const fuzz_cfg *c, FILE *o, int fi, int fuel,
					  int ind) {
	for (int i = 0; i < ind; i++)
		fputc('\t', o);
	unsigned k = fuzz_pick(r, 8 + (c->use_switch ? 1 : 0));
	switch (k) {
	case 0:
	case 1:
	case 2:
		fprintf(o, "v%u = ", fuzz_pick(r, (unsigned)c->nvars));
		fuzz_expr(r, c, o, fuel);
		fputs(";\n", o);
		break;
	case 3: {
		static const char *aop[] = {"+=", "-=", "*=", "^=", "|=", "&="};
		fprintf(o, "acc %s ", aop[fuzz_pick(r, 6)]);
		fuzz_expr(r, c, o, fuel);
		fputs(";\n", o);
		break;
	}
	case 4:
		fputs("if (", o);
		fuzz_expr(r, c, o, fuel);
		fputs(" & 1UL) {\n", o);
		fuzz_stmt(r, c, o, fi, fuel, ind + 1);
		for (int i = 0; i < ind; i++)
			fputc('\t', o);
		fputs("} else {\n", o);
		fuzz_stmt(r, c, o, fi, fuel, ind + 1);
		for (int i = 0; i < ind; i++)
			fputc('\t', o);
		fputs("}\n", o);
		break;
	case 5: {
		unsigned bound = 1 + fuzz_pick(r, 6);
		fprintf(o, "for (unsigned long i = 0; i < %uUL; i++) {\n", bound);
		fuzz_stmt(r, c, o, fi, fuel, ind + 1);
		for (int i = 0; i < ind; i++)
			fputc('\t', o);
		fputs("}\n", o);
		break;
	}
	case 6:
		if (fi > 0) {
			fprintf(o, "if (depth < %d) acc += f%u(", c->depth_limit,
					fuzz_pick(r, (unsigned)fi));
			fuzz_expr(r, c, o, fuel - 1);
			fputs(", ", o);
			fuzz_expr(r, c, o, fuel - 1);
			fputs(", depth + 1);\n", o);
		} else {
			fputs("acc += ", o);
			fuzz_expr(r, c, o, fuel);
			fputs(";\n", o);
		}
		break;
	case 7:
		if (c->use_array) {
			fprintf(o, "arr[%u] ^= ", fuzz_pick(r, 8));
			fuzz_expr(r, c, o, fuel);
			fputs(";\n", o);
		} else if (c->use_struct) {
			fprintf(o, "g.f%u = ", fuzz_pick(r, 3));
			fuzz_expr(r, c, o, fuel);
			fputs(" & 0xffUL;\n", o);
		} else {
			fprintf(o, "acc += ");
			fuzz_expr(r, c, o, fuel);
			fputs(";\n", o);
		}
		break;
	default: {
		unsigned n = 2 + fuzz_pick(r, 3);
		fputs("switch (", o);
		fuzz_expr(r, c, o, fuel);
		fprintf(o, " %% %uUL) {\n", n);
		for (unsigned ci = 0; ci < n; ci++) {
			for (int i = 0; i < ind; i++)
				fputc('\t', o);
			fprintf(o, "case %uUL: acc += ", ci);
			fuzz_expr(r, c, o, fuel - 1);
			fputs("; break;\n", o);
		}
		for (int i = 0; i < ind; i++)
			fputc('\t', o);
		fputs("}\n", o);
		break;
	}
	}
}

static void fuzz_emit(unsigned long seed, FILE *o) {
	fuzz_rng r;
	r.s = seed * 0x2545f4914f6cdd1dull + 0x1234567ull;

	fuzz_cfg c;
	c.nvars = 3 + (int)fuzz_pick(&r, 5);
	c.nfuncs = 2 + (int)fuzz_pick(&r, 4);
	c.use_struct = fuzz_pick(&r, 2);
	c.use_union = fuzz_pick(&r, 2);
	c.use_array = fuzz_pick(&r, 2);
	c.use_switch = fuzz_pick(&r, 2);
	c.use_signed = fuzz_pick(&r, 2);
	c.depth_limit = 3 + (int)fuzz_pick(&r, 4);

	fprintf(o, "#include <stdio.h>\n\n");
	if (c.use_struct)
		fprintf(o, "struct S { unsigned long f0 : 5; unsigned long f1 : 9; "
				   "unsigned long f2 : 18; };\nstatic struct S g = {1, 2, 3};\n");
	if (c.use_union)
		fprintf(o, "union U { unsigned long w; unsigned int h[2]; };\n"
				   "static union U u = {0x1122334455667788UL};\n");
	if (c.use_array)
		fprintf(o, "static unsigned long arr[8] = {1,2,3,4,5,6,7,8};\n");
	for (int i = 0; i < c.nvars; i++)
		fprintf(o, "static unsigned long v%d = %luUL;\n", i,
				(unsigned long)(fuzz_next(&r) & 0xffffffUL));
	fputc('\n', o);

	for (int fi = 0; fi < c.nfuncs; fi++) {
		fprintf(o, "static unsigned long f%d(unsigned long a, unsigned long b, "
				   "int depth) {\n\tunsigned long acc = a ^ (b * 3UL);\n",
				fi);
		int stmts = 3 + (int)fuzz_pick(&r, 5);
		for (int si = 0; si < stmts; si++)
			fuzz_stmt(&r, &c, o, fi, 3, 1);
		if (c.use_union)
			fputs("\tacc += u.h[a & 1UL] ^ u.w;\n", o);
		fputs("\treturn acc;\n}\n\n", o);
	}

	fputs("int main(void) {\n\tunsigned long acc = 0xcafef00dUL;\n", o);
	int rounds = 4 + (int)fuzz_pick(&r, 6);
	for (int ri = 0; ri < rounds; ri++) {
		fprintf(o, "\tacc = acc * 1000003UL + f%u(acc, %luUL, 0);\n",
				fuzz_pick(&r, (unsigned)c.nfuncs),
				(unsigned long)(fuzz_next(&r) & 0xffffffUL));
	}
	for (int i = 0; i < c.nvars; i++)
		fprintf(o, "\tacc = acc * 1000003UL + v%d;\n", i);
	if (c.use_array)
		fputs("\tfor (int i = 0; i < 8; i++) acc = acc * 1000003UL + arr[i];\n",
			  o);
	if (c.use_struct)
		fputs("\tacc = acc * 1000003UL + g.f0 + g.f1 + g.f2;\n", o);
	fputs("\tprintf(\"%lu\\n\", acc);\n", o);
	fputs("\treturn (int)(acc & 0x7fUL);\n}\n", o);
}

#endif

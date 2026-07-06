/* D1a — expression fill-in: Unary / Cast / Paren / Primary must be produced
 * (docs/CST.md §D1a). Also exercises the existing Binary/Call/Member/Index. */
struct pt { int x, y; };

int expr_kinds(int a, int b, struct pt *p, int *arr) {
	int paren = (a + b) * 2;      /* Paren + Binary + Primary */
	int neg = -a;                 /* Unary (prefix -)         */
	int deref = *arr;             /* Unary (prefix *)         */
	int cast = (int)3.5;          /* Cast                     */
	int sz = sizeof a;            /* Unary (sizeof)           */
	int mem = p->x + arr[b];      /* Member + Index + Primary */
	char *s = "literal";          /* Primary (string)         */
	return paren + neg + deref + cast + sz + mem + (int)(long)s;
}

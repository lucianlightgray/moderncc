/* Frame dependence: the address of a local escapes, so the arena's Ref
   offsets are host frame offsets and not renameable value names. */
int f(int a) {
	int t = a * 3;
	int *p = &t;
	*p += 1;
	return t;
}

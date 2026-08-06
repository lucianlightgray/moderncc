/* A VLA node has no portable meaning in a shader. */
int f(int n, int *out) {
	int v[n];
	v[0] = n;
	*out = v[0];
	return v[0];
}

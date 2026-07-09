int gv = 0;

int main(void) {
	int a = 1, b = 3, c = 0;
	int r = a && b;
	int s = (a && b) + 40;

	if (((a && b) || c) != 1)
		return 91;
	if (((a && gv) || (a && !c)) != 1)
		return 92;

	if ((a && b) || gv)
		if (!gv)
			return r + s;
	return 99;
}

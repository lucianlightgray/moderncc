












static int licm(int n, int a, int b)
{
	int i, s = 0;

	for (i = 0; i < n; i++)
		s += a * b + (a - b);
	return s;
}

static int ivsr(int n, int step)
{
	int i, s = 0;

	for (i = 0; i < n; i++)
		s += i * step;
	return s;
}

static int nest(int n, int m)
{
	int i, j, s = 0;

	for (i = 0; i < n; i++)
		for (j = 0; j < m; j++)
			s += i * m + j;
	return s;
}

static int fuse(int n)
{
	int i, s = 0, t = 0;

	for (i = 0; i < n; i++)
		s += i;
	for (i = 0; i < n; i++)
		t += i * 2;
	return s + t;
}

static int tile_sum(int n)
{
	int a[32], i, j, s = 0;

	for (i = 0; i < n; i++)
		a[i] = i * 3 - 1;
	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++)
			s += a[i] ^ a[j];
	return s;
}

int main(void)
{
	volatile int vn, va, vb, vs, vi, vj, vstep, vm;
	int i, j;

	vn = 17;
	va = 5;
	vb = 3;
	vs = 0;
	for (i = 0; i < vn; i++)
		vs += va * vb + (va - vb);
	if (licm(17, 5, 3) != vs)
		return 1;

	vn = 21;
	vstep = 4;
	vs = 0;
	for (i = 0; i < vn; i++)
		vs += i * vstep;
	if (ivsr(21, 4) != vs)
		return 2;

	vn = 9;
	vm = 7;
	vs = 0;
	for (i = 0; i < vn; i++)
		for (j = 0; j < vm; j++)
			vs += i * vm + j;
	if (nest(9, 7) != vs)
		return 3;

	vn = 13;
	vs = 0;
	for (i = 0; i < vn; i++)
		vs += i;
	for (i = 0; i < vn; i++)
		vs += i * 2;
	if (fuse(13) != vs)
		return 4;

	{
		volatile int ta[32];
		vn = 11;
		for (i = 0; i < vn; i++)
			ta[i] = i * 3 - 1;
		vs = 0;
		for (i = 0; i < vn; i++)
			for (j = 0; j < vn; j++) {
				vi = ta[i];
				vj = ta[j];
				vs += vi ^ vj;
			}
		if (tile_sum(11) != vs)
			return 5;
	}



	if (licm(0, 5, 3) != 0)
		return 6;
	if (ivsr(0, 4) != 0)
		return 7;
	if (nest(0, 7) != 0)
		return 8;
	if (nest(3, 0) != 0)
		return 9;

	return 0;
}

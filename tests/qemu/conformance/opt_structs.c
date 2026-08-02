









struct inner {
	int a;
	int b;
};

struct outer {
	struct inner in;
	int c;
	int d;
	struct inner *p;
};

static int arrow_chain(struct outer *o)
{
	int s = 0;

	s += o->in.a;
	s += o->in.b;
	s += o->c;
	s += o->d;
	s += o->p->a;
	s += o->p->b;
	return s;
}

static int arrow_opassign(struct outer *o, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		o->c += 2;
		o->d -= 1;
		o->in.a ^= 3;
		o->in.b |= i & 1;
	}
	return o->c + o->d + o->in.a + o->in.b;
}

static int arrow_incdec(struct outer *o, int n)
{
	int i, s = 0;

	for (i = 0; i < n; i++) {
		o->c++;
		++o->d;
		s += o->c - o->d;
	}
	return s + o->c + o->d;
}

static int chainstore(struct outer *o)
{
	o->in.a = 1;
	o->in.b = 2;
	o->c = 3;
	o->d = 4;
	return o->in.a + o->in.b + o->c + o->d;
}

int main(void)
{
	struct inner side;
	struct outer o;
	volatile int vs, vc, vd, va, vb, i;

	side.a = 11;
	side.b = 13;
	o.in.a = 2;
	o.in.b = 3;
	o.c = 5;
	o.d = 7;
	o.p = &side;

	vs = 0;
	vs += o.in.a;
	vs += o.in.b;
	vs += o.c;
	vs += o.d;
	vs += side.a;
	vs += side.b;
	if (arrow_chain(&o) != vs)
		return 1;

	{
		struct outer t = o;
		vc = o.c;
		vd = o.d;
		va = o.in.a;
		vb = o.in.b;
		for (i = 0; i < 9; i++) {
			vc = vc + 2;
			vd = vd - 1;
			va = va ^ 3;
			vb = vb | (i & 1);
		}
		if (arrow_opassign(&t, 9) != vc + vd + va + vb)
			return 2;
	}

	{
		struct outer t = o;
		vc = o.c;
		vd = o.d;
		vs = 0;
		for (i = 0; i < 6; i++) {
			vc = vc + 1;
			vd = vd + 1;
			vs = vs + (vc - vd);
		}
		if (arrow_incdec(&t, 6) != vs + vc + vd)
			return 3;
	}

	{
		struct outer t = o;
		if (chainstore(&t) != 1 + 2 + 3 + 4)
			return 4;
		if (t.in.a != 1 || t.in.b != 2 || t.c != 3 || t.d != 4)
			return 5;
	}



	if (o.in.a != 2 || o.in.b != 3 || o.c != 5 || o.d != 7)
		return 6;
	if (o.p != &side || side.a != 11 || side.b != 13)
		return 7;


	{
		struct outer t = o;
		if (arrow_opassign(&t, 0) != o.c + o.d + o.in.a + o.in.b)
			return 8;
		if (arrow_incdec(&t, 0) != o.c + o.d)
			return 9;
	}

	return 0;
}

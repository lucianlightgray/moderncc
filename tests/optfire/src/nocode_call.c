extern int sink(int);

static int dead_if(int x)
{
	if (0)
		sink(x);
	return x;
}

static int dead_tail(int x)
{
	return x;
	sink(x);
}

static int dead_and(int x)
{
	return 0 && sink(x);
}

int nocode_call_main(int x)
{
	return dead_if(x) + dead_tail(x) + dead_and(x);
}

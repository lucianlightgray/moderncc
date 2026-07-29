extern int sink(int);

int not_and(int a, int b)
{
	return !(a && b);
}

int not_or(int a, int b)
{
	if (!(a || b))
		return sink(a);
	return b;
}

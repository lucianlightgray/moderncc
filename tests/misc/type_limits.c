int always_false(unsigned x)
{
	return x < 0;
}

int always_true(unsigned x)
{
	return x >= 0;
}

int legitimate(unsigned x)
{
	return x < 5;
}

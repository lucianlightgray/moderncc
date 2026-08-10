int printf(const char *, ...);

int mix(int x)
{
	return ((x * 2654435761) ^ ((x * 2654435761) >> 13)) * 5 + 17;
}

int main(void)
{
	int k, s = 0;
	for (k = 0; k < 64; k++)
		s += mix(k & 7);
	printf("s=%d m0=%d m7=%d\n", s, mix(0), mix(7));
	return 0;
}

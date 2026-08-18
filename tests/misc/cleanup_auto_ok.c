static int n;
void cf(int *p) { n += *p; }
int main(void) {
	{
		int x __attribute__((cleanup(cf))) = 5;
		(void)x;
	}
	return n == 5 ? 0 : 1;
}

static int n;
void cf(int *p) { n += *p; }
void cfv(void *p) { (void)p; }
void cfc(const int *p) { (void)p; }
int main(void) {
	{
		int x __attribute__((cleanup(cf))) = 5;
		int y __attribute__((cleanup(cfv))) = 0;
		int z __attribute__((cleanup(cfc))) = 0;
		(void)x;
		(void)y;
		(void)z;
	}
	return n == 5 ? 0 : 1;
}

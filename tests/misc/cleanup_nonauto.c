static void cf(int *p) { (void)p; }
static int g __attribute__((cleanup(cf)));
int main(void) {
	static int s __attribute__((cleanup(cf)));
	(void)s;
	return g;
}

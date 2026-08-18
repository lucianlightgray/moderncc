void cf_long(long *p) { (void)p; }
int main(void) {
	int x __attribute__((cleanup(cf_long))) = 0;
	(void)x;
	return 0;
}

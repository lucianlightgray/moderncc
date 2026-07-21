int GetUserNameA(char *buf, unsigned long *sz);

int main(void) {
	char buf[64];
	unsigned long n = sizeof buf;
	return GetUserNameA(buf, &n) ? 0 : 1;
}

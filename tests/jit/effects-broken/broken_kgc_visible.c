char *getenv(const char *);
int printf(const char *, ...);

int main(void)
{
	char *e = getenv("MCC_JIT_KGC");
	printf("kgc=%s\n", (e && e[0] == '0') ? "off" : "on");
	return 0;
}

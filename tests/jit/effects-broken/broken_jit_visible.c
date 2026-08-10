char *getenv(const char *);
int printf(const char *, ...);

int main(void)
{
	char *j = getenv("MCC_JIT");
	char *k = getenv("MCC_JIT_KGC");
	int on = j && j[0] == '1' && !(k && k[0] == '0');
	printf("route=%s\n", on ? "kgc" : "aot");
	return 0;
}

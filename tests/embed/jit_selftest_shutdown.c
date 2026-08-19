extern int mccjit_selftest_shutdown(int mode);

int main(int argc, char **argv) {
	int mode = 0;
	int i;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == 'k')
			mode = 1;
		else if (argv[i][0] == 's')
			mode = 2;
	}
	return mccjit_selftest_shutdown(mode);
}

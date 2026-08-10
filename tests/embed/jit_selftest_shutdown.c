extern int mccjit_selftest_shutdown(int known_positive);

int main(int argc, char **argv) {
	int kp = 0;
	int i;
	for (i = 1; i < argc; i++)
		if (argv[i][0] == 'k')
			kp = 1;
	return mccjit_selftest_shutdown(kp);
}

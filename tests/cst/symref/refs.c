int myglobal = 5;

static int helper(int p) {
	return p + myglobal;
}

int reader(void) {
	int local = helper(myglobal);
	return local;
}

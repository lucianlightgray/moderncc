int x = 1;

int outer(void) {
	int x = 2;
	return x;
}

int reader(void) {
	return x;
}

int f(void) {
	static int s = ({ int x = 3; x; });
	return s;
}

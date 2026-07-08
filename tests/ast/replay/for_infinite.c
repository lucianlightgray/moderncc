int main(void) {
	int s = 0;
	int i = 0;
	for (;;) {
		if (i >= 8)
			break;
		s = s + i;
		i = i + 1;
	}
	return s;
}

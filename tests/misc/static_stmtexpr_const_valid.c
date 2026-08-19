int g;

int main(void) {
	static int a = ({ 7; });
	static int b = ({ 1 + 2; });
	static int c = ({ (char)300; });
	static int *p = ({ &g; });
	if (a != 7) return 1;
	if (b != 3) return 2;
	if (c != 44) return 3;
	if (p != &g) return 4;
	return 0;
}

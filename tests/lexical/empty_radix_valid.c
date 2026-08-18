/* T-mac-30213 known-positive: every well-formed radix literal must still be
 * accepted after adding the >=1-digit requirement — including hex floats with
 * no integer part (0x.5p3) and the 0b/0o extensions. Exit 0 on success. */
int main(void) {
	int a = 0xAB, b = 0b101, c = 0o17, d = 017, e = 0xff, f = 0;
	double g = 0x.5p3, hf = 0x1.8p1;
	if (a != 0xAB || b != 5 || c != 15 || d != 15 || e != 255 || f != 0) return 1;
	if (g != 2.5 || hf != 3.0) return 2;
	return 0;
}

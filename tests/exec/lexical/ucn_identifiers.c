extern int printf(const char *, ...);

int é = 10;
int café = 20;
int wide_\U000001ff = 30;

int α = 3;
int 中 = 7;
int À = 4;

int grαde = 100;
int grβde = 200;

struct box {
	int ü;
	int 中;
};

int föo(int x) {
	return x + 1;
}

#define CONCAT_ID(a, b) a##b
int CONCAT_ID(vå, lue) = 55;

int main(void) {
	int ok = 1;

	é += 1;
	ok &= (é == 11);
	ok &= (\U000000e9 == 11);
	ok &= (&é == & \U000000e9);

	café += 5;
	ok &= (café == 25);
	ok &= (&café == &caf\U000000e9);

	ok &= (wide_ǿ == 30);
	ok &= (wide_\U000001ff == 30);

	α += 1;
	ok &= (α == 4);
	ok &= (\U000003b1 == 4);
	ok &= (&α == & \U000003b1);

	中 += 1;
	ok &= (中 == 8);
	ok &= (\U00004e2d == 8);
	ok &= (&中 == & \U00004e2d);

	À += 1;
	ok &= (À == 5);
	ok &= (\U000000c0 == 5);
	ok &= (&À == & \U000000c0);

	ok &= (grαde == 100);
	ok &= (grβde == 200);
	ok &= (&grαde != &grβde);
	ok &= (&gr\U000003b1de == &grαde);
	ok &= (&gr\U000003b2de == &grβde);

	struct box b;
	b.ü = 1;
	b.中 = 2;
	ok &= (b.ü == 1);
	ok &= (b. \U000000fc == 1);
	ok &= (b.中 == 2);
	ok &= (b. \U00004e2d == 2);

	ok &= (föo(41) == 42);
	ok &= (f\U000000f6o(1) == 2);

	ok &= (vålue == 55);
	ok &= (v\U000000e5lue == 55);

	printf(ok ? "OK\n" : "FAIL\n");
	return ok ? 0 : 1;
}

extern int printf(const char *, ...);

_Static_assert(123'45'6 == 123456, "decimal separators");
_Static_assert(0'123 == 0123, "octal separators");
_Static_assert(0x1'23 == 0x123, "hex separators");
_Static_assert(0b1'01 == 0b101, "binary separators");
_Static_assert(0o3'703 == 0O3703, "0o prefix with separators");
_Static_assert(0o17 == 017, "0o prefix");
_Static_assert(0x0'e - 0xe == 0, "e after a separator is not an exponent");

#define m(x) 0
_Static_assert(m(1'2) + (3'4) == 34, "separators inside macro arguments");

int main(void) {
	int ok = 1;
	unsigned long long big = 18'446'744'073'709'551'615ULL;

	if (314'159e-0'5f != 3.14159f)
		ok = 0;
	if (big != ~0ULL)
		ok = 0;
	if (0o777 != 511)
		ok = 0;
	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}

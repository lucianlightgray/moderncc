enum U { UM = ~0ULL };
enum H { H1 = 0x8000000000000000ULL };
enum B { B1 = 0xFFFFFFFFU };
enum L { L1 = 0x100000000LL };
enum A { A1 = 1, A2 = 2 };
enum N { N1 = -1, N2 = 5 };

_Static_assert(sizeof(enum U) == 8, "unsigned 64-bit enumerator needs 8 bytes");
_Static_assert(sizeof(enum H) == 8, "2^63 enumerator needs 8 bytes");
_Static_assert(sizeof(enum B) == 4, "0xFFFFFFFF fits unsigned int");
_Static_assert(sizeof(enum L) == 8, "value above 32 bits needs 8 bytes");
_Static_assert(sizeof(enum A) == 4, "small enum is int-sized");
_Static_assert(sizeof(enum N) == 4, "small signed enum is int-sized");
_Static_assert(_Generic((enum U)0, unsigned long long: 1, unsigned long: 1, default: 0) == 1,
	"UM enum underlying type is unsigned");
_Static_assert(_Generic((enum H)0, unsigned long long: 1, unsigned long: 1, default: 0) == 1,
	"H1 enum underlying type is unsigned");

int main(void) {
	if (UM != ~0ULL) return 1;
	if ((unsigned long long)H1 != 0x8000000000000000ULL) return 2;
	if (N1 != -1) return 3;
	if (B1 != 0xFFFFFFFFU) return 4;
	return 0;
}

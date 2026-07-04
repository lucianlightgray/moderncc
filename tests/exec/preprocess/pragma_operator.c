extern int printf(const char *, ...);

_Pragma("pack(1)") struct Packed {
	char c;
	int i;
};
_Pragma("pack()") struct Normal {
	char c;
	int i;
};

#define DO_PRAGMA(x) _Pragma(#x)
DO_PRAGMA(pack(1))
struct MacroPacked {
	char c;
	int i;
};
DO_PRAGMA(pack())

int main(void) {
	int ok = sizeof(struct Packed) == 5 && sizeof(struct Normal) == 8 && sizeof(struct MacroPacked) == 5;
	printf(ok ? "OK\n" : "FAIL\n");
	return ok ? 0 : 1;
}

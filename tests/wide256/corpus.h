#ifndef MCC_WIDE256_CORPUS_H
#define MCC_WIDE256_CORPUS_H

#define W256_NOPER 18
#define W256_NSHIFT 14

static const unsigned long long w256_oper[W256_NOPER][4] = {
		{0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull},
		{0x0000000000000001ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull},
		{0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull},
		{0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x8000000000000000ull},
		{0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0x7fffffffffffffffull},
		{0x0000000000000000ull, 0x0000000000000001ull, 0x0000000000000000ull, 0x0000000000000000ull},
		{0xffffffffffffffffull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull},
		{0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000001ull, 0x0000000000000000ull},
		{0xffffffffffffffffull, 0xffffffffffffffffull, 0x0000000000000000ull, 0x0000000000000000ull},
		{0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000001ull},
		{0xfffffffffffffffdull, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull},
		{0xc4653601a52a5d1full, 0xffffffffffffffffull, 0xffffffffffffffffull, 0xffffffffffffffffull},
		{0x0123456789abcdefull, 0xfedcba9876543210ull, 0x00ff00ff00ff00ffull, 0x123456789abcdef0ull},
		{0xdeadbeefcafebabeull, 0x0000000000000000ull, 0xffffffffffffffffull, 0x0000000000000000ull},
		{0x00000000ffffffffull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull},
		{0x0000000100000000ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull},
		{0x7fffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull, 0x8000000000000000ull},
		{0x0000000000000007ull, 0x0000000000000000ull, 0x0000000000000000ull, 0x0000000000000000ull},
};

static const long long w256_shift[W256_NSHIFT] = {
		0, 1, 31, 32, 63, 64, 65, 127, 128, 191, 192, 255, 256, -5};

#define W256_FOLD_LIST(X)                                                      \
	X(0, 1)                                                                      \
	X(1, 2) X(2, 3) X(3, 4) X(4, 5) X(5, 6) X(6, 7) X(7, 8) X(8, 9) X(9, 10)     \
			X(10, 11) X(11, 12) X(12, 13) X(13, 14) X(14, 15) X(15, 16) X(16, 17)   \
					X(17, 1) X(3, 2) X(4, 2) X(2, 4) X(0, 3) X(12, 11) X(16, 10)

#define W256_NFOLD 24

#endif

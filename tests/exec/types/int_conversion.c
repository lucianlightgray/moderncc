#include <stdio.h>

unsigned char get_uc(void) {
	return 0x80;
}
signed char get_sc(void) {
	return 0x80;
}
unsigned short get_us(void) {
	return 0x8000;
}
signed short get_ss(void) {
	return 0x8000;
}

unsigned char (*volatile fp_uc)(void) = get_uc;
signed char (*volatile fp_sc)(void) = get_sc;
unsigned short (*volatile fp_us)(void) = get_us;
signed short (*volatile fp_ss)(void) = get_ss;

int promote_main(void) {
	int ok = 1;
	unsigned long long uc = fp_uc();
	signed long long sc = fp_sc();
	unsigned long long us = fp_us();
	signed long long ss = fp_ss();

	printf("uc=%llx sc=%llx us=%llx ss=%llx\n",
		   (unsigned long long)uc,
		   (unsigned long long)sc,
		   (unsigned long long)us,
		   (unsigned long long)ss);

	if (uc != 0x80) {
		printf("FAIL: uc not zero-extended\n");
		ok = 0;
	}
	if (sc != 0xffffffffffffff80LL) {
		printf("FAIL: sc not sign-extended\n");
		ok = 0;
	}
	if (us != 0x8000) {
		printf("FAIL: us not zero-extended\n");
		ok = 0;
	}
	if (ss != 0xffffffffffff8000LL) {
		printf("FAIL: ss not sign-extended\n");
		ok = 0;
	}

	printf("%s\n", ok ? "PASS" : "FAIL");
	return ok ? 0 : 1;
}

int cast_main(void) {
	int ok = 1;
	int x = 0x12345678;

	char c = (char)x + 1;
	unsigned char uc = (unsigned char)x + 1;
	short s = (short)x + 1;
	unsigned short us = (unsigned short)x + 1;

	printf("c=%x uc=%x s=%x us=%x\n",
		   (unsigned char)c, (unsigned)uc,
		   (unsigned short)s, (unsigned)us);

	if (c != (char)0x78 + 1) {
		printf("FAIL: char conversion\n");
		ok = 0;
	}
	if (uc != (unsigned char)0x78 + 1) {
		printf("FAIL: unsigned char conversion\n");
		ok = 0;
	}
	if (s != (short)0x5678 + 1) {
		printf("FAIL: short conversion\n");
		ok = 0;
	}
	if (us != (unsigned short)0x5678 + 1) {
		printf("FAIL: unsigned short conversion\n");
		ok = 0;
	}

	printf("%s\n", ok ? "PASS" : "FAIL");
	return ok ? 0 : 1;
}

int sign_main(void) {
	int ok = 1;
	int x = 0x80000000;
	long long y = (long long)x;

	printf("y=%llx\n", (unsigned long long)y);

	if (y != 0xffffffff80000000LL) {
		printf("FAIL: int→long long sign-extension\n");
		ok = 0;
	}

	x = 0x40000000;
	y = (long long)x;
	printf("y=%llx\n", (unsigned long long)y);

	if (y != 0x40000000LL) {
		printf("FAIL: int→long long positive value\n");
		ok = 0;
	}

	unsigned int ux = 0x80000000;
	long long uy = (long long)(int)ux;
	printf("uy=%llx\n", (unsigned long long)uy);

	if (uy != 0xffffffff80000000LL) {
		printf("FAIL: unsigned→int→long long sign-extension\n");
		ok = 0;
	}

	printf("%s\n", ok ? "PASS" : "FAIL");
	return ok ? 0 : 1;
}

int main() {
	return promote_main() | cast_main() | sign_main();
}

#include <stdatomic.h>
#include <stdio.h>

#define S(x) #x

atomic_char8_t g;

int main(void) {
	unsigned char a = u8'a';
	unsigned char b = u8'\xff';
	unsigned char c = u8'\377';
	const char *s = S(u8"t");
	atomic_store(&g, (unsigned char)7);
	if (a == 97 && b == 255 && c == 255 && u8'\0' == 0 &&
			s[0] == 'u' && s[1] == '8' && s[2] == '"' &&
			atomic_load(&g) == 7)
		printf("OK\n");
	return 0;
}

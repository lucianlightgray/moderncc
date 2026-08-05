#include <stdatomic.h>
#include <stdio.h>

#define S(x) #x

atomic_char8_t g;

_Static_assert(sizeof(u8'a') == 1, "u8 char is one byte");
_Static_assert(_Generic(u8'a', unsigned char: 1, default: 0) == 1, "u8 char type");
_Static_assert(_Generic(u8"text", unsigned char *: 1, default: 0) == 1, "u8 str type");

#if u8'\0' - 1 < 0
#error "u8 character constants must be unsigned in the preprocessor"
#endif
#if u'\0' - 1 < 0
#error "u character constants must be unsigned in the preprocessor"
#endif
#if U'\0' - 1 < 0
#error "U character constants must be unsigned in the preprocessor"
#endif

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

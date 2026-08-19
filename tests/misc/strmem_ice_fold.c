_Static_assert(__builtin_strlen("abcdef") == 6, "strlen abcdef");
_Static_assert(__builtin_strlen("") == 0, "strlen empty");
_Static_assert(__builtin_strlen("a\0b") == 1, "strlen stops at NUL");
_Static_assert(__builtin_memcmp("abc", "abc", 3) == 0, "memcmp equal");
_Static_assert(__builtin_memcmp("abc", "abd", 3) != 0, "memcmp diff");
_Static_assert(__builtin_memcmp("abd", "abc", 3) > 0, "memcmp gt");
_Static_assert(__builtin_memcmp("abc", "abd", 3) < 0, "memcmp lt");
_Static_assert(__builtin_memcmp("abc", "abz", 2) == 0, "memcmp prefix");
_Static_assert(__builtin_strcmp("abc", "abc") == 0, "strcmp equal");
_Static_assert(__builtin_strcmp("abc", "abd") < 0, "strcmp lt");
_Static_assert(__builtin_strcmp("abd", "abc") > 0, "strcmp gt");
_Static_assert(__builtin_strcmp("ab", "abc") < 0, "strcmp shorter lt");
_Static_assert(__builtin_strncmp("abcXX", "abcYY", 3) == 0, "strncmp eq prefix");
_Static_assert(__builtin_strncmp("abX", "abY", 3) != 0, "strncmp diff");
_Static_assert(__builtin_strncmp("abc", "abd", 0) == 0, "strncmp zero len");

int g_arr[__builtin_strlen("hello")];
static const int g_len = __builtin_strlen("test1234");

int main(void) {
	if (sizeof(g_arr) / sizeof(g_arr[0]) != 5) return 1;
	if (g_len != 8) return 2;

	int x = 3;
	switch (x) {
	case __builtin_strlen("abc"):
		break;
	default:
		return 3;
	}

	char buf[__builtin_strlen("hi") + 1];
	if (sizeof(buf) != 3) return 4;
	buf[0] = 'x';
	buf[2] = '\0';

	return 0;
}

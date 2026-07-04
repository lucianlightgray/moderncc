#include <stdio.h>
#include <string.h>

int main(void) {

	const char *s = "abc"
					"def"
					"ghi";
	printf("concat: %s len=%d\n", s, (int)strlen(s));

	const char *multi =
		"line one is here "
		"and continues here";
	printf("multi: %s\n", multi);

	printf("escapes: [%s]\n", "tab\tend\\backslash\"quote");

	printf("bytes: %d %d\n", "\101\102"[0], "\x43\x44"[1]);

	char buf[] = "ab\0cd";
	printf("embedded: strlen=%d sizeof=%d byte4=%d\n",
		   (int)strlen(buf), (int)sizeof(buf), buf[3]);

	printf("array: %c %c sizeof=%d\n",
		   "hello"[0], "hello"[4], (int)sizeof("hello"));
	return 0;
}

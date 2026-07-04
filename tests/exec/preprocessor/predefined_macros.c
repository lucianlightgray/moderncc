#include <stdio.h>
#include <string.h>

int main(void)
{

	printf("stdc: %d\n", __STDC__);
	printf("version: %ld\n", (long)__STDC_VERSION__);

	int a = __LINE__;
	int b = __LINE__;
	printf("line_delta: %d\n", b - a);

	int here = __LINE__; printf("same_line: %d\n", here == __LINE__);

	printf("file_ok: %d\n", strstr(__FILE__, "predefined_macros.c") != NULL);

	printf("date_len: %d time_len: %d\n",
		   (int)strlen(__DATE__), (int)strlen(__TIME__));
	return 0;
}

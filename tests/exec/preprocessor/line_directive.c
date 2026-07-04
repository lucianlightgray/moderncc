#include <stdio.h>
#include <string.h>

int main(void) {
#line 100
	int l100 = __LINE__;
	int l101 = __LINE__;
	printf("set100: %d %d\n", l100, l101);

#line 1
	int back = __LINE__;
	printf("reset: %d\n", back);

#line 42 "synthetic.c"

	printf("withfile: line=%d file_match=%d\n",
		   __LINE__, strstr(__FILE__, "synthetic.c") != NULL);

	printf("next: %d\n", __LINE__);
	return 0;
}

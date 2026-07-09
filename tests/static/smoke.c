#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

__thread int tls_i = 42;
__thread char tls_s[8] = "tls";

int main(void) {
	char *p = malloc(8);
	if (!p)
		return 1;
	strcpy(p, "heap");
	printf("i=%d s=%s digit=%d up=%c %s\n",
				 tls_i, tls_s, isdigit('5') != 0, toupper('a'), p);
	free(p);
	return 0;
}

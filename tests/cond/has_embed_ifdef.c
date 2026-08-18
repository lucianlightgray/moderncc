/* T-mac-30137: #ifdef/#elifdef __has_embed must agree with #if defined(__has_embed)
 * (mcc supports #embed), and #elifdef __has_include must also work. */
#include <stdio.h>
int main(void) {
	int a = 0, b = 0, c = 0, d = 0;
#ifdef __has_embed
	a = 1;
#endif
#if defined(__has_embed)
	b = 1;
#endif
#if 0
#elifdef __has_embed
	c = 1;
#endif
#if 0
#elifdef __has_include
	d = 1;
#endif
	printf("has_embed_ifdef a=%d b=%d c=%d d=%d\n", a, b, c, d);
	return (a == 1 && b == 1 && c == 1 && d == 1) ? 0 : 1;
}

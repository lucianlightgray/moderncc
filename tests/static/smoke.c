/* Static-link smoke test for the glibc fully-static path.
 *
 * Exercises the three linker fixes that make `mcc -static` work under glibc:
 *   - weak-undef guard via GOTPCREL (__libc_start_main's optional hooks) —
 *     any glibc-static startup path that reaches printf touches these;
 *   - GOTTPOFF IE->LE TLS relaxation + local-exec TPOFF (glibc's internal
 *     __thread state, reached through isdigit/toupper's ctype tables);
 *   - the .tdata TLS init image (must be copied, not zero-filled) — the
 *     nonzero-initialized __thread objects below read back their initializers.
 *
 * Output is normalized (isdigit compared !=0) so it is identical across
 * glibc and musl ctype encodings. Expected: "i=42 s=tls digit=1 up=A heap".
 */
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

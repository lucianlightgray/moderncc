/* TLS model coverage: link an object compiled in each of the four x86-64 TLS
 * models (general-dynamic, local-dynamic, initial-exec, local-exec) with mcc
 * and run it. mcc's own codegen only ever emits local-exec, so the GD/LD/IE
 * relaxation + pattern-match paths in {x86_64,i386}-link.c fire only when it
 * links external (gcc/clang) objects — this test drives exactly those.
 *
 * The reference compiler produces this TU under -ftls-model=<model>; mcc links
 * it both dynamically and (when a static libc is present) fully static. Both a
 * .tdata (initialized) and a .tbss (zero) __thread object are present so the
 * GD->LE relaxation's whole-TLS-block TP-offset computation is exercised.
 *
 * Expected stdout: "g=112 l=224".
 */
#include <stdio.h>

__thread int g_tls = 111;        /* .tdata; drives GD / IE / LE */
static __thread int l_tls = 222; /* .tdata, file-local; drives LD */
__thread long z_tls;             /* .tbss; forces a 2nd TLS section */

int get_g(void) { return g_tls; }
int get_l(void) { return l_tls; }

int main(void) {
	g_tls += 1;
	l_tls += 2;
	z_tls += 5;
	printf("g=%d l=%d\n", get_g(), get_l());
	return (get_g() == 112 && get_l() == 224 && z_tls == 5) ? 0 : 1;
}

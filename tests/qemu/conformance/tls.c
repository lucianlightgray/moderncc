/* Self-checking thread-local-storage conformance. _Thread_local objects need
   per-arch TLS relocations (R_*_TPOFF / R_*_DTPMOD, GD/LE/IE models) and a
   tp-relative access sequence that the dynamic loader and the target libc's TLS
   setup must agree with -- a place a from-scratch compiler diverges from the
   platform even when ordinary globals work. Single-threaded (we exercise the
   access sequence + relocations + initial-image copy, not concurrency).
   Endianness-independent; 0 on success. */

#include <string.h>

_Thread_local int counter = 100;            /* nonzero TLS initializer (.tdata) */
_Thread_local long widearr[4] = { 1, 2, 3, 4 };
static _Thread_local char name[8];          /* zero-init TLS (.tbss) */

int main(void)
{
    if (counter != 100) return 1;           /* .tdata image copied per thread */
    counter -= 58;
    if (counter != 42) return 2;

    long s = 0;
    for (int i = 0; i < 4; i++) s += widearr[i];
    if (s != 10) return 3;
    widearr[2] = 30;
    if (widearr[0] + widearr[2] != 31) return 4;

    if (name[0] != 0) return 5;             /* .tbss zero-initialized */
    strcpy(name, "tls");
    if (strcmp(name, "tls")) return 6;

    /* address of a TLS object is a runtime, thread-relative value; it must be a
       stable lvalue within the thread */
    char *p = name;
    p[3] = '!';
    if (name[3] != '!') return 7;

    return 0;
}

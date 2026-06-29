/* Self-checking thread-local conformance for aggregates, pointer initializers
   and &tls crossing a call boundary into libc -- codegen paths distinct from
   the scalar TLS load/store in tls.c:
     - _Thread_local struct: member access = &tls + member offset
     - _Thread_local pointer initialised to the address of a global: a static
       relocation living in .tdata (the per-thread init image), not .data
     - passing &tls_buffer to memset/snprintf: the tp-relative address must be
       materialised and handed to libc, then read back through direct TLS
   Endianness-independent; returns a distinct nonzero code per failure. */

#include <stdio.h>
#include <string.h>

struct kv { int k; long v; char tag[8]; };

static int global = 1234;

_Thread_local struct kv rec = { 7, 99, "hi" };
_Thread_local int *pg = &global;        /* .tdata relocation to a global */
static _Thread_local char buf[32];

int main(void)
{
    /* struct member access through TLS */
    if (rec.k != 7 || rec.v != 99 || strcmp(rec.tag, "hi")) return 1;
    rec.k += 3;
    rec.v *= 2;
    if (rec.k != 10 || rec.v != 198) return 2;

    /* &tls.member into libc, then read back directly */
    snprintf(rec.tag, sizeof rec.tag, "k%ld", rec.v);
    if (strcmp(rec.tag, "k198")) return 3;

    /* TLS pointer initialised to a global's address (relocated .tdata image) */
    if (pg != &global || *pg != 1234) return 4;
    *pg = 4321;
    if (global != 4321) return 5;

    /* pass a TLS buffer's address to memset/snprintf */
    memset(buf, 'x', 4);
    buf[4] = '\0';
    if (strcmp(buf, "xxxx")) return 6;
    int n = snprintf(buf, sizeof buf, "%d/%s", rec.k, rec.tag);
    if (n != 7 || strcmp(buf, "10/k198")) return 7;

    /* &tls and direct access must alias the same storage */
    char *p = buf;
    p[0] = 'Z';
    if (buf[0] != 'Z') return 8;

    return 0;
}

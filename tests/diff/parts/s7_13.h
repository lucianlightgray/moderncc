/* C9911 §7.13-§7.16 setjmp, signal, stdalign, stdarg (s7_13) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s7_13.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
static jmp_buf s7_13_jb0;
static jmp_buf s7_13_jb42;
static volatile sig_atomic_t s7_13_sigflag = 0;
static void s7_13_handler(int sig) { (void)sig; s7_13_sigflag = 1; }
static _Alignas(32) unsigned char s7_13_buf[32];
static _Alignas(double) unsigned char s7_13_da[1];

void s7_13_setjmp_signal_align(void)
{
    /* §7.13.2.1p4: longjmp(env,0) makes setjmp return 1, not 0.
       §7.13.2.1p3: a volatile automatic changed before longjmp keeps its value. */
    volatile int v = 0;
    int r0 = setjmp(s7_13_jb0);
    if (r0 == 0) { v = 5; longjmp(s7_13_jb0, 0); }
    printf("s7_13 setjmp val0 -> r=%d v=%d\n", r0, (int)v);

    /* §7.13.2.1p4: longjmp(env,42) makes setjmp return 42. */
    int r42 = setjmp(s7_13_jb42);
    if (r42 == 0) longjmp(s7_13_jb42, 42);
    printf("s7_13 setjmp val42 -> r=%d\n", r42);

    /* §7.14.1.1p2/p6, §7.14.2.1p2: raise synchronously invokes the installed
       handler (sets flag) and returns 0; a second signal() returns the most
       recent handler for that signal. Gated off on Windows: msvcrt lacks
       SIGUSR1, uses System-V (reset-on-delivery) signal() semantics unlike
       glibc, and mcc's bounds-checking cannot instrument the async handler that
       msvcrt invokes on raise() (it faults on PE). The differential is
       per-platform, so both mcc and the reference simply omit these lines. */
#if !defined(_WIN32)
    void (*prev)(int) = signal(SIGUSR1, s7_13_handler);
    s7_13_sigflag = 0;
    int rc = raise(SIGUSR1);
    printf("s7_13 signal flag=%d rc0=%d\n", (int)s7_13_sigflag, rc == 0);
    void (*prev2)(int) = signal(SIGUSR1, prev);
    printf("s7_13 signal prev_returned=%d\n", prev2 == s7_13_handler);
#endif

    /* §7.15p4: stdalign macros expand to integer constant 1. */
    printf("s7_13 stdalign defs=%d\n",
           __alignas_is_defined == 1 && __alignof_is_defined == 1);

    /* §6.5.3.4p1: _Alignof keyword yields the type alignment; array type ->
       alignment of element type; matches the __alignof__ extension. Only
       invariants are printed (never the impl-defined absolute value). */
    printf("s7_13 _Alignof int matches=%d\n",
           _Alignof(int) == __alignof__(int));
    printf("s7_13 _Alignof array=elem=%d\n",
           _Alignof(double[10]) == _Alignof(double));

    /* §6.7.5p1: _Alignas(type-name) == _Alignas(_Alignof(type-name)). */
    printf("s7_13 _Alignas typename=ice=%d\n",
           _Alignof(s7_13_da) == _Alignof(double));

    /* §6.7.5p1: a static object over-aligned with _Alignas is actually aligned. */
    printf("s7_13 _Alignas32 aligned=%d\n",
           ((uintptr_t)(void *)s7_13_buf) % 32u == 0);
}

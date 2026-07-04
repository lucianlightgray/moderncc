static jmp_buf s6_7_4_jb;

noreturn static void s6_7_4_never(int v) {
    longjmp(s6_7_4_jb, v);
}

struct s6_7_4_ms {
    char c;
    _Alignas(16) int a;
};

void s6_7_4_specifiers(void) {
    _Alignas(64) static char s6_7_4_buf[8];
    _Alignas(8) _Alignas(16) static char s6_7_4_m[4];
    _Alignas(double) static char s6_7_4_d1[4];
    _Alignas(_Alignof(double)) static char s6_7_4_d2[4];
    _Alignas(0) int z = 5;
    int r;

    printf("alignof char==1: %d\n", (int)(_Alignof(char) == 1));
    printf("alignof(int)<=alignof(double): %d\n", (int)(_Alignof(int) <= _Alignof(double)));
    printf("alignas64 ok: %d\n", (int)(((unsigned long)(void *)s6_7_4_buf & 63u) == 0));
    printf("alignas0 val: %d\n", z);
    printf("strictest16: %d\n", (int)(((unsigned long)(void *)s6_7_4_m & 15u) == 0));
    printf("alignas type==const: %d\n",
           (int)((((unsigned long)(void *)s6_7_4_d1 % _Alignof(double)) == 0) &&
                 (((unsigned long)(void *)s6_7_4_d2 % _Alignof(double)) == 0)));
    printf("member raises: %d\n", (int)(_Alignof(struct s6_7_4_ms) == 16));
    printf("alignas macro: %d\n", (int)(alignof(int) == _Alignof(int)));
    printf("alignof macro type: %d\n", (int)(alignof(double) == _Alignof(double)));

    r = setjmp(s6_7_4_jb);
    if (r == 0) {
        s6_7_4_never(42);
        printf("UNREACHABLE\n");
    } else {
        printf("noreturn escaped: %d\n", r);
    }
}

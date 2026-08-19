#include <stdio.h>
__attribute__((constructor(200))) static void c_200(void) { putchar('2'); }
__attribute__((constructor(100))) static void c_100(void) { putchar('1'); }
__attribute__((constructor)) static void c_def(void) { putchar('D'); }
__attribute__((destructor(200))) static void d_200(void) { putchar('y'); }
__attribute__((destructor(100))) static void d_100(void) { putchar('x'); }
__attribute__((destructor)) static void d_def(void) { putchar('z'); }
int main(void) { putchar('M'); return 0; }

#include <stdlib.h>
#include <stdio.h>

#define MAX_DIGITS 32
#define NO_MAIN

void topline(int d, char *p) {

    *p++ = ' ';
    switch (d) {

    case 0:
    case 2:
    case 3:
    case 5:
    case 7:
    case 8:
    case 9:
        *p++ = '_';
        break;
    default:
        *p++ = ' ';
    }
    *p++ = ' ';
}

void midline(int d, char *p) {

    switch (d) {

    case 0:
    case 4:
    case 5:
    case 6:
    case 8:
    case 9:
        *p++ = '|';
        break;
    default:
        *p++ = ' ';
    }
    switch (d) {

    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 8:
    case 9:
        *p++ = '_';
        break;
    default:
        *p++ = ' ';
    }
    switch (d) {

    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 7:
    case 8:
    case 9:
        *p++ = '|';
        break;
    default:
        *p++ = ' ';
    }
}

void botline(int d, char *p) {

    switch (d) {

    case 0:
    case 2:
    case 6:
    case 8:
        *p++ = '|';
        break;
    default:
        *p++ = ' ';
    }
    switch (d) {

    case 0:
    case 2:
    case 3:
    case 5:
    case 6:
    case 8:
        *p++ = '_';
        break;
    default:
        *p++ = ' ';
    }
    switch (d) {

    case 0:
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
        *p++ = '|';
        break;
    default:
        *p++ = ' ';
    }
}

void print_led(unsigned long x, char *buf) {

    int i = 0, n;
    static int d[MAX_DIGITS];

    n = (x == 0L ? 1 : 0);

    while (x) {
        d[n++] = (int)(x % 10L);
        if (n >= MAX_DIGITS)
            break;
        x = x / 10L;
    }

    for (i = n - 1; i >= 0; i--) {
        topline(d[i], buf);
        buf += 3;
        *buf++ = ' ';
    }
    *buf++ = '\n';

    for (i = n - 1; i >= 0; i--) {
        midline(d[i], buf);
        buf += 3;
        *buf++ = ' ';
    }
    *buf++ = '\n';

    for (i = n - 1; i >= 0; i--) {
        botline(d[i], buf);
        buf += 3;
        *buf++ = ' ';
    }
    *buf++ = '\n';
    *buf = '\0';
}

int main() {
    char buf[5 * MAX_DIGITS];
    print_led(1234567, buf);
    printf("%s\n", buf);

    return 0;
}

#ifndef NO_MAIN
int main(int argc, char **argv) {

    int i = 0, n;
    long x;
    static int d[MAX_DIGITS];
    char buf[5 * MAX_DIGITS];

    if (argc != 2) {
        fprintf(stderr, "led: usage: led integer\n");
        return 1;
    }

    x = atol(argv[1]);

    if (x < 0) {
        fprintf(stderr, "led: %d must be non-negative\n", x);
        return 1;
    }

    print_led(x, buf);
    printf("%s\n", buf);

    return 0;
}
#endif

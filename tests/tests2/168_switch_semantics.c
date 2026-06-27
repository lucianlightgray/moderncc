/* C99 6.8.4.2: the switch statement. Control jumps to the matching case (or to
   default, which need not be last); execution then falls through subsequent
   labels until a break. Case labels are integer constant expressions and must
   be unique. Duff's device demonstrates that case labels can appear inside a
   nested statement and interleave with a loop. */
#include <stdio.h>

/* classify with intentional fallthrough accumulating a result */
static int classify(int x)
{
    int acc = 0;
    switch (x) {
    case 1:
        acc += 1;          /* fall through */
    case 2:
        acc += 2;          /* fall through */
    case 3:
        acc += 3;
        break;             /* stop here */
    default:
        acc = -1;
        break;
    case 9:                /* default is not required to be last */
        acc = 99;
        break;
    }
    return acc;
}

/* Duff's device: case labels interleaved with a do/while loop */
static void copy_n(const char *src, char *dst, int n)
{
    int count = (n + 3) / 4;
    int i = 0;
    switch (n % 4) {
    case 0: do { dst[i] = src[i]; i++;
    case 3:      dst[i] = src[i]; i++;
    case 2:      dst[i] = src[i]; i++;
    case 1:      dst[i] = src[i]; i++;
            } while (--count > 0);
    }
}

int main(void)
{
    printf("classify: %d %d %d %d %d\n",
           classify(1), classify(2), classify(3), classify(5), classify(9));

    char dst[16] = {0};
    copy_n("abcdefg", dst, 7);
    printf("duff: %s\n", dst);

    /* a switch on a single value with only default still runs default */
    int hit = 0;
    switch (42) {
    case 0: hit = 1; break;
    default: hit = 2; break;
    }
    printf("defonly: %d\n", hit);
    return 0;
}

#include <setjmp.h>
#include <stdlib.h>

static jmp_buf jb;

static int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static void deep(int n) {
    volatile int local = n * 2;
    if (n == 0)
        longjmp(jb, 42);
    deep(n - 1);
    (void)local;
}

int main(void) {

    volatile int marker = 12345;
    int keep = 678;

    int rc = setjmp(jb);
    if (rc == 0) {
        deep(5);
        return 1;
    }
    if (rc != 42)
        return 2;
    if (marker != 12345)
        return 3;
    if (keep != 678)
        return 4;

    int arr[] = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0};
    qsort(arr, sizeof arr / sizeof arr[0], sizeof arr[0], cmp_int);
    for (int i = 0; i < 10; i++)
        if (arr[i] != i)
            return 5;

    return 0;
}

#include <stdio.h>
#include <stdlib.h>

#define TRUE 1
#define FALSE 0

#define N 4

int A[N], B[N], C[N];

void Hanoi(int, int *, int *, int *);

void PrintAll() {
    int i;

    printf("A: ");
    for (i = 0; i < N; i++)
        printf(" %d ", A[i]);
    printf("\n");

    printf("B: ");
    for (i = 0; i < N; i++)
        printf(" %d ", B[i]);
    printf("\n");

    printf("C: ");
    for (i = 0; i < N; i++)
        printf(" %d ", C[i]);
    printf("\n");
    printf("------------------------------------------\n");
    return;
}

int Move(int *source, int *dest) {
    int i = 0, j = 0;

    while (i < N && (source[i]) == 0)
        i++;
    while (j < N && (dest[j]) == 0)
        j++;

    dest[j - 1] = source[i];
    source[i] = 0;
    PrintAll();
    return dest[j - 1];
}

void Hanoi(int n, int *source, int *dest, int *spare) {
    int i;
    if (n == 1) {
        Move(source, dest);
        return;
    }

    Hanoi(n - 1, source, spare, dest);
    Move(source, dest);
    Hanoi(n - 1, spare, dest, source);
    return;
}

int main() {
    int i;

    for (i = 0; i < N; i++)
        A[i] = i + 1;
    for (i = 0; i < N; i++)
        B[i] = 0;
    for (i = 0; i < N; i++)
        C[i] = 0;

    printf("Solution of Tower of Hanoi Problem with %d Disks\n\n", N);

    printf("Starting state:\n");
    PrintAll();
    printf("\n\nSubsequent states:\n\n");

    Hanoi(N, A, B, C);

    return 0;
}

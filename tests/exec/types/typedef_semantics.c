#include <stdio.h>

typedef unsigned long ulong_t;
typedef int int_array5[5];
typedef struct node {
    int val;
    struct node *next;
} node_t;
typedef int (*binop)(int, int);

static int add(int a, int b) {
    return a + b;
}
static int mul(int a, int b) {
    return a * b;
}

int main(void) {

    ulong_t big = 4000000000UL;
    printf("ulong: %lu\n", big);

    int_array5 arr = {1, 2, 3, 4, 5};
    int s = 0;
    for (int i = 0; i < 5; i++)
        s += arr[i];
    printf("arr: count=%d sum=%d\n", (int)(sizeof arr / sizeof arr[0]), s);

    node_t c = {3, NULL};
    node_t b = {2, &c};
    node_t a = {1, &b};
    int chain = a.val + a.next->val + a.next->next->val;
    printf("list: %d\n", chain);

    binop ops[2] = {add, mul};
    printf("ops: %d %d\n", ops[0](6, 7), ops[1](6, 7));

    unsigned long via_builtin = big + 1;
    ulong_t via_typedef = big + 1;
    printf("compat: %d\n", via_builtin == via_typedef);
    return 0;
}

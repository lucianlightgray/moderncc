/* C23 / GCC fixed underlying type for enums: `enum E : T`.
   The base type controls sizeof(enum) and enumerator range. */
#include <stdio.h>

enum small : unsigned char { A = 1, B = 200 };
enum wide  : long long    { C = 1, D = 1LL << 40 };

int main(void)
{
    printf("%zu %zu\n", sizeof(enum small), sizeof(enum wide));
    printf("%d %d %lld\n", A, B, (long long)D);
    return 0;
}

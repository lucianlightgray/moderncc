/* C99 6.7.1 storage-class specifiers and 6.9.2 external object definitions.
   Exercises tentative definitions (a file-scope `int x;` with no initializer
   may appear repeatedly and yields a single, zero-initialized object), static
   storage-duration zero initialization, function-local static persistence, and
   register/auto locals. */
#include <stdio.h>

/* tentative definitions: these refer to one and the same object, which is
   zero-initialized because no definition supplies an initializer */
int tent;
int tent;

/* a file-scope object with static storage is zero-initialized */
static int zero_static;
static int zarr[4];

/* internal vs external linkage both have static storage duration */
static long counter_seed = 41;

int next_id(void)
{
    /* function-local static: initialized once, persists across calls */
    static int id = 0;
    return ++id;
}

int main(void)
{
    printf("tentative: %d\n", tent);            /* 0 */
    printf("zero: %d %d\n", zero_static, zarr[3]);

    /* the static local keeps its value between calls (sequence the calls
       explicitly -- argument evaluation order is unspecified) */
    int id1 = next_id(), id2 = next_id(), id3 = next_id();
    printf("ids: %d %d %d\n", id1, id2, id3);

    /* register is just a hint; the variable behaves like any auto local */
    register int r = 7;
    int sum = 0;
    for (int i = 0; i < r; i++)
        sum += i;
    printf("regsum: %d\n", sum);                /* 0+..+6 = 21 */

    printf("seed: %ld\n", counter_seed + 1);    /* 42 */
    return 0;
}

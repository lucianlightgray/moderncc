/* GNU __attribute__((transparent_union)): a function parameter declared with a
   transparent-union type accepts an argument of any of the union's member types
   with no explicit cast (the value is passed using the first member's calling
   convention). This is the mechanism glibc historically used for socket APIs
   such as bind()/connect() taking either "struct sockaddr *" or a more specific
   pointer. Supported by gcc and clang as an extension; verified 3-way. */
#include <stdio.h>

struct a { int x; };
struct b { int y; };

typedef union {
    struct a *pa;
    struct b *pb;
    int *pi;
} __attribute__((transparent_union)) U;

/* every member points at an int-first layout, so reading through pi is valid
   regardless of which member the caller supplied */
static int first_int(U u) { return *u.pi; }

int main(void)
{
    struct a A = { 11 };
    struct b B = { 22 };
    int i = 33;
    U var;
    var.pa = &A;

    printf("%d\n", first_int(&A));          /* struct a*  -> exact member  */
    printf("%d\n", first_int(&B));          /* struct b*  -> exact member  */
    printf("%d\n", first_int(&i));          /* int*       -> exact member  */
    printf("%d\n", first_int(var));         /* the union value itself      */
    printf("%d\n", first_int((U){ .pb = &B })); /* a compound-literal union */
    return 0;
}

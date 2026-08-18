/* T-mac-30186: `int z = {{5}}` (extra braces around a scalar initializer) is a
 * C11 6.7.9p11 constraint violation. mcc only diagnosed it under -Wall or
 * -pedantic; at the default level it compiled silently. Now a default-ON
 * warning "too many braces around scalar initializer" fires (error under
 * -Werror). gcc-16 errors here, clang warns. */
int z = {{5}};

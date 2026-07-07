/* increment.h — recursively re-includes itself, bumping INCREMENTING to its
 * successor each pass via an explicit table, until the depth gate stops it.
 * Exercises the CST capturing one header re-included several times under a
 * changing PP context (all passes hash-cons to ONE SourceFile template). */
#if (INCREMENTING) < 3
int ICAT(level_, INCREMENTING) = INCREMENTING; /* distinct decl per depth */

#if (INCREMENTING) == 1
#undef INCREMENTING
#define INCREMENTING 2 /* redefine using the previous value's successor */
#elif (INCREMENTING) == 2
#undef INCREMENTING
#define INCREMENTING 3
#endif

#include "increment.h" /* recursive re-include */
#endif

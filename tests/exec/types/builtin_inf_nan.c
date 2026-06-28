/* Built-in IEEE float tokens __inf__ / __nan__ (back <math.h> macros). */
#include <stdio.h>

int main(void)
{
    float inf = __inf__;
    float nan = __nan__;
    printf("%d %d %d\n",
           inf > 1e30f,          /* +infinity compares above any finite */
           inf == inf,           /* infinity is ordered with itself */
           nan != nan);          /* NaN is unordered with itself */
    return 0;
}

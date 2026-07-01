/* Standalone unit test for s_stddef — compiled 3-way by the parts-suite. */
#include "std_env.h"
#include "s_stddef.h"

int main(void)
{
    s_stddef_offsetof();
    s_stddef_stdint();
    return 0;
}

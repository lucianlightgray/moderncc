/* Standalone unit test for s7_9 — compiled 3-way by the parts-suite. */
#include "std_env.h"
#include "s7_9.h"

int main(void)
{
    s7_9_iso646_test();
    s7_9_limits_test();
    s7_9_locale_test();
    return 0;
}

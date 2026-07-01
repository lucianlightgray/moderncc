/* Standalone unit test for s7_23 — compiled 3-way by the parts-suite. */
#include "std_env.h"
#include "s7_23.h"

int main(void)
{
    s7_23_string_test();
    s7_23_tgmath_test();
    return 0;
}

/* Standalone unit test for s7_22 — compiled 3-way by the parts-suite. */
#include "std_env.h"
#include "s7_22.h"

int main(void)
{
    s7_22_strtol_test();
    s7_22_intarith_test();
    s7_22_sortsearch_test();
    s7_22_mem_test();
    return 0;
}

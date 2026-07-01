/* Standalone unit test for s7_25 — compiled 3-way by the parts-suite. */
#include "std_env.h"
#include "s7_25.h"

int main(void)
{
    s7_25_asctime();
    s7_25_strftime();
    s7_25_mktime_norm();
    s7_25_difftime();
    return 0;
}

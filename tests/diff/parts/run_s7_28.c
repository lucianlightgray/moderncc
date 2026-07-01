/* Standalone unit test for s7_28 — compiled 3-way by the parts-suite. */
#include "std_env.h"
#include "s7_28.h"

int main(void)
{
#ifdef PARTS_STD_HAVE_UCHAR
    s7_28_uchar();
#endif
    s7_28_wconv();
    s7_28_wstr();
    s7_28_wmem();
    s7_28_wnum();
    s7_28_wprintf();
    s7_28_wctypef();
    s7_28_wtok();
    return 0;
}

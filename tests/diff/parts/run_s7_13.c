#include "std_env.h"
#include "s7_13.h"

int main(void) {
    s7_13_setjmp_signal_align();
    return 0;
}

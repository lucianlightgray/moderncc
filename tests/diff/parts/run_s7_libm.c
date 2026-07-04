#include "std_env.h"
#include "s7_libm.h"

int main(void) {
    s7_6_fenv_test();
    s7_1_complex_libm_test();
    s7_23_tgmath_eval_test();
    return 0;
}

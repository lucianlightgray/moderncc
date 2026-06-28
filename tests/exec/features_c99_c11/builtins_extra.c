/* GNU/Clang __builtin_* that lacked dedicated coverage:
   __builtin_constant_p, __builtin_choose_expr, __builtin_types_compatible_p,
   __builtin_frame_address, __builtin_unreachable. */
#include <stdio.h>
#include "vlog.h"          /* DEFINE-gated verbose tracing (-DVLOG_ENABLE) */

static int runtime_val(int x) { return x; }

/* __builtin_unreachable: control never falls past the returns. */
static int classify(int x)
{
    if (x > 0) return 1;
    if (x < 0) return -1;
    if (x == 0) return 0;
    __builtin_unreachable();
}

int main(void)
{
    int r = runtime_val(7);
    VLOG_ENTER("main");
    VLOG_VALUE("%d", r);

    /* constant_p: literal-folded expr is constant, a runtime var is not. */
    VLOG("constant_p(1+2)=%d constant_p(r)=%d",
         __builtin_constant_p(1 + 2), __builtin_constant_p(r));
    printf("cp: %d %d %d\n",
           __builtin_constant_p(1 + 2),
           __builtin_constant_p(r),
           __builtin_constant_p("s"));

    /* choose_expr: pick by compile-time constant; unchosen arm not evaluated. */
    int c = __builtin_choose_expr(sizeof(int) == 4, 7, r / 0);
    printf("ce: %d %zu\n", c,
           sizeof(__builtin_choose_expr(0, 0, (char)0)));

    /* types_compatible_p: qualifiers stripped; distinct pointee types differ. */
    printf("tc: %d %d\n",
           __builtin_types_compatible_p(int, const int),
           __builtin_types_compatible_p(int *, char *));

    /* frame_address(0) is the current frame: non-NULL. */
    printf("fa: %d\n", __builtin_frame_address(0) != 0);

    printf("ur: %d %d %d\n", classify(5), classify(-5), classify(0));
    return 0;
}

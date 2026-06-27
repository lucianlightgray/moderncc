/* C99 __func__: a predefined static const char[] holding the unadorned name
   of the enclosing function. */
#include <stdio.h>
#include <string.h>

static const char *who(void)
{
    return __func__;
}

static int length_of_name(void)
{
    /* __func__ is a real array object: strlen sees the function's own name */
    return (int)strlen(__func__);
}

void a_rather_long_function_name(void)
{
    printf("long: %s (%d)\n", __func__, (int)sizeof(__func__));
}

int main(void)
{
    printf("here: %s\n", __func__);
    printf("callee: %s\n", who());
    printf("len: %d\n", length_of_name());

    /* __func__ is usable inside nested blocks and still names the function */
    {
        {
            printf("nested: %s\n", __func__);
        }
    }

    /* it is an lvalue array, so comparisons and indexing work */
    printf("match=%d first=%c\n", strcmp(__func__, "main") == 0, __func__[0]);

    a_rather_long_function_name();
    return 0;
}

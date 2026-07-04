#include <stdio.h>
#include <string.h>

static const char *who(void) {
    return __func__;
}

static int length_of_name(void) {

    return (int)strlen(__func__);
}

void a_rather_long_function_name(void) {
    printf("long: %s (%d)\n", __func__, (int)sizeof(__func__));
}

int main(void) {
    printf("here: %s\n", __func__);
    printf("callee: %s\n", who());
    printf("len: %d\n", length_of_name());

    {
        {
            printf("nested: %s\n", __func__);
        }
    }

    printf("match=%d first=%c\n", strcmp(__func__, "main") == 0, __func__[0]);

    a_rather_long_function_name();
    return 0;
}






#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "libmcc.h"

void handle_error(void *opaque, const char *msg)
{
    fprintf(opaque, "%s\n", msg);
}


int add(int a, int b)
{
    return a + b;
}


const char hello[] = "Hello World!";

char my_program[] =
"#include <mcclib.h>\n"
"extern int add(int a, int b);\n"
"#ifdef _WIN32\n"
" __attribute__((dllimport))\n"
"#endif\n"
"extern const char hello[];\n"
"int fib(int n)\n"
"{\n"
"    if (n <= 2)\n"
"        return 1;\n"
"    else\n"
"        return fib(n-1) + fib(n-2);\n"
"}\n"
"\n"
"int foo(int n)\n"
"{\n"
"    printf(\"%s\\n\", hello);\n"
"    printf(\"fib(%d) = %d\\n\", n, fib(n));\n"
"    printf(\"add(%d, %d) = %d\\n\", n, 2 * n, add(n, 2 * n));\n"
"    return 0;\n"
"}\n";

int main(int argc, char **argv)
{
    MCCState *s;
    int i;
    int (*func)(int);

    s = mcc_new();
    if (!s) {
        fprintf(stderr, "Could not create mcc state\n");
        exit(1);
    }


    mcc_set_error_func(s, stderr, handle_error);


    for (i = 1; i < argc; ++i) {
        char *a = argv[i];
        if (a[0] == '-') {
            if (a[1] == 'B')
                mcc_set_lib_path(s, a+2);
            else if (a[1] == 'I')
                mcc_add_include_path(s, a+2);
            else if (a[1] == 'L')
                mcc_add_library_path(s, a+2);
        }
    }


    mcc_set_output_type(s, MCC_OUTPUT_MEMORY);

    if (mcc_compile_string(s, my_program) == -1)
        return 1;



    mcc_add_symbol(s, "add", add);
    mcc_add_symbol(s, "hello", hello);


    if (mcc_relocate(s) < 0)
        return 1;


    func = mcc_get_symbol(s, "foo");
    if (!func)
        return 1;


    func(32);


    mcc_delete(s);

    return 0;
}

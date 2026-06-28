


#include <stdio.h>

int main(void)
{

    printf("simple: %d %d %d %d %d %d %d\n",
           '\a', '\b', '\f', '\n', '\r', '\t', '\v');
    printf("punct: %d %d %d %d\n", '\\', '\'', '\"', '\?');
    printf("nul: %d\n", '\0');


    printf("octal: %d %d %d\n", '\101', '\0', '\177');


    printf("hex: %d %d\n", '\x41', '\x7e');


    char nl = '\n';
    printf("isnl: %d\n", nl == 10);


    printf("arith: %d\n", '\x10' + '\020');


    printf("type: %d\n", (int)sizeof('A'));
    return 0;
}

#include <string.h>

_Thread_local int counter = 100;
_Thread_local long widearr[4] = {1, 2, 3, 4};
static _Thread_local char name[8];

int main(void) {
    if (counter != 100)
        return 1;
    counter -= 58;
    if (counter != 42)
        return 2;

    long s = 0;
    for (int i = 0; i < 4; i++)
        s += widearr[i];
    if (s != 10)
        return 3;
    widearr[2] = 30;
    if (widearr[0] + widearr[2] != 31)
        return 4;

    if (name[0] != 0)
        return 5;
    strcpy(name, "tls");
    if (strcmp(name, "tls"))
        return 6;

    char *p = name;
    p[3] = '!';
    if (name[3] != '!')
        return 7;

    return 0;
}

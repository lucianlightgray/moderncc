#include <stdio.h>

struct string {
    char *str;
    int len;
};

void dummy(struct string fpath, int dump_arg) {
}

int main() {
    int a = 1;
    struct string x;
    x.str = "gg.v";
    x.len = 4;
    dummy(x, a == 0);
    printf("done\n");
    return 0;
}

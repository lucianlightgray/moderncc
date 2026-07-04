extern int printf(const char *, ...);

int a1[];
int a2[];
int a2[3] = {7, 8, 9};
int a3[];
int a3[4];

int main(void) {
    a1[0] = 5;
    int ok = a1[0] == 5 && sizeof a2 == 12 && a2[2] == 9 && sizeof a3 == 16;
    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}

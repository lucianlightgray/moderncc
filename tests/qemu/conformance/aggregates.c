struct point {
    int x, y;
};

static struct point padd(struct point a, struct point b) {
    struct point r;
    r.x = a.x + b.x;
    r.y = a.y + b.y;
    return r;
}

union bytes {
    int i;
    unsigned char b[4];
};

struct bits {
    unsigned a : 3, b : 5, c : 24;
};

int main(void) {
    struct point a = {1, 2}, b = {3, 4};
    struct point p = padd(a, b);
    if (p.x != 4 || p.y != 6)
        return 1;

    int arr[5];
    int i;
    for (i = 0; i < 5; i++)
        arr[i] = i * i;
    if (arr[0] != 0 || arr[4] != 16)
        return 2;

    union bytes u;
    u.i = 0x01020304;
    if (u.b[0] + u.b[1] + u.b[2] + u.b[3] != 0x0A)
        return 3;

    struct bits bf;
    bf.a = 5;
    bf.b = 17;
    bf.c = 1000000;
    if (bf.a != 5 || bf.b != 17 || bf.c != 1000000)
        return 4;

    return 0;
}

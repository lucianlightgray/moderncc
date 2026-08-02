int add3(int a, int b) { return a + b * 3; }
long long mix(long long a, long long b, long long c) {
    return (a << 2) + (b ^ c) - (a & b);
}
unsigned udiv(unsigned x, unsigned y) { return x / y + x % y; }

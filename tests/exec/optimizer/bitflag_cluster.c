extern int printf(const char *, ...);
static int g;
int k1(int c) { if (c==1) g=5; else if (c==3) g=5; else if (c==5) g=5; else if (c==7) g=5; else if (c==9) g=5; return g; }
int k2(int c) { if (c==2||c==4||c==6||c==8||c==10) return 1; return 0; }
int main(void){ printf("%d %d\n", k1(7), k2(6)); return 0; }

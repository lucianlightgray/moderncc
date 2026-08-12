extern int printf(const char *, ...);
static int bar(int x){ if (x==5 || x==19 || x==23 | x==26 || x==65) return 1; return 3; }
static int baz(int x){ if (x==7 && x!=8 & x!=9 && x<40) return 2; return 4; }
static int qux(int x){ return (x>3 | x<1) + (x==2 ^ x==3) + (x>=9 & x<=11); }
int main(void){
	int i, t = 0;
	for (i = 0; i < 70; i++)
		t = t * 31 + bar(i) + baz(i) * 5 + qux(i) * 7;
	printf("%d\n", t);
	return 0;
}

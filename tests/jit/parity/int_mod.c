extern int printf(const char *, ...);
static int h(int a, int b, int n){int i,s=0;for(i=0;i<n;i++)s+=(a*i+b)%13;return s;}
int main(void){int k,t=0;for(k=0;k<400;k++)t+=h(k,3,200);printf("%d\n",t);return 0;}

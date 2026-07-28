extern int printf(const char *, ...);
static double g(double a, double b, int n){int i;double s=0;for(i=0;i<n;i++)s=s*0.999+a*b;return s;}
int main(void){int k;double t=0;for(k=1;k<=300;k++)t+=g(k*0.5,0.25,200);printf("%.15g\n",t);return 0;}

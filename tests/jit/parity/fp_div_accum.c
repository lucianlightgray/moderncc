extern int printf(const char *, ...);
static double f(double x, int n){int i;double s=0;for(i=0;i<n;i++)s+=x*x/(i+1.5);return s;}
int main(void){int k;double t=0;for(k=1;k<=400;k++)t+=f(k*0.01,150);printf("%.15g\n",t);return 0;}

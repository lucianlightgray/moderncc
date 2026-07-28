extern int printf(const char *, ...);
static float ff(float x, int n){int i;float s=0;for(i=0;i<n;i++)s+=x/(i+2.0f);return s;}
int main(void){int k;double t=0;for(k=1;k<=300;k++)t+=ff(k*0.01f,150);printf("%.9g\n",t);return 0;}

#include <stdio.h>

int fred(int p)
{
   printf("yo %d\n", p);
   return 42;
}

int (*f)(int) = &fred;



int (*fprintfptr)(FILE *, const char *, ...) = &fprintf;

int main()
{
   fprintfptr(stdout, "%d\n", (*f)(24));

   return 0;
}



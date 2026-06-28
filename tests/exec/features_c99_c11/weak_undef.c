#include <stdio.h>
int absent_weak(void) __attribute__((weak));
int present_weak(void) __attribute__((weak));
int present_weak(void){ return 7; }
int main(void){
  printf("%d %d\n", &absent_weak == 0, (&present_weak != 0) ? present_weak() : -1);
  return 0;
}

#include <stdio.h>

void backward_target(void) { printf("back\n"); }
void backward_alias(void) __attribute__((alias("backward_target")));

void forward_alias(void) __attribute__((alias("forward_target")));
void forward_target(void) { printf("fwd\n"); }

int main(void) {
	backward_alias();
	forward_alias();
	return 0;
}

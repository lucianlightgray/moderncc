#include <stdio.h>

extern int in_custom;
extern int appended_custom;
extern int in_data_after_pop;
extern int after_previous;

asm(
		".section .mydata,\"aw\"\n"
		".globl in_custom\n"
		"in_custom: .long 0x1111\n"

		".data\n"
		".pushsection .mydata,\"aw\"\n"
		".globl appended_custom\n"
		"appended_custom: .long 0x2222\n"
		".popsection\n"
		".globl in_data_after_pop\n"
		"in_data_after_pop: .long 0x3333\n"

		".section .other,\"aw\"\n"
		".data\n"
		".previous\n"
		".globl after_previous\n"
		"after_previous: .long 0x4444\n");

int main(void) {
	printf("%x %x %x %x\n",
				 in_custom, appended_custom, in_data_after_pop, after_previous);
	return 0;
}

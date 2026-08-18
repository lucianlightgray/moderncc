	.data
	.globl bin_q
bin_q:
	.quad 0b1111111111
	.globl bin_l
bin_l:
	.long 0b100000001
	.globl bin_b
bin_b:
	.byte 0b1100

	.text
	.globl bin_imm
bin_imm:
	movl $0b101010, %eax
	ret

	.data
	.globl wtgt
wtgt:
	.byte 0xcd
	.globl wref32
wref32:
	.word wtgt
	.globl wref_diff
wref_diff:
	.word wref32 - wtgt

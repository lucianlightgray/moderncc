/* N39: branches to a label inside a riscv64 top-level __asm__ block.
 *
 * mcc's own riscv64 assembler is the subject here, not gas, so this file is
 * MCC-ONLY and riscv64-only; everywhere else it is a trivial pass so the exec
 * goldens stay identical across targets.
 *
 * Every instruction below is written in its base form -- no pseudo-ops, no
 * `li`, no `ret`, no `mv` -- because a pseudo-op that the built-in assembler
 * does not implement would fail this file for a reason that has nothing to do
 * with what it is testing.
 *
 * The three shapes are exactly the three that were observed broken:
 *   1. a conditional branch to a forward LOCAL label   (bnez a0, 1f)
 *   2. a conditional branch to a forward NAMED label   (bnez a0, .Lrv_named)
 *   3. a conditional branch to a backward local label  (bnez a0, 1b), i.e. a loop
 */

extern int printf(const char *, ...);

static int fails;
#define CK(cond)                            \
	do {                                      \
		if (!(cond)) {                          \
			printf("FAIL line %d\n", __LINE__);   \
			fails++;                              \
		}                                       \
	} while (0)

#if defined __MCC__ && defined __riscv && __riscv_xlen == 64
#define RV_ASM_PROBE 1

/* 1. forward local label. Returns 7 when the branch is taken, 100 when it
 *    falls through -- so a branch that does not branch is not a crash, it is a
 *    wrong answer this file can name. */
__asm__(".text\n"
				".globl rv_fwd_local\n"
				".type rv_fwd_local, @function\n"
				"rv_fwd_local:\n"
				"  bnez a0, 1f\n"
				"  addi a0, x0, 100\n"
				"  jalr x0, 0(ra)\n"
				"1:\n"
				"  addi a0, x0, 7\n"
				"  jalr x0, 0(ra)\n");

/* 2. forward NAMED label, same contract. */
__asm__(".text\n"
				".globl rv_fwd_named\n"
				".type rv_fwd_named, @function\n"
				"rv_fwd_named:\n"
				"  bnez a0, .Lrv_named_target\n"
				"  addi a0, x0, 100\n"
				"  jalr x0, 0(ra)\n"
				".Lrv_named_target:\n"
				"  addi a0, x0, 7\n"
				"  jalr x0, 0(ra)\n");

/* 3. backward local label: count down a0, counting up a1, return the trip
 *    count. A branch that falls through returns 1; one that loops forever
 *    hangs; the symptom actually seen was a wildly wrong count, which this
 *    returns verbatim. */
__asm__(".text\n"
				".globl rv_back_local\n"
				".type rv_back_local, @function\n"
				"rv_back_local:\n"
				"  addi a1, x0, 0\n"
				"1:\n"
				"  addi a1, a1, 1\n"
				"  addi a0, a0, -1\n"
				"  bnez a0, 1b\n"
				"  addi a0, a1, 0\n"
				"  jalr x0, 0(ra)\n");

/* 4. the inverse condition, so a fix that hard-codes one polarity is caught:
 *    beqz taken when a0 == 0. */
__asm__(".text\n"
				".globl rv_fwd_beqz\n"
				".type rv_fwd_beqz, @function\n"
				"rv_fwd_beqz:\n"
				"  beqz a0, 1f\n"
				"  addi a0, x0, 100\n"
				"  jalr x0, 0(ra)\n"
				"1:\n"
				"  addi a0, x0, 7\n"
				"  jalr x0, 0(ra)\n");

/* 5. a two-register branch, since bnez/beqz are beq/bne against x0 and could
 *    plausibly be special-cased. */
__asm__(".text\n"
				".globl rv_beq_regs\n"
				".type rv_beq_regs, @function\n"
				"rv_beq_regs:\n"
				"  beq a0, a1, 1f\n"
				"  addi a0, x0, 100\n"
				"  jalr x0, 0(ra)\n"
				"1:\n"
				"  addi a0, x0, 7\n"
				"  jalr x0, 0(ra)\n");

extern int rv_fwd_local(int);
extern int rv_fwd_named(int);
extern int rv_back_local(int);
extern int rv_fwd_beqz(int);
extern int rv_beq_regs(int, int);
#endif

int main(void) {
#ifdef RV_ASM_PROBE
	/* Taken and not-taken, both directions, so neither a branch that never
	 * fires nor one that always fires can pass. */
	CK(rv_fwd_local(1) == 7);
	CK(rv_fwd_local(0) == 100);
	CK(rv_fwd_named(1) == 7);
	CK(rv_fwd_named(0) == 100);
	CK(rv_fwd_beqz(0) == 7);
	CK(rv_fwd_beqz(1) == 100);
	CK(rv_beq_regs(5, 5) == 7);
	CK(rv_beq_regs(5, 6) == 100);
	/* The loop: 3 in, 3 iterations out. The filed symptom was 256. */
	CK(rv_back_local(3) == 3);
	CK(rv_back_local(1) == 1);
	CK(rv_back_local(10) == 10);
#endif
	printf(fails ? "FAIL\n" : "OK\n");
	return fails ? 1 : 0;
}

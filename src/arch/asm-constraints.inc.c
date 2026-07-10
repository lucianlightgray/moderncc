static void asm_constraints_prologue(ASMOperand *operands, int nb_operands,
																		 int nb_outputs,
																		 const uint8_t *clobber_regs,
																		 int *sorted_op, uint8_t *regs_allocated) {
	ASMOperand *op;
	const char *str;
	int j, k, p1, p2, tmp, reg;

	for (int i = 0; i < nb_operands; i++) {
		op = &operands[i];
		op->input_index = -1;
		op->ref_index = -1;
		op->reg = -1;
		op->is_memory = 0;
		op->is_rw = 0;
		op->is_llong = 0;
	}
	for (int i = 0; i < nb_operands; i++) {
		op = &operands[i];
		str = op->constraint;
		str = skip_constraint_modifiers(str);
		if (isnum(*str) || *str == '[') {
			k = find_constraint(operands, nb_operands, str, NULL);
			if ((unsigned)k >= i || i < nb_outputs)
				mcc_error("invalid reference in constraint %d ('%s')", i, str);
			op->ref_index = k;
			if (operands[k].input_index >= 0)
				mcc_error("cannot reference twice the same operand");
			operands[k].input_index = i;
			op->priority = 5;
		} else if ((op->vt->r & VT_VALMASK) == VT_LOCAL && op->vt->sym && (reg = op->vt->sym->r & VT_VALMASK) < VT_CONST) {
			op->priority = 1;
			op->reg = reg;
		} else {
			op->priority = constraint_priority(str);
		}
	}

	for (int i = 0; i < nb_operands; i++)
		sorted_op[i] = i;
	for (int i = 0; i < nb_operands - 1; i++) {
		for (j = i + 1; j < nb_operands; j++) {
			p1 = operands[sorted_op[i]].priority;
			p2 = operands[sorted_op[j]].priority;
			if (p2 < p1) {
				tmp = sorted_op[i];
				sorted_op[i] = sorted_op[j];
				sorted_op[j] = tmp;
			}
		}
	}

	for (int i = 0; i < MCC_NB_ASM_REGS; i++) {
		if (clobber_regs[i])
			regs_allocated[i] = REG_IN_MASK | REG_OUT_MASK;
		else
			regs_allocated[i] = 0;
	}
}

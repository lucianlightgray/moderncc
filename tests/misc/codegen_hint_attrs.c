/* T-mac-30222: well-known codegen-hint attributes (hot/cold/flatten/no_reorder/
 * no_stack_protector/no_icf) were routed to parse_one_attribute's default arm,
 * which warns "attribute ignored" -> a file using them fails under -Werror,
 * though gcc/clang accept them silently (they only influence optimization and
 * mcc ignores them, which is a valid choice). This file must compile clean even
 * under -Wall -Werror; the __x__ spellings must be accepted too. */
int hot_fn(void) __attribute__((hot));
int cold_fn(void) __attribute__((cold));
int flat_fn(void) __attribute__((flatten));
int nore_fn(void) __attribute__((no_reorder));
int nosp_fn(void) __attribute__((__no_stack_protector__));
int noicf_fn(void) __attribute__((__no_icf__));
int hot_fn(void) { return 1; }
int cold_fn(void) { return 2; }
int flat_fn(void) { return 3; }
int nore_fn(void) { return 4; }
int nosp_fn(void) { return 5; }
int noicf_fn(void) { return 6; }
int main(void) {
	return (hot_fn() + cold_fn() + flat_fn() + nore_fn() + nosp_fn() + noicf_fn()) == 21 ? 0 : 1;
}

#include "std_env.h"
#include "s6_7_7.h"

int main(void) {
	s6_7_7_init_both_ends();
	s6_7_7_init_inconsistent();
	s6_7_7_init_mixed();
	s6_7_7_init_elision();
	s6_7_7_init_union_next();
	s6_7_7_init_enum_desig();
	s6_7_7_init_sparse();
	s6_7_7_init_string();
	s6_7_7_typedef_vla();
	s6_7_7_typedef_incomplete();
	s6_7_7_init_flat();
	return 0;
}

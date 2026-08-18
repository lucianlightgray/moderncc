/* T-mac-30188: a struct/union/enum tag first declared inside a function
 * prototype's parameter list has prototype scope and will not be visible
 * outside — gcc/clang warn ("declared inside parameter list will not be
 * visible..."); mcc was silent. Warns under -Wall. */
void f(struct Local *p);

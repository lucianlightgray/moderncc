/* dg-error: assignment of read-only location */
/* C11 6.9.1p9: a parameter is an lvalue of its DECLARED type inside the body,
 * so a const-qualified one is not modifiable. mcc used to accept this silently:
 * 6.7.6.3p15 ignores top-level parameter qualifiers when deciding function-type
 * compatibility, and convert_parameter_type() implemented that by stripping the
 * qualifier outright, so it was gone before the body was ever parsed. The
 * const-local form next door was diagnosed the whole time. */
int f(const int x)
{
	x = 5;
	return x;
}

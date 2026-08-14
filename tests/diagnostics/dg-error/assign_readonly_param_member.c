/* dg-error: assignment of read-only location */
/* Same defect through a member: a const struct parameter's members are not
 * modifiable either. */
struct s {
	int a;
};
int f(const struct s v)
{
	v.a = 5;
	return v.a;
}

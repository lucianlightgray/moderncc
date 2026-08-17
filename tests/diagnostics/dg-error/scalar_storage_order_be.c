/* dg-error: scalar_storage_order */
struct __attribute__((scalar_storage_order("big-endian"))) be {
	long double v;
};

int f(void)
{
	struct be s;
	s.v = 1;
	return (int)s.v;
}

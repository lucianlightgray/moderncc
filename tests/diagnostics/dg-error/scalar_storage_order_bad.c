/* dg-error: argument must be */
struct __attribute__((scalar_storage_order("middle-endian"))) x {
	int v;
};

int f(void)
{
	struct x s;
	s.v = 1;
	return s.v;
}

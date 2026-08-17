/* dg-error: reverse storage order */
struct __attribute__((scalar_storage_order("big-endian"))) be {
	int flat;
	int arr[3];
};

int *f(struct be *s)
{
	return &s->flat;
}

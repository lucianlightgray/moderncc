_Static_assert(__SIZEOF_LONG_DOUBLE__ == sizeof(long double),
	"__SIZEOF_LONG_DOUBLE__ must equal sizeof(long double)");

int main(void) {
	return (int)(__SIZEOF_LONG_DOUBLE__ == sizeof(long double)) - 1;
}

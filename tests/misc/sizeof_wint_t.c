_Static_assert(__SIZEOF_WINT_T__ == sizeof(__WINT_TYPE__),
	"__SIZEOF_WINT_T__ must equal sizeof(__WINT_TYPE__)");

int main(void) {
	return (int)(__SIZEOF_WINT_T__ == sizeof(__WINT_TYPE__)) - 1;
}

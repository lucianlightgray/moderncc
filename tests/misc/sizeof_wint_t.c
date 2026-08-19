_Static_assert(__SIZEOF_WINT_T__ == sizeof(__WINT_TYPE__),
	"__SIZEOF_WINT_T__ must equal sizeof(__WINT_TYPE__)");

#if !defined(_WIN32) && !defined(__linux__)
_Static_assert(__WINT_MAX__ == 0x7fffffff,
	"signed int wint_t (Apple/BSD): __WINT_MAX__ must be INT_MAX");
_Static_assert(__WINT_MIN__ == -0x7fffffff - 1,
	"signed int wint_t (Apple/BSD): __WINT_MIN__ must be INT_MIN");
#else
_Static_assert(__WINT_MIN__ == 0,
	"unsigned wint_t (Windows/Linux): __WINT_MIN__ must be 0");
#endif

int main(void) {
	return (int)(__SIZEOF_WINT_T__ == sizeof(__WINT_TYPE__)) - 1;
}

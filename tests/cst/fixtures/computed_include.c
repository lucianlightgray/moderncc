#define STDDEF_INC <stddef.h>
#define __cst_str(x) #x
#define cst_str(x) __cst_str(x)
#define boolhdr stdbool.h

#include STDDEF_INC

#include cst_str(boolhdr)

static size_t before = sizeof(size_t);

#include <stdarg.h>

int computed(int v) {
	bool flag = v > 0;
	return flag ? (int)before : v;
}

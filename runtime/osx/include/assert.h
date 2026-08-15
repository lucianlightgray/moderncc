#undef assert

#ifdef NDEBUG
#define assert(e) ((void)0)
#else
#define assert(e) \
	((void)((e) ? ((void)0) : __assert_rtn(__func__, __FILE__, __LINE__, #e)))
#endif

#ifndef _MCC_OSX_ASSERT_H
#define _MCC_OSX_ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

void __assert_rtn(const char *, const char *, int, const char *);

#ifdef __cplusplus
}
#endif

#endif

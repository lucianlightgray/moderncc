/* T-mac-30233: __has_builtin must report 1 for every builtin the compiler
 * actually accepts, including the name-recognized (non-tokenized) families:
 * the mem/str/alloca/stdio symbols declared in mccdefs.h and the libm aliases
 * synthesized by builtin_libm_alias. Pre-fix pp_has_builtin_arg only saw
 * tokenized builtins, the untokenized[] list, and macros, so it reported 0 for
 * memcpy/strlen/alloca/offsetof/abs/sin/... though all compile+run cleanly ->
 * portable code guarding a fast path behind #if __has_builtin(...) silently
 * took the fallback. A genuinely unknown builtin must still report 0. This
 * file compiles clean only when every case is correct. */

/* mccdefs.h-declared symbol family */
#if !__has_builtin(__builtin_memcpy)
#error memcpy
#endif
#if !__has_builtin(__builtin_memmove)
#error memmove
#endif
#if !__has_builtin(__builtin_memset)
#error memset
#endif
#if !__has_builtin(__builtin_memcmp)
#error memcmp
#endif
#if !__has_builtin(__builtin_strlen)
#error strlen
#endif
#if !__has_builtin(__builtin_strcpy)
#error strcpy
#endif
#if !__has_builtin(__builtin_alloca)
#error alloca
#endif
#if !__has_builtin(__builtin_offsetof)
#error offsetof
#endif
#if !__has_builtin(__builtin_abs)
#error abs
#endif
#if !__has_builtin(__builtin_mempcpy)
#error mempcpy
#endif

/* libm alias family (base + f/l suffix synthesized on use) */
#if !__has_builtin(__builtin_sin)
#error sin
#endif
#if !__has_builtin(__builtin_sinf)
#error sinf
#endif
#if !__has_builtin(__builtin_sqrtl)
#error sqrtl
#endif

/* tokenized builtins still work (regression guard) */
#if !__has_builtin(__builtin_expect)
#error expect
#endif
#if !__has_builtin(__builtin_popcount)
#error popcount
#endif

/* a genuinely unknown builtin must report 0 */
#if __has_builtin(__builtin_frobnicate_totally_fake)
#error fake_present
#endif

int main(void) { return 0; }

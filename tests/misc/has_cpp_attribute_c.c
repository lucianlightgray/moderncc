/* T-mac-30237: __has_cpp_attribute must work in C mode (gcc/clang support it as
 * an extension) — it was unregistered as a pp-builtin in mcc, so every query
 * returned 0. Compiles clean only when the standard attributes report nonzero
 * and an unknown attribute reports 0. */
#if !__has_cpp_attribute(nodiscard)
#error nodiscard
#endif
#if !__has_cpp_attribute(deprecated)
#error deprecated
#endif
#if !__has_cpp_attribute(maybe_unused)
#error maybe_unused
#endif
#if !__has_cpp_attribute(fallthrough)
#error fallthrough
#endif
#if __has_cpp_attribute(totally_fake_attribute_xyz)
#error fake_present
#endif
#ifndef __has_cpp_attribute
#error macro_not_defined
#endif
int main(void) { return 0; }

/* dg-error: is_target arch/os builtins evaluate */
/* T-mac-30022 slice-2.  `__is_target_arch`/`__is_target_os` (and the confident
 * `__is_target_vendor(apple)` / linux `__is_target_environment(gnu)`) used to hit
 * the pp_builtin_func fallback and evaluate to 0 unconditionally, so
 * `#if __is_target_arch(x86_64)` was always false and target-conditional code
 * never compiled.  They are now answered against mcc's own target identity.
 *
 * Portable red->green check: exactly one of the five arches must match and at
 * least one os must match on every target.  When the builtins work this #if is
 * true and the #error below is the *expected* diagnostic the dg-error harness
 * looks for; pre-fix (all builtins 0) the #if is false, the file compiles
 * cleanly, and the harness fails with "expected compile to FAIL". */
#if (__is_target_arch(x86_64) + __is_target_arch(i386) + __is_target_arch(aarch64) \
   + __is_target_arch(arm) + __is_target_arch(riscv64) == 1) \
  && (__is_target_os(linux) + __is_target_os(darwin) + __is_target_os(windows) >= 1)
#error "is_target arch/os builtins evaluate"
#endif
int x;

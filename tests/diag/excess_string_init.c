/* T-mac-30092: a too-long string initializer must be gated on
 * -Wexcess-initializers, like the element-list sibling — not a plain always-on
 * warning. Default: warns (compiles). -Wno-excess-initializers: silent.
 * -Werror=excess-initializers: error. */
char a[3] = "hello";

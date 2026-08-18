/* T-mac-30129: typeof of a bit-field is a constraint violation (clang/gcc reject);
 * mcc must not create a bit-field-typed object. */
struct S { int bf : 5; };
typeof(((struct S *)0)->bf) x;

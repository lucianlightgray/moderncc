/* T-mac-30185 slice: a struct/union/enum tag carrying __attribute__((deprecated))
 * must warn when the tag is used; mcc honored deprecated only on ordinary
 * identifiers (functions/objects/typedefs), not on tag uses. -Wall-independent
 * (warn_deprecated_declarations is on by default). */
struct __attribute__((deprecated)) S { int x; };
enum __attribute__((deprecated)) E { A };
int use(void) {
	struct S s;
	enum E e = A;
	s.x = 1;
	return s.x + e;
}

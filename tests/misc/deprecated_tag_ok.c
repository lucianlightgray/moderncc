/* T-mac-30185 known-positive: non-deprecated tags stay clean under -Werror. */
struct S { int x; };
enum E { A };
int main(void) { struct S s; enum E e = A; s.x = 1; return (s.x - 1) + e; }

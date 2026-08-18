/* T-mac-30188 known-positive: a tag pre-declared (visible) before the prototype,
 * an anonymous struct param, and ordinary struct definitions do NOT warn, even
 * under -Wall -Werror. */
struct Pre;
void g(struct Pre *p);
void h(struct { int x; } *p);
struct S { int a; };
int main(void) { struct S s; s.a = 7; return s.a - 7; }

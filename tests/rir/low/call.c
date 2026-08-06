/* An AST_Invoke inside a region ends it: slices are the code BETWEEN
   anonymous invokes, so a call is a boundary and never interior. */
int h(int (*fp)(int), int a) { return fp(a); }

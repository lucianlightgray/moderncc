/* T-mac-30151: initializing a flexible array member of an AUTOMATIC-storage
 * object is a hard error (both gcc and clang reject; mcc previously accepted
 * it silently and DROPPED the data). Must fail to compile. */
struct S { int n; char a[]; };
int main(void){ struct S s = {1, {2, 3, 4}}; return s.n; }

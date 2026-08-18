/* Known-positive for T-mac-30151: a static/file-scope FAM initializer is a GNU
 * extension both oracles accept — the fix must NOT break it. Exit 0 iff correct. */
struct S { int n; char a[]; };
struct S g = {1, {5, 6, 7}};              /* file scope: static storage */
int main(void){
    static struct S s = {2, {3, 4, 5}};   /* block scope but static storage */
    if (g.n != 1 || g.a[0] != 5 || g.a[2] != 7) return 1;
    if (s.n != 2 || s.a[0] != 3 || s.a[2] != 5) return 2;
    return 0;
}

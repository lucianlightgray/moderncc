/* T-mac-30143: stdcall is meaningless off i386; mcc must warn-and-ignore
 * (gcc/clang both warn) rather than silently accept. */
__attribute__((stdcall)) int f(int x);
int f(int x) { return x + 1; }
int main(void) { return f(41) - 42; }

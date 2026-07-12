/* dg-error: redefinition of 'f' */
int f(void) { return 0; }
int f(void) { return 1; }
int main(void)
{
	return f();
}

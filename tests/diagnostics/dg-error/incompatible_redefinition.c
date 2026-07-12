/* dg-error: incompatible types for redefinition of 'foo' */
int foo;
float foo;
int main(void)
{
	return 0;
}

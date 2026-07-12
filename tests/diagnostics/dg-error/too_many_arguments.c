/* dg-error: too many arguments to function */
int f(int a) { return a; }
int main(void)
{
	return f(1, 2, 3);
}

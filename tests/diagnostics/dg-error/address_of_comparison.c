/* dg-error: lvalue expected */
int f(int a)
{
	return (int)(long)&(1 < a);
}

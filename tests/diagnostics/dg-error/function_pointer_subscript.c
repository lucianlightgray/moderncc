/* dg-error: subscripted value is pointer to function */
void (*f)(void);
void (*a[3])(void);

void g(void)
{
	a[0] = f;
	a[1]();
	(*f)();
	f[0];
}

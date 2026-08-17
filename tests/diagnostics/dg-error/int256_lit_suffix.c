/* dg-error: incorrect integer suffix */
/* T-lin-10013.  i256 composes with u/U on either side but not with l/L (there
 * is no 'long __int256') and not with itself. */
int f(void)
{
	return (int)1li256;
}

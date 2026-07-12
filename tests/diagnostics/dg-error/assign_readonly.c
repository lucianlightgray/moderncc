/* dg-error: assignment of read-only location */
int main(void)
{
	const int c = 5;
	c = 6;
	return c;
}

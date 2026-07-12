/* dg-error: dereferencing incomplete type */
int main(void)
{
	struct nope_t *p = 0;
	return p->x;
}

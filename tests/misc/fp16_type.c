_Static_assert(sizeof(__fp16) == 2, "__fp16 is a 2-byte storage type");

int main(void)
{
	__fp16 a = 1.5f;
	__fp16 *p = &a;
	*p = *p + 1.0f;
	if ((int)a != 2)
		return 1;

	__fp16 big = 65504.0f;
	big = big + big;
	if (!__builtin_isinf((float)big))
		return 2;

	return 0;
}

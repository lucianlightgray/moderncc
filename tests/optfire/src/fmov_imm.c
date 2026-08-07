double sink;

void put(double v) { sink += v; }

int main(void)
{
	put(0.5);
	put(1.0);
	put(2.0);
	put(-1.5);
	put(12.0);
	put(0.125);
	put(31.0);
	return sink != 0.0;
}

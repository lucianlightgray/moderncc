int main(void) {
	int i = 0, s = 0;
	if (i < 0)
		goto neg;
loop:
	if (i >= 9)
		goto done;
	s += i;
	i++;
	goto loop;
neg:
	return -1;
done:
	return s + 6;
}

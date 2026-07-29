struct ops {
	int (*fn)(int, int);
};

extern struct ops *pick(void);
extern struct ops tab[4];

int via_arrow(int a, int b)
{
	return pick()->fn(a, b);
}

int via_index(int i, int a, int b)
{
	return tab[i].fn(a, b);
}

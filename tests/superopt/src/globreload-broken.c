int g_flag;
int g_data;
int g_ready;

extern void opaque(void);

int spin_wait(void) {
	int t = g_flag;
	int n = 0;
	while (!t)
		n++;
	return n;
}

int spin(int k) {
	int t = g_flag, n = 0, i;
	for (i = 0; i < k; i++)
		n += t + i;
	return n;
}

int spin_cached(int k) {
	int n = 0, i;
	for (i = 0; i < k; i++)
		n += g_flag + i;
	return n;
}

int across_call(void) {
	int a = g_data;
	opaque();
	return a;
}

int once_call(void) {
	int a = g_data;
	opaque();
	return a + g_data;
}

int publish(void) {
	g_ready = 1;
	g_data = 42;
	return 0;
}

int publish_rev(void) {
	g_data = 42;
	g_ready = 1;
	return 0;
}

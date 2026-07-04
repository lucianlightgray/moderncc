void stack_user(char *p) {
	char buf[64];
	buf[0] = p[0];
	p[1] = buf[0];
}

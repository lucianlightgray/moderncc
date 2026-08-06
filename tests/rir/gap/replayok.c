/* Arena built and taken, but ast_replay_ok() refuses it at ast_func_end. */
void f(void *p, unsigned long n) {
	(void)p;
	(void)n;
}

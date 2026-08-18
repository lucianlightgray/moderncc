/* Companion: writable data must stay writable under -run (the RO-protection
 * fix must not freeze .data/.bss). Exit 0 iff the writes take. */
int g[3] = {1, 2, 3};
int b[3];
int main(void){ g[1] = 20; b[1] = 5; return (g[1]==20 && b[1]==5) ? 0 : 1; }

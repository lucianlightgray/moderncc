/* cf takes a long long* (pointee 8 bytes on every ABI) while x is int (4);
 * the size mismatch is guaranteed on both LP64 and LLP64 (unlike long*, which
 * equals int on LLP64/Windows and would spuriously pass). */
void cf_ll(long long *p) { (void)p; }
int main(void) {
	int x __attribute__((cleanup(cf_ll))) = 0;
	(void)x;
	return 0;
}

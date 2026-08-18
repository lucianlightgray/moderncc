/* T-mac-30187 known-positive: a plain extern declaration (no initializer), an
 * ordinary definition, and a static definition must all stay clean under -Werror. */
extern int y;
int y = 3;
int z = 7;
static int s = 9;
int main(void) { return (y == 3 && z == 7 && s == 9) ? 0 : 1; }

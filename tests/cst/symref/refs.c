/* refs.c — controlled def/use fixture for CST symbol-ref resolution (slice I).
 * The driver (symref.cmake) asserts each use resolves to the correct def. */

int myglobal = 5;

static int helper(int p) {
    return p + myglobal;
}

int reader(void) {
    int local = helper(myglobal);
    return local;
}

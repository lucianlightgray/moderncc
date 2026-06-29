/* GCC/Clang-compatible value-based __atomic_*_n builtins, available without
   including <stdatomic.h> (defined in mccdefs.h over the bare address-taking
   __atomic_* generic forms). */
extern int printf(const char *, ...);

int main(void)
{
    int x = 5;
    long lx = 100;

    __atomic_store_n(&x, 10, __ATOMIC_SEQ_CST);          /* x=10 */
    int l = __atomic_load_n(&x, __ATOMIC_SEQ_CST);       /* l=10 */
    int e = __atomic_exchange_n(&x, 20, __ATOMIC_SEQ_CST);/* e=10, x=20 */

    int exp = 20, des = 30;
    int ok = __atomic_compare_exchange_n(&x, &exp, des, 0,
                 __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);    /* ok=1, x=30 */
    int bad = __atomic_compare_exchange_n(&x, &exp, 99, 0,
                 __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);    /* exp!=x(30): bad=0, exp=30 */

    /* exercise on a wider type too */
    __atomic_store_n(&lx, 7L, __ATOMIC_RELAXED);
    long ll = __atomic_load_n(&lx, __ATOMIC_RELAXED);    /* ll=7 */

    int pass = l == 10 && e == 10 && x == 30 && ok == 1 && exp == 30
            && bad == 0 && ll == 7;
    printf(pass ? "OK\n" : "FAIL\n");
    return pass ? 0 : 1;
}

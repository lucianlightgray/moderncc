/* GCC-compatible legacy __sync_* atomic builtins (mapped to __atomic_*). */
extern int printf(const char *, ...);

int main(void)
{
    int x = 10;
    int a = __sync_fetch_and_add(&x, 5);   /* a=10, x=15 */
    int b = __sync_add_and_fetch(&x, 5);   /* b=20, x=20 */
    int c = __sync_fetch_and_or(&x, 1);    /* c=20, x=21 */
    int ok1 = __sync_bool_compare_and_swap(&x, 21, 100);  /* 1, x=100 */
    int v = __sync_val_compare_and_swap(&x, 100, 7);      /* v=100, x=7 */
    int lock = 0;
    int prev = __sync_lock_test_and_set(&lock, 1);        /* prev=0, lock=1 */
    int held = lock;
    __sync_lock_release(&lock);                            /* lock=0 */
    __sync_synchronize();                                  /* full barrier */
    int y = ++x;                                           /* y=8, x=8 */
    __sync_synchronize();
    int ok = a==10 && b==20 && c==20 && ok1==1 && v==100 && x==8
          && prev==0 && held==1 && lock==0 && y==8;
    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}

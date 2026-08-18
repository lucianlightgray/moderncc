/* T-mac-30182: __attribute__((unused)) / [[maybe_unused]] must suppress
 * -Wunused for the annotated entity; un-annotated ones must still warn. */
static void ann_fn(void) __attribute__((unused));
static void ann_fn(void) {}
static void plain_fn(void) {}                 /* must still warn */
int main(void){
    int ann_var __attribute__((unused));      /* suppressed */
    [[maybe_unused]] int mu_var;              /* suppressed */
    int plain_var;                            /* must still warn */
    return 0;
}

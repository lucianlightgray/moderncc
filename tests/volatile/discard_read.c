/* T-mac-30122(1): a discarded volatile lvalue read must still emit the load
 * (read-to-clear MMIO). Pre-fix mcc dropped these entirely — no reference to
 * the volatile object appeared in the function body. */
volatile int x;
void f_stmt(void)      { x; }        /* bare expression-statement discard */
void f_void(void)      { (void)x; }  /* cast-to-void discard */
void f_comma(void)     { x, 1; }     /* comma left-operand discard */
void f_ptr(volatile int *p) { *p; }  /* dereference discard */

int y;
void nonvol(void)      { y; }        /* non-volatile discard: must NOT load */

/* T-mac-30196: under -run, const/rodata must be read-only (writing it faults),
 * matching AOT — pre-fix -run left it writable (silent mutation, exit 0). */
const int cg = 42;
int main(void){ *(int*)&cg = 99; return cg; }

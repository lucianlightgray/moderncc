/* Known-positive for T-mac-30146: valid (non-surrogate, in-range) UCNs in wide
 * strings must still be accepted with correct code-unit values. */
int main(void){
    unsigned  u32[] = U"€\U0001F600";   /* euro, grinning face */
    unsigned short u16[] = u"€";
    if (u32[0] != 0x20ACu || u32[1] != 0x1F600u) return 1;
    if (u16[0] != 0x20ACu) return 2;
    return 0;
}

/* Known-positive for T-mac-30147: correctly-signed wide-string inits still
 * work — wchar_t/int[] from L"", char32_t/unsigned[] from U", char16_t/
 * unsigned short[] from u". Returns 0 iff all element values are right. */
int            wl[] = L"AB";   /* wchar_t = signed int here */
unsigned       u3[] = U"AB";   /* char32_t = unsigned int  */
unsigned short u1[] = u"AB";   /* char16_t = unsigned short*/
int main(void){
    if (wl[0] != 'A' || wl[1] != 'B' || wl[2] != 0) return 1;
    if (u3[0] != 'A' || u3[1] != 'B' || u3[2] != 0) return 2;
    if (u1[0] != 'A' || u1[1] != 'B' || u1[2] != 0) return 3;
    return 0;
}

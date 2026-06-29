/* 6.4.5: U"..." char32_t string literals (array init, pointer, concatenation). */
typedef __CHAR32_TYPE__ char32_t;
extern int printf(const char *, ...);

char32_t g[] = U"Hi";                   /* static/global init */

int main(void)
{
    char32_t s[] = U"ABC";              /* local array init: A,B,C,0 */
    const char32_t *p = U"xy";          /* pointer to literal */
    char32_t cat[] = U"a" U"b";         /* concatenation */

    int ok = sizeof(s) == 4 * sizeof(char32_t)
          && s[0] == 65 && s[2] == 67 && s[3] == 0
          && p[0] == (char32_t)'x' && p[1] == (char32_t)'y' && p[2] == 0
          && g[0] == (char32_t)'H' && g[1] == (char32_t)'i' && g[2] == 0
          && cat[0] == (char32_t)'a' && cat[1] == (char32_t)'b' && cat[2] == 0;
    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}

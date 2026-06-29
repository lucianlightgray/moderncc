/* Self-checking lexical conformance: universal-character-name identifiers
   (6.4.2.1/6.4.3, declared via \u escape and used via literal UTF-8 -- same
   object), digraphs (6.4.6), and the u8 string prefix (6.4.5). Returns 0 on
   success; runs identically on every arch/platform. (u"/U" wide-string sizeof
   differs on PE and is covered by the exec u16_string/U32_string tests.) */
static int \u00e9 = 40;
static int caf\u00e9 = 2;
int main(void)
{
    int ok = 1;
    é += 1;            /* literal UTF-8 names the \u00e9 object */
    café += 3;
    if (é != 41) ok = 0;       /* 40 + 1 */
    if (café != 5) ok = 0;     /* 2 + 3 */

    /* digraphs: <: :> -> [ ] ; <% %> -> { } ; %: -> # */
    int arr<:3:> = <% 10, 20, 30 %>;
    if (arr<:0:> + arr<:2:> != 40) ok = 0;

    /* u8 string prefix (portable across ELF/PE/Mach-O) */
    if (sizeof(u8"x") != 2) ok = 0;

    return ok ? 0 : 1;
}

/* C9911 §6.4 Lexical elements (s6_4) — main-free test unit.
   No #includes: the includer provides the environment (full_language.c
   via mcclib.h; parts/run_s6_4.c via <std_env.h>). Compiled 3-way
   (gcc/clang/mcc) as a unit by the parts-suite, and aggregated into
   full_language.c. */
#define s6_4_STR(x) #x
#define s6_4_XSTR(x) s6_4_STR(x)
#define s6_4_CAT(a,b) a %:%: b

static const char *s6_4_where(void)
{
    /* §6.4.2.2p1/p2: __func__ names the lexically-enclosing function */
    return __func__;
}

void s6_4_lexical_test(void)
{
    /* §6.4p3/p4: maximal munch — a+++b parses as (a++) + b */
    {
        int a = 1, b = 2;
        int r = a+++b;
        printf("munch a+++b=%d a=%d\n", r, a);   /* 3, 2 */
    }

    /* §6.4.4.1p3: value bases (decimal/octal/hex), first digit most significant */
    printf("bases dec=%d oct=%d hex=%d\n", 255, 0377, 0xFF);   /* 255 255 255 */
    printf("oct0777=%d hexcap=%d\n", 0777, 0Xabc);             /* 511 2748 */

    /* §6.4.4.1p5: type/size selection by form and suffix */
    printf("sz 1U=%d 1L=%d 1LL=%d\n",
           (int)sizeof(1U), (int)sizeof(1L), (int)sizeof(1LL));
    printf("sz maxhex=%d maxull=%d\n",
           (int)sizeof(0xFFFFFFFFFFFFFFFF), (int)sizeof(18446744073709551615ULL));

    /* §6.4.4.2p1/p3: hexadecimal floating constants, power-of-2 scaling */
    printf("hexf %d %d %d %d\n",
           0x1.8p1 == 3.0, 0x1p-1 == 0.5, 0x1.0p4 == 16.0, 0x1p+4 == 16.0);
    printf("hexf2 %d %d\n", 0x0.8p1 == 1.0, 0x10p-4 == 1.0);

    /* §6.4.4.2p4: floating suffix determines type (size); avoid printing
       the padding-dependent long double size, assert it equals long double */
    printf("fltsz f=%d d=%d ldeq=%d\n",
           (int)sizeof(1.0f), (int)sizeof(1.0),
           (int)(sizeof(1.0L) == sizeof(long double)));

    /* §6.4.4.4p3: simple escape sequences map to their control codes */
    printf("esc %d %d %d %d %d %d %d\n",
           '\a', '\b', '\f', '\n', '\r', '\t', '\v');   /* 7 8 12 10 13 9 11 */
    printf("esc2 q=%d dq=%d qm=%d bs=%d\n",
           '\'', '\"', '\?', '\\');                     /* 39 34 63 92 */

    /* §6.4.4.4p5/p6: octal and hex escape numeric values */
    printf("num oct=%d hex=%d\n", '\101', '\x41');      /* 65 65 */

    /* §6.4.4.4p10: integer character constant has type int */
    printf("charsz=%d val=%d\n", (int)sizeof('A'), 'A');   /* 4 65 */

    /* §6.4.4.3p2: enumeration constant has type int */
    {
        enum s6_4_e { s6_4_RED, s6_4_GREEN = 10, s6_4_BLUE };
        printf("enum %d %d %d sz=%d\n",
               s6_4_RED, s6_4_GREEN, s6_4_BLUE, (int)sizeof(s6_4_GREEN));
    }

    /* §6.4.5p5: adjacent string literal concatenation */
    printf("concat=[%s] len=%d\n", "ab" "cd" "ef", (int)strlen("ab" "cd" "ef"));

    /* §6.4.5p8: "\x12" "3" is two chars (0x12,'3'), not one hex escape */
    {
        char s[] = "\x12" "3";
        printf("x12str len=%d b0=%d b1=%d\n",
               (int)(sizeof(s) - 1), s[0], s[1]);        /* 2 18 51 */
    }

    /* §6.4.2.1p2: uppercase and lowercase letters are distinct identifiers */
    {
        int Ab = 3, aB = 7, AB = 9, ab = 1;
        printf("distinct %d %d %d %d\n", Ab, aB, AB, ab);
    }

    /* §6.4.2.2: __func__ inside a named function */
    printf("func=%s here=%s\n", s6_4_where(), __func__);

    /* §6.4.6p3/p4: digraphs <: :> <% %> and %:%: behave like [ ] { } ## */
    {
        int arr<:3:> = <%10, 20, 30%>;
        printf("digraph %d %d %d\n", arr<:0:>, arr<:1:>, arr<:2:>);
    }
    printf("paste %d\n", s6_4_CAT(12, 34));              /* 1234 */

    /* §6.4.8p1: a pp-number (0x1.2p3) is one token (survives stringization) */
    printf("ppnum=[%s]\n", s6_4_XSTR(0x1.2p3));

    /* §6.4.9p3: "a//b" is a string literal, not a comment */
    printf("slashes=[%s] len=%d\n", "a//b", (int)strlen("a//b"));
    /* §6.4.9p1: this /* is a block comment */ printf("aftercomment\n");
}

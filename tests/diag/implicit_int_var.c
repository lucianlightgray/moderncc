/* T-mac-30093: a variable declarator with no type-specifier (implicit int) must
 * obey -Wimplicit-int and the C99 permerror promotion, like its function/
 * specifier siblings — not a plain always-on warning. In C99+ (default C23) this
 * is a hard error; -Wno-implicit-int silences it; pre-C99 it is a warning. */
x;

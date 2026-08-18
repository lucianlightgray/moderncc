/* Known-positive for T-mac-30133: __extension__ must NOT leak pedantic-off — a
 * plain `long long` after an __extension__ declaration must still be diagnosed. */
__extension__ long long g = 1;
long long h = 2;

/* T-mac-30160: `#pragma GCC poison ident` must make any subsequent use of the
 * identifier an error ("attempt to use a poisoned identifier"); mcc ignored the
 * pragma and compiled banned identifiers silently. This file must be REJECTED
 * (the poisoned identifier `forbidden` is used after the pragma). */
#pragma GCC poison forbidden
int forbidden = 5;

/* 6.10.9: under -E the _Pragma operator is re-emitted as a #pragma directive
   on its own line (destringized), not passed through as _Pragma ( "..." )
   tokens.  Covers a direct _Pragma with an embedded \" escape and the classic
   macro-generated DO_PRAGMA form. */
#define DO_PRAGMA(x) _Pragma(#x)
int before;
_Pragma("message \"hello\"")
DO_PRAGMA(GCC diagnostic push)
int after;

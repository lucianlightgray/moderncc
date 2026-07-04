#define DO_PRAGMA(x) _Pragma(#x)
int before;
_Pragma("message \"hello\"")
    DO_PRAGMA(GCC diagnostic push) int after;

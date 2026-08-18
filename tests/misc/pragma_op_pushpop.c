#define V 2
_Pragma("push_macro(\"V\")")
#undef V
#define V 9
_Pragma("pop_macro(\"V\")")
int a = V;

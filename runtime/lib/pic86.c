#ifdef __leading_underscore
# define _(s) _##s
#else
# define _(s) s
#endif
#define STR_(x) #x
#define STR(x) STR_(x)

#if defined __i386__
__asm__(
"        .text\n"
"        .globl " STR(_(__x86.get_pc_thunk.ax)) "\n"
"        .hidden " STR(_(__x86.get_pc_thunk.ax)) "\n"
STR(_(__x86.get_pc_thunk.ax)) ":\n"
"        mov (%esp),%eax\n"
"        ret\n"
"        .size " STR(_(__x86.get_pc_thunk.ax)) ", .-" STR(_(__x86.get_pc_thunk.ax)) "\n"

"        .globl " STR(_(__x86.get_pc_thunk.bx)) "\n"
"        .hidden " STR(_(__x86.get_pc_thunk.bx)) "\n"
STR(_(__x86.get_pc_thunk.bx)) ":\n"
"        mov (%esp),%ebx\n"
"        ret\n"
"        .size " STR(_(__x86.get_pc_thunk.bx)) ", .-" STR(_(__x86.get_pc_thunk.bx)) "\n"

"        .globl " STR(_(__x86.get_pc_thunk.cx)) "\n"
"        .hidden " STR(_(__x86.get_pc_thunk.cx)) "\n"
STR(_(__x86.get_pc_thunk.cx)) ":\n"
"        mov (%esp),%ecx\n"
"        ret\n"
"        .size " STR(_(__x86.get_pc_thunk.cx)) ", .-" STR(_(__x86.get_pc_thunk.cx)) "\n"

"        .globl " STR(_(__x86.get_pc_thunk.dx)) "\n"
"        .hidden " STR(_(__x86.get_pc_thunk.dx)) "\n"
STR(_(__x86.get_pc_thunk.dx)) ":\n"
"        mov (%esp),%edx\n"
"        ret\n"
"        .size " STR(_(__x86.get_pc_thunk.dx)) ", .-" STR(_(__x86.get_pc_thunk.dx)) "\n"
);
#endif

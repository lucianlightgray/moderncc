/* Auto-ported from win32/lib/chkstk.S to a C translation unit (file-scope __asm__)
   so the project carries no .S files. The assembly is byte-identical; the
   former cpp-time _(sym) symbol decoration is preserved via STR(_(sym)),
   and labels resolve across the separate __asm__ blocks (same TU). */
#define STR_(x) #x
#define STR(x) STR_(x)
/* ---------------------------------------------- */
/* chkstk86.s */

#ifdef __leading_underscore
# define _(s) _##s
#else
# define _(s) s
#endif

/* ---------------------------------------------- */
#if defined(__aarch64__)

__asm__(
".globl __chkstk\n"
"__chkstk:\n"
);
    /* Windows ARM64 stack probing helper.
       arm64-gen.c passes the requested frame size in x15, scaled in 16-byte
       units. Probe one 4 KiB page at a time and leave SP unchanged; the caller
       subtracts SP after the probe returns. */
__asm__(
"    mov     x16, sp\n"
"    lsl     x17, x15, 4\n"
"    cbz     x17, L_chkstk_done\n"
"L_chkstk_loop:\n"
"    subs    x0, x17, 4096\n"
"    bls     L_chkstk_tail\n"
"    sub     x16, x16, 4096\n"
"    ldr     xzr, [x16]\n"
"    sub     x17, x17, 4096\n"
"    b       L_chkstk_loop\n"
"L_chkstk_tail:\n"
"    sub     x16, x16, x17\n"
"    ldr     xzr, [x16]\n"
"L_chkstk_done:\n"
"    ret\n"
);

/* ---------------------------------------------- */
#elif defined(__i386__)

__asm__(
".globl " STR(_(__chkstk)) "\n"
"" STR(_(__chkstk)) ":\n"
"    xchg    (%esp),%ebp     /* store ebp, get ret.addr */\n"
"    push    %ebp            /* push ret.addr */\n"
"    lea     4(%esp),%ebp    /* setup frame ptr */\n"
"    push    %ecx            /* save ecx */\n"
"    mov     %ebp,%ecx\n"
"P0:\n"
"    sub     $4096,%ecx\n"
"    test    %eax,(%ecx)\n"
"    sub     $4096,%eax\n"
"    cmp     $4096,%eax\n"
"    jge     P0\n"
"    sub     %eax,%ecx\n"
"    test    %eax,(%ecx)\n"
);

__asm__(
"    mov     %esp,%eax\n"
"    mov     %ecx,%esp\n"
"    mov     (%eax),%ecx     /* restore ecx */\n"
"    jmp     *4(%eax)\n"
);

/* ---------------------------------------------- */
#else /* __x86_64__ */

__asm__(
".globl " STR(_(__chkstk)) "\n"
"" STR(_(__chkstk)) ":\n"
"    xchg    (%rsp),%rbp     /* store ebp, get ret.addr */\n"
"    push    %rbp            /* push ret.addr */\n"
"    lea     8(%rsp),%rbp    /* setup frame ptr */\n"
"    push    %rcx            /* save ecx */\n"
"    mov     %rbp,%rcx\n"
"    movslq  %eax,%rax\n"
"P0:\n"
"    sub     $4096,%rcx\n"
"    test    %rax,(%rcx)\n"
"    sub     $4096,%rax\n"
"    cmp     $4096,%rax\n"
"    jge     P0\n"
"    sub     %rax,%rcx\n"
"    test    %rax,(%rcx)\n"
);

__asm__(
"    mov     %rsp,%rax\n"
"    mov     %rcx,%rsp\n"
"    mov     (%rax),%rcx     /* restore ecx */\n"
"    jmp     *8(%rax)\n"
);

/* ---------------------------------------------- */
#endif
/* ---------------------------------------------- */

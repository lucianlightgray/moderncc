#define STR_(x) #x
#define STR(x) STR_(x)

#ifdef __leading_underscore
# define _(s) _##s
#else
# define _(s) s
#endif

#if defined __i386__

__asm__(
".globl " STR(_(alloca)) ", " STR(_(__alloca)) "\n"
"" STR(_(alloca)) ":\n"
"" STR(_(__alloca)) ":\n"
"    pop     %edx\n"
"    pop     %eax\n"
"    add     $3,%eax\n"
"    and     $-4,%eax\n"
"    jz      p3\n"
);

#ifdef _WIN32
__asm__(
"p1:\n"
"    cmp     $4096,%eax\n"
"    jb      p2\n"
"    test    %eax,-4096(%esp)\n"
"    sub     $4096,%esp\n"
"    sub     $4096,%eax\n"
"    jmp     p1\n"
"p2:\n"
);
#endif
__asm__(
"    sub     %eax,%esp\n"
"    mov     %esp,%eax\n"
"p3:\n"
"    push    %edx\n"
"    push    %edx\n"
"    ret\n"
);

#elif defined __x86_64__

__asm__(
".globl " STR(_(alloca)) "\n"
"" STR(_(alloca)) ":\n"
"    pop     %rdx\n"
);
#ifdef _WIN32
__asm__(
"    mov     %rcx,%rax\n"
);
#else
__asm__(
"    mov     %rdi,%rax\n"
);
#endif
__asm__(
"    add     $15,%rax\n"
"    and     $-16,%rax\n"
"    jz      p3\n"
);

#ifdef _WIN32
__asm__(
"p1:\n"
"    cmp     $4096,%rax\n"
"    jb      p2\n"
"    test    %rax,-4096(%rsp)\n"
"    sub     $4096,%rsp\n"
"    sub     $4096,%rax\n"
"    jmp p1\n"
"p2:\n"
);
#endif
__asm__(
"    sub     %rax,%rsp\n"
"    mov     %rsp,%rax\n"
"p3:\n"
"    push    %rdx\n"
"    ret\n"
);

#elif defined __arm__

__asm__(
".globl " STR(_(alloca)) "\n"
"" STR(_(alloca)) ":\n"
"    rsb sp, r0, sp\n"
"    bic sp, sp, #7\n"
"    mov r0, sp\n"
"    mov pc, lr\n"
);

#elif defined __aarch64__ || defined __arm64__

__asm__(
".globl " STR(_(alloca)) "\n"
"" STR(_(alloca)) ":\n"
);
#ifdef __TINYC__
__asm__(
"    .int 0x91003c00\n"
"    .int 0x927cec00\n"
);
#ifdef _WIN32
__asm__(
"    .int 0xb4000160\n"
"    .int 0xd2820001\n"
"    .int 0xeb01001f\n"
"    .int 0x540000c3\n"
"    .int 0xcb2163e2\n"
"    .int 0xf940005f\n"
"    .int 0xcb2163ff\n"
"    .int 0xcb010000\n"
"    .int 0x17fffffa\n"
"    .int 0xb4000040\n"
);
#endif
__asm__(
"    .int 0xcb2063ff\n"
"    .int 0x910003e0\n"
"    .int 0xd65f03c0\n"
);
#else
__asm__(
"    add x0, x0, #15     // Round up to 16-byte boundary\n"
"    and x0, x0, #-16    // Ensure 16-byte alignment\n"
);
#ifdef _WIN32
__asm__(
"    cbz x0, p100        // If size is 0, skip to return\n"
"    // Windows requires page-wise allocation with stack probing\n"
"    mov x1, #4096       // Page size = 4096 bytes\n"
);

__asm__(
"p101:\n"
"    cmp x0, x1          // Compare remaining size with page size\n"
"    b.lo    p102        // If less than page, jump to remainder\n"
);

__asm__(
"    // Probe first, then allocate\n"
"    sub x2, sp, x1      // Calculate guard page address (sp - 4096)\n"
"    ldr xzr, [x2]       // Touch guard page FIRST\n"
"    sub sp, sp, x1      // THEN allocate the page\n"
);

__asm__(
"    sub x0, x0, x1      // Decrement remaining size\n"
"    b   p101            // Continue loop\n"
);

__asm__(
"p102:\n"
"    // Allocate remaining bytes (less than one page)\n"
"    cbz x0, p100        // If no remaining bytes, skip\n"
"    sub sp, sp, x0      // Allocate remaining space\n"
);
#else
__asm__(
"    // Non-Windows: simple one-time allocation\n"
"    sub sp, sp, x0      // Allocate space on stack\n"
);
#endif

__asm__(
"p100:\n"
"    mov x0, sp          // Return allocated address\n"
"    ret                 // Return to caller\n"
);
#endif

#elif defined __riscv

__asm__(
".globl " STR(_(alloca)) "\n"
"" STR(_(alloca)) ":\n"
"    sub    sp, sp, a0\n"
"    addi   sp, sp, -15\n"
"    andi   sp, sp, -16\n"
"    add    a0, sp, zero\n"
"    ret\n"
);

#endif

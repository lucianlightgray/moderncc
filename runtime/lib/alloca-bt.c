#define STR_(x) #x
#define STR(x) STR_(x)

#ifdef __leading_underscore
#define _(s) _##s
#else
#define _(s) s
#endif

#if defined __i386__

__asm__(
		".globl " STR(_(__bound_alloca)) "\n"
																		 "" STR(_(__bound_alloca)) ":\n"
																															 "    pop     %edx\n"
																															 "    pop     %eax\n"
																															 "    mov     %eax, %ecx\n"
																															 "    test    %eax,%eax\n"
																															 "    jz      p6\n"
																															 "    add     $3 + 1,%eax\n"
																															 "    and     $-4,%eax\n");

#ifdef _WIN32
__asm__(
		"p4:\n"
		"    cmp     $4096,%eax\n"
		"    jb      p5\n"
		"    test    %eax,-4096(%esp)\n"
		"    sub     $4096,%esp\n"
		"    sub     $4096,%eax\n"
		"    jmp p4\n");

__asm__(
		"p5:\n");
#endif

__asm__(
		"    sub     %eax,%esp\n"
		"    mov     %esp,%eax\n");

__asm__(
		"    push    %edx\n"
		"    push    %eax\n"
		"    push    %ecx\n"
		"    push    %eax\n"
		"    call    " STR(_(__bound_new_region)) "\n"
																							"    add     $8, %esp\n"
																							"    pop     %eax\n"
																							"    pop     %edx\n");

__asm__(
		"p6:\n"
		"    push    %edx\n"
		"    push    %edx\n"
		"    ret\n");

#elif defined __x86_64__

__asm__(
		".globl " STR(_(__bound_alloca)) "\n"
																		 "" STR(_(__bound_alloca)) ":\n");
#ifdef _WIN32
__asm__(
		"    inc %rcx            # add one extra to separate regions\n"
		"    jmp " STR(_(alloca)) "\n"
															".globl " STR(_(__bound_alloca_nr)) "\n"
																																	"" STR(_(__bound_alloca_nr)) ":\n"
																																															 "    dec     %rcx\n"
																																															 "    push    %rax\n"
																																															 "    mov     %rcx,%rdx\n"
																																															 "    mov     %rax,%rcx\n"
																																															 "    sub     $32,%rsp\n"
																																															 "    call    " STR(_(__bound_new_region)) "\n"
																																																																				 "    add     $32,%rsp\n"
																																																																				 "    pop     %rax\n"
																																																																				 "    ret\n");
#else
__asm__(
		"    pop     %rdx\n"
		"    mov     %rdi,%rax\n"
		"    and     %eax,%eax\n"
		"    jz      p3\n"
		"    mov     %rax,%rsi	# size, a second parm to the __bound_new_region\n"
		"    add     $15 + 1,%rax  # add one extra to separate regions\n"
		"    and     $-16,%rax\n");

__asm__(
		"    sub     %rax,%rsp\n"
		"    mov     %rsp,%rdi	# pointer, a first parm to the __bound_new_region\n"
		"    mov     %rsp,%rax\n");

__asm__(
		"    push    %rdx\n"
		"    push    %rax\n"
		"    call    " STR(_(__bound_new_region)) "\n"
																							"    pop     %rax\n"
																							"    pop     %rdx\n");

__asm__(
		"p3:\n"
		"    push    %rdx\n"
		"    ret\n");
#endif

#elif defined __arm__

__asm__(
		".globl " STR(_(__bound_alloca)) "\n"
																		 "" STR(_(__bound_alloca)) ":\n"
																															 "    mov r1, r0\n"
																															 "    add r0, r0, #1\n"
																															 "    rsb sp, r0, sp\n"
																															 "    bic sp, sp, #7\n"
																															 "    mov r0, sp\n"
																															 "    push { lr }\n"
																															 "    bl " STR(_(__bound_new_region)) "\n"
																																																		"    pop { lr }\n"
																																																		"    mov r0, sp\n"
																																																		"    mov pc, lr\n");

#elif defined __aarch64__ || defined __arm64__

__asm__(
		".globl " STR(_(__bound_alloca)) "\n"
																		 "" STR(_(__bound_alloca)) ":\n");
#ifdef __MCC__
__asm__(
		"    .int 0xaa0003e1\n"
		"    .int 0x91004000\n"
		"    .int 0x927cec00\n");
#ifdef _WIN32
__asm__(
		"    .int 0xb4000160\n"
		"    .int 0xd2820002\n"
		"    .int 0xeb02001f\n"
		"    .int 0x540000c3\n"
		"    .int 0xcb2263e3\n"
		"    .int 0xf940007f\n"
		"    .int 0xcb2263ff\n"
		"    .int 0xcb020000\n"
		"    .int 0x17fffffa\n"
		"    .int 0xb4000040\n");
#endif
__asm__(
		"    .int 0xcb2063ff\n"
		"    .int 0x910003e0\n"
		"    .int 0xa9bf7bfd\n"
		"    .reloc ., R_AARCH64_CALL26,  " STR(_(__bound_new_region)) "\n"
																																	 "    .int 0x94000000\n"
																																	 "    .int 0xa8c17bfd\n"
																																	 "    .int 0x910003e0\n"
																																	 "    .int 0xd65f03c0\n");
#else
__asm__(
		"    mov x1, x0\n"
		"    add x0, x0, #16     // Round up to 16-byte boundary\n"
		"    and x0, x0, #-16    // Ensure 16-byte alignment\n");
#ifdef _WIN32
__asm__(
		"    cbz x0, p100        // If size is 0, skip to return\n"
		"    // Windows requires page-wise allocation with stack probing\n"
		"    mov x2, #4096       // Page size = 4096 bytes\n");

__asm__(
		"p101:\n"
		"    cmp x0, x2          // Compare remaining size with page size\n"
		"    b.lo    p102        // If less than page, jump to remainder\n");

__asm__(
		"    // Probe first, then allocate\n"
		"    sub x3, sp, x2      // Calculate guard page address (sp - 4096)\n"
		"    ldr xzr, [x3]       // Touch guard page FIRST\n"
		"    sub sp, sp, x2      // THEN allocate the page\n");

__asm__(
		"    sub x0, x0, x2      // Decrement remaining size\n"
		"    b   p101            // Continue loop\n");

__asm__(
		"p102:\n"
		"    // Allocate remaining bytes (less than one page)\n"
		"    cbz x0, p100        // If no remaining bytes, skip\n"
		"    sub sp, sp, x0      // Allocate remaining space\n");
#else
__asm__(
		"    // Non-Windows: simple one-time allocation\n"
		"    sub sp, sp, x0      // Allocate space on stack\n");
#endif

__asm__(
		"p100:\n"
		"    mov x0, sp          // Return allocated address\n"
		"    stp x29, x30, [sp, #-16]!\n"
		"    bl " STR(_(__bound_new_region)) "\n"
																				 "    ldp x29, x30, [sp], #16\n"
																				 "    mov x0, sp          // Return allocated address\n"
																				 "    ret                 // Return to caller\n");
#endif

#elif defined __riscv

__asm__(
		".globl " STR(_(__bound_alloca)) "\n"
																		 "" STR(_(__bound_alloca)) ":\n"
																															 "    mv     a1, a0\n"
																															 "    sub    sp, sp, a0\n"
																															 "    addi   sp, sp, -16\n"
																															 "    andi   sp, sp, -16\n"
																															 "    add    a0, sp, zero\n"
																															 "    addi   sp,sp,-16\n"
																															 "    sd     s0,0(sp)\n"
																															 "    sd     ra,8(sp)\n"
																															 "    jal    " STR(_(__bound_new_region)) "\n"
																																																				"    ld     s0,0(sp)\n"
																																																				"    ld     ra,8(sp)\n"
																																																				"    addi   sp,sp,16\n"
																																																				"    add    a0, sp, zero\n"
																																																				"    ret\n");

#endif

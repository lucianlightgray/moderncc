#define STR_(x) #x
#define STR(x) STR_(x)
#if defined __leading_underscore || defined __APPLE__
#define _(s) _##s
#else
#define _(s) s
#endif

/* A foreign (host-CC) Mach-O assembler has no .type/.size directives and needs
   assembler-local labels to start with "L", not ".L". mcc emits ELF objects even
   when targeting Mach-O, so it keeps the ELF spelling. */
#if defined __APPLE__ && !defined __MCC__
#define TYPE(s)
#define SIZE(s)
#define LP "L"
#else
#define TYPE(s) "        .type   " s ", %function\n"
#define SIZE(s) "        .size   " s ", .-" s "\n"
#define LP ".L"
#endif

#if defined __i386__

__asm__(
	"        .text\n");

__asm__(
	"        .global " STR(_(__atomic_load_1)) "\n"
											   TYPE(STR(_(__atomic_load_1)))
																						  "" STR(_(__atomic_load_1)) ":\n"
																													 "        mov    0x4(%esp),%eax\n"
																													 "        movzbl (%eax),%eax\n"
																													 "        ret\n"
																													 SIZE(STR(_(__atomic_load_1))));

__asm__(
	"        .global " STR(_(__atomic_load_2)) "\n"
											   TYPE(STR(_(__atomic_load_2)))
																						  "" STR(_(__atomic_load_2)) ":\n"
																													 "        mov    0x4(%esp),%eax\n"
																													 "        movzwl (%eax),%eax\n"
																													 "        ret\n"
																													 SIZE(STR(_(__atomic_load_2))));

__asm__(
	"        .global " STR(_(__atomic_load_4)) "\n"
											   TYPE(STR(_(__atomic_load_4)))
																						  "" STR(_(__atomic_load_4)) ":\n"
																													 "        mov    0x4(%esp),%eax\n"
																													 "        mov    (%eax),%eax\n"
																													 "        ret\n"
																													 SIZE(STR(_(__atomic_load_4))));

__asm__(
	"        .global " STR(_(__atomic_load_8)) "\n"
											   TYPE(STR(_(__atomic_load_8)))
																						  "" STR(_(__atomic_load_8)) ":\n"
																													 "        sub    $0xc,%esp\n"
																													 "        mov    0x10(%esp),%eax\n"
																													 "        fildll (%eax)\n"
																													 "        fistpll (%esp)\n"
																													 "        mov    (%esp),%eax\n"
																													 "        mov    0x4(%esp),%edx\n"
																													 "        add    $0xc,%esp\n"
																													 "        ret\n"
																													 SIZE(STR(_(__atomic_load_8))));

__asm__(
	"        .global " STR(_(__atomic_store_1)) "\n"
												TYPE(STR(_(__atomic_store_1)))
																							"" STR(_(__atomic_store_1)) ":\n"
																														"        mov    0x4(%esp),%edx\n"
																														"        mov    0x8(%esp),%eax\n"
																														"        xchg   %al,(%edx)\n"
																														"        ret\n"
																														SIZE(STR(_(__atomic_store_1))));

__asm__(
	"        .global " STR(_(__atomic_store_2)) "\n"
												TYPE(STR(_(__atomic_store_2)))
																							"" STR(_(__atomic_store_2)) ":\n"
																														"        mov    0x4(%esp),%edx\n"
																														"        mov    0x8(%esp),%eax\n"
																														"        xchg   %ax,(%edx)\n"
																														"        ret\n"
																														SIZE(STR(_(__atomic_store_2))));

__asm__(
	"        .global " STR(_(__atomic_store_4)) "\n"
												TYPE(STR(_(__atomic_store_4)))
																							"" STR(_(__atomic_store_4)) ":\n"
																														"        mov    0x4(%esp),%edx\n"
																														"        mov    0x8(%esp),%eax\n"
																														"        xchg   %eax,(%edx)\n"
																														"        ret\n"
																														SIZE(STR(_(__atomic_store_4))));

__asm__(
	"        .global " STR(_(__atomic_store_8)) "\n"
												TYPE(STR(_(__atomic_store_8)))
																							"" STR(_(__atomic_store_8)) ":\n"
																														"        push   %ebx\n"
																														"        sub    $0x8,%esp\n"
																														"        mov    0x18(%esp),%ebx\n"
																														"        mov    0x14(%esp),%ecx\n"
																														"        mov    %ecx,(%esp)\n"
																														"        mov    %ebx,0x4(%esp)\n"
																														"        fildll (%esp)\n"
																														"        mov    0x10(%esp),%eax\n"
																														"        fistpll (%eax)\n"
																														"        lock orl $0x0,(%esp)\n"
																														"        add    $0x8,%esp\n"
																														"        pop    %ebx\n"
																														"        ret\n"
																														SIZE(STR(_(__atomic_store_8))));

__asm__(
	"        .global " STR(_(__atomic_compare_exchange_1)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_1)))
																												  "" STR(_(__atomic_compare_exchange_1)) ":\n"
																																						 "        push   %ebx\n"
																																						 "        mov    0xc(%esp),%ecx\n"
																																						 "        mov    0x8(%esp),%edx\n"
																																						 "        movzbl 0x10(%esp),%ebx\n"
																																						 "        movzbl (%ecx),%eax\n"
																																						 "        lock cmpxchg %bl,(%edx)\n"
																																						 "        sete   %dl\n"
																																						 "        je      " LP "___atomic_compare_exchange_1_020\n"
																																						 "        mov    %al,(%ecx)\n"
																																						 "" LP "___atomic_compare_exchange_1_020:\n"
																																						 "        mov    %edx,%eax\n"
																																						 "        pop    %ebx\n"
																																						 "        ret\n"
																																						 SIZE(STR(_(__atomic_compare_exchange_1))));

__asm__(
	"        .global " STR(_(__atomic_compare_exchange_2)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_2)))
																												  "" STR(_(__atomic_compare_exchange_2)) ":\n"
																																						 "        push   %ebx\n"
																																						 "        mov    0xc(%esp),%ecx\n"
																																						 "        mov    0x8(%esp),%edx\n"
																																						 "        movzwl 0x10(%esp),%ebx\n"
																																						 "        movzwl (%ecx),%eax\n"
																																						 "        lock cmpxchg %bx,(%edx)\n"
																																						 "        sete   %dl\n"
																																						 "        je      " LP "___atomic_compare_exchange_2_022\n"
																																						 "        mov    %ax,(%ecx)\n"
																																						 "" LP "___atomic_compare_exchange_2_022:\n"
																																						 "        mov    %edx,%eax\n"
																																						 "        pop    %ebx\n"
																																						 "        ret\n"
																																						 SIZE(STR(_(__atomic_compare_exchange_2))));

__asm__(
	"        .global " STR(_(__atomic_compare_exchange_4)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_4)))
																												  "" STR(_(__atomic_compare_exchange_4)) ":\n"
																																						 "        push   %ebx\n"
																																						 "        mov    0xc(%esp),%ecx\n"
																																						 "        mov    0x8(%esp),%edx\n"
																																						 "        mov    0x10(%esp),%ebx\n"
																																						 "        mov    (%ecx),%eax\n"
																																						 "        lock cmpxchg %ebx,(%edx)\n"
																																						 "        sete   %dl\n"
																																						 "        je      " LP "___atomic_compare_exchange_4_01e\n"
																																						 "        mov    %eax,(%ecx)\n"
																																						 "" LP "___atomic_compare_exchange_4_01e:\n"
																																						 "        mov    %edx,%eax\n"
																																						 "        pop    %ebx\n"
																																						 "        ret\n"
																																						 SIZE(STR(_(__atomic_compare_exchange_4))));

__asm__(
	"        .global " STR(_(__atomic_compare_exchange_8)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_8)))
																												  "" STR(_(__atomic_compare_exchange_8)) ":\n"
																																						 "        push   %edi\n"
																																						 "        push   %esi\n"
																																						 "        push   %ebx\n"
																																						 "        mov    0x14(%esp),%esi\n"
																																						 "        mov    0x1c(%esp),%ecx\n"
																																						 "        mov    0x10(%esp),%edi\n"
																																						 "        mov    (%esi),%eax\n"
																																						 "        mov    0x4(%esi),%edx\n"
																																						 "        mov    0x18(%esp),%ebx\n"
																																						 "        lock cmpxchg8b (%edi)\n"
																																						 "        sete   %cl\n"
																																						 "        je      " LP "___atomic_compare_exchange_8_02a\n"
																																						 "        mov    %eax,(%esi)\n"
																																						 "        mov    %edx,0x4(%esi)\n"
																																						 "" LP "___atomic_compare_exchange_8_02a:\n"
																																						 "        pop    %ebx\n"
																																						 "        mov    %ecx,%eax\n"
																																						 "        pop    %esi\n"
																																						 "        pop    %edi\n"
																																						 "        ret\n"
																																						 SIZE(STR(_(__atomic_compare_exchange_8))));

__asm__(
	"        .global " STR(_(__atomic_test_and_set_1)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_1)))
																										  "" STR(_(__atomic_test_and_set_1)) ":\n"
																																			 "        mov    0x4(%esp),%edx\n"
																																			 "        mov    $0x1,%eax\n"
																																			 "        xchg   %al,(%edx)\n"
																																			 "        ret\n"
																																			 SIZE(STR(_(__atomic_test_and_set_1))));

__asm__(
	"        .global " STR(_(__atomic_test_and_set_2)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_2)))
																										  "" STR(_(__atomic_test_and_set_2)) ":\n"
																																			 "        mov    0x4(%esp),%edx\n"
																																			 "        mov    $0x1,%eax\n"
																																			 "        xchg   %al,(%edx)\n"
																																			 "        ret\n"
																																			 SIZE(STR(_(__atomic_test_and_set_2))));

__asm__(
	"        .global " STR(_(__atomic_test_and_set_4)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_4)))
																										  "" STR(_(__atomic_test_and_set_4)) ":\n"
																																			 "        mov    0x4(%esp),%edx\n"
																																			 "        mov    $0x1,%eax\n"
																																			 "        xchg   %al,(%edx)\n"
																																			 "        ret\n"
																																			 SIZE(STR(_(__atomic_test_and_set_4))));

__asm__(
	"        .global " STR(_(__atomic_test_and_set_8)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_8)))
																										  "" STR(_(__atomic_test_and_set_8)) ":\n"
																																			 "        mov    0x4(%esp),%edx\n"
																																			 "        mov    $0x1,%eax\n"
																																			 "        xchg   %al,(%edx)\n"
																																			 "        ret\n"
																																			 SIZE(STR(_(__atomic_test_and_set_8))));

__asm__(
	"        .global " STR(_(atomic_thread_fence)) "\n"
												   TYPE(STR(_(atomic_thread_fence)))
																								  "" STR(_(atomic_thread_fence)) ":\n"
																																 "        lock orl $0x0,(%esp)\n"
																																 "        ret\n"
																																 SIZE(STR(_(atomic_thread_fence))));

__asm__(
	"        .global " STR(_(atomic_signal_fence)) "\n"
												   TYPE(STR(_(atomic_signal_fence)))
																								  "" STR(_(atomic_signal_fence)) ":\n"
																																 "        ret\n"
																																 SIZE(STR(_(atomic_signal_fence))));

__asm__(
	"        .global " STR(_(atomic_flag_test_and_set)) "\n"
														TYPE(STR(_(atomic_flag_test_and_set)))
																											"" STR(_(atomic_flag_test_and_set)) ":\n"
																																				"        mov    0x4(%esp),%edx\n"
																																				"        mov    $0x1,%eax\n"
																																				"        xchg   %al,(%edx)\n"
																																				"        ret\n"
																																				SIZE(STR(_(atomic_flag_test_and_set))));

__asm__(
	"        .global " STR(_(atomic_flag_test_and_set_explicit)) "\n"
																 TYPE(STR(_(atomic_flag_test_and_set_explicit)))
																															  "" STR(_(atomic_flag_test_and_set_explicit)) ":\n"
																																										   "        mov    0x4(%esp),%edx\n"
																																										   "        mov    $0x1,%eax\n"
																																										   "        xchg   %al,(%edx)\n"
																																										   "        ret\n"
																																										   SIZE(STR(_(atomic_flag_test_and_set_explicit))));

__asm__(
	"        .global " STR(_(atomic_flag_clear)) "\n"
												 TYPE(STR(_(atomic_flag_clear)))
																							  "" STR(_(atomic_flag_clear)) ":\n"
																														   "        mov    0x4(%esp),%edx\n"
																														   "        xor    %eax,%eax\n"
																														   "        xchg   %al,(%edx)\n"
																														   "        ret\n"
																														   SIZE(STR(_(atomic_flag_clear))));

__asm__(
	"        .global " STR(_(atomic_flag_clear_explicit)) "\n"
														  TYPE(STR(_(atomic_flag_clear_explicit)))
																												"" STR(_(atomic_flag_clear_explicit)) ":\n"
																																					  "        mov    0x4(%esp),%edx\n"
																																					  "        xor    %eax,%eax\n"
																																					  "        xchg   %al,(%edx)\n"
																																					  "        ret\n"
																																					  SIZE(STR(_(atomic_flag_clear_explicit))));

#endif

#if defined __x86_64__ && !defined _WIN32

__asm__(
	"        .text\n");

__asm__(
	"        .global " STR(_(__atomic_load_1)) "\n"
											   TYPE(STR(_(__atomic_load_1)))
																						  "" STR(_(__atomic_load_1)) ":\n"
																													 "        movzbl (%rdi),%eax\n"
																													 "        ret\n"
																													 SIZE(STR(_(__atomic_load_1))));

__asm__(
	"        .global " STR(_(__atomic_load_2)) "\n"
											   TYPE(STR(_(__atomic_load_2)))
																						  "" STR(_(__atomic_load_2)) ":\n"
																													 "        movzwl (%rdi),%eax\n"
																													 "        ret\n"
																													 SIZE(STR(_(__atomic_load_2))));

__asm__(
	"        .global " STR(_(__atomic_load_4)) "\n"
											   TYPE(STR(_(__atomic_load_4)))
																						  "" STR(_(__atomic_load_4)) ":\n"
																													 "        mov    (%rdi),%eax\n"
																													 "        ret\n"
																													 SIZE(STR(_(__atomic_load_4))));

__asm__(
	"        .global " STR(_(__atomic_load_8)) "\n"
											   TYPE(STR(_(__atomic_load_8)))
																						  "" STR(_(__atomic_load_8)) ":\n"
																													 "        mov    (%rdi),%rax\n"
																													 "        ret\n"
																													 SIZE(STR(_(__atomic_load_8))));

__asm__(
	"        .global " STR(_(__atomic_store_1)) "\n"
												TYPE(STR(_(__atomic_store_1)))
																							"" STR(_(__atomic_store_1)) ":\n"
																														"        xchg   %sil,(%rdi)\n"
																														"        ret\n"
																														SIZE(STR(_(__atomic_store_1))));

__asm__(
	"        .global " STR(_(__atomic_store_2)) "\n"
												TYPE(STR(_(__atomic_store_2)))
																							"" STR(_(__atomic_store_2)) ":\n"
																														"        xchg   %si,(%rdi)\n"
																														"        ret\n"
																														SIZE(STR(_(__atomic_store_2))));

__asm__(
	"        .global " STR(_(__atomic_store_4)) "\n"
												TYPE(STR(_(__atomic_store_4)))
																							"" STR(_(__atomic_store_4)) ":\n"
																														"        xchg   %esi,(%rdi)\n"
																														"        ret\n"
																														SIZE(STR(_(__atomic_store_4))));

__asm__(
	"        .global " STR(_(__atomic_store_8)) "\n"
												TYPE(STR(_(__atomic_store_8)))
																							"" STR(_(__atomic_store_8)) ":\n"
																														"        xchg   %rsi,(%rdi)\n"
																														"        ret\n"
																														SIZE(STR(_(__atomic_store_8))));

__asm__(
	"        .global " STR(_(__atomic_compare_exchange_1)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_1)))
																												  "" STR(_(__atomic_compare_exchange_1)) ":\n"
																																						 "        movzbl (%rsi),%eax\n"
																																						 "        lock cmpxchg %dl,(%rdi)\n"
																																						 "        sete   %dl\n"
																																						 "        je      " LP "___atomic_compare_exchange_1_012\n"
																																						 "        mov    %al,(%rsi)\n"
																																						 "" LP "___atomic_compare_exchange_1_012:\n"
																																						 "        mov    %edx,%eax\n"
																																						 "        ret\n"
																																						 SIZE(STR(_(__atomic_compare_exchange_1))));

__asm__(
	"        .global " STR(_(__atomic_compare_exchange_2)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_2)))
																												  "" STR(_(__atomic_compare_exchange_2)) ":\n"
																																						 "        movzwl (%rsi),%eax\n"
																																						 "        lock cmpxchg %dx,(%rdi)\n"
																																						 "        sete   %dl\n"
																																						 "        je      " LP "___atomic_compare_exchange_2_014\n"
																																						 "        mov    %ax,(%rsi)\n"
																																						 "" LP "___atomic_compare_exchange_2_014:\n"
																																						 "        mov    %edx,%eax\n"
																																						 "        ret\n"
																																						 SIZE(STR(_(__atomic_compare_exchange_2))));

__asm__(
	"        .global " STR(_(__atomic_compare_exchange_4)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_4)))
																												  "" STR(_(__atomic_compare_exchange_4)) ":\n"
																																						 "        mov    (%rsi),%eax\n"
																																						 "        lock cmpxchg %edx,(%rdi)\n"
																																						 "        sete   %dl\n"
																																						 "        je      " LP "___atomic_compare_exchange_4_011\n"
																																						 "        mov    %eax,(%rsi)\n"
																																						 "" LP "___atomic_compare_exchange_4_011:\n"
																																						 "        mov    %edx,%eax\n"
																																						 "        ret\n"
																																						 SIZE(STR(_(__atomic_compare_exchange_4))));

__asm__(
	"        .global " STR(_(__atomic_compare_exchange_8)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_8)))
																												  "" STR(_(__atomic_compare_exchange_8)) ":\n"
																																						 "        mov    (%rsi),%rax\n"
																																						 "        lock cmpxchg %rdx,(%rdi)\n"
																																						 "        sete   %dl\n"
																																						 "        je      " LP "___atomic_compare_exchange_8_014\n"
																																						 "        mov    %rax,(%rsi)\n"
																																						 "" LP "___atomic_compare_exchange_8_014:\n"
																																						 "        mov    %edx,%eax\n"
																																						 "        ret\n"
																																						 SIZE(STR(_(__atomic_compare_exchange_8))));

__asm__(
	"        .global " STR(_(__atomic_test_and_set_1)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_1)))
																										  "" STR(_(__atomic_test_and_set_1)) ":\n"
																																			 "        mov    $0x1,%eax\n"
																																			 "        xchg   %al,(%rdi)\n"
																																			 "        ret\n"
																																			 SIZE(STR(_(__atomic_test_and_set_1))));

__asm__(
	"        .global " STR(_(__atomic_test_and_set_2)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_2)))
																										  "" STR(_(__atomic_test_and_set_2)) ":\n"
																																			 "        mov    $0x1,%eax\n"
																																			 "        xchg   %al,(%rdi)\n"
																																			 "        ret\n"
																																			 SIZE(STR(_(__atomic_test_and_set_2))));

__asm__(
	"        .global " STR(_(__atomic_test_and_set_4)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_4)))
																										  "" STR(_(__atomic_test_and_set_4)) ":\n"
																																			 "        mov    $0x1,%eax\n"
																																			 "        xchg   %al,(%rdi)\n"
																																			 "        ret\n"
																																			 SIZE(STR(_(__atomic_test_and_set_4))));

__asm__(
	"        .global " STR(_(__atomic_test_and_set_8)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_8)))
																										  "" STR(_(__atomic_test_and_set_8)) ":\n"
																																			 "        mov    $0x1,%eax\n"
																																			 "        xchg   %al,(%rdi)\n"
																																			 "        ret\n"
																																			 SIZE(STR(_(__atomic_test_and_set_8))));

__asm__(
	"        .global " STR(_(atomic_thread_fence)) "\n"
												   TYPE(STR(_(atomic_thread_fence)))
																								  "" STR(_(atomic_thread_fence)) ":\n"
																																 "        lock orq $0x0,(%rsp)\n"
																																 "        ret\n"
																																 SIZE(STR(_(atomic_thread_fence))));

__asm__(
	"        .global " STR(_(atomic_signal_fence)) "\n"
												   TYPE(STR(_(atomic_signal_fence)))
																								  "" STR(_(atomic_signal_fence)) ":\n"
																																 "        ret\n"
																																 SIZE(STR(_(atomic_signal_fence))));

__asm__(
	"        .global " STR(_(atomic_flag_test_and_set)) "\n"
														TYPE(STR(_(atomic_flag_test_and_set)))
																											"" STR(_(atomic_flag_test_and_set)) ":\n"
																																				"        mov    $0x1,%eax\n"
																																				"        xchg   %al,(%rdi)\n"
																																				"        ret\n"
																																				SIZE(STR(_(atomic_flag_test_and_set))));

__asm__(
	"        .global " STR(_(atomic_flag_test_and_set_explicit)) "\n"
																 TYPE(STR(_(atomic_flag_test_and_set_explicit)))
																															  "" STR(_(atomic_flag_test_and_set_explicit)) ":\n"
																																										   "        mov    $0x1,%eax\n"
																																										   "        xchg   %al,(%rdi)\n"
																																										   "        ret\n"
																																										   SIZE(STR(_(atomic_flag_test_and_set_explicit))));

__asm__(
	"        .global " STR(_(atomic_flag_clear)) "\n"
												 TYPE(STR(_(atomic_flag_clear)))
																							  "" STR(_(atomic_flag_clear)) ":\n"
																														   "        xor    %eax,%eax\n"
																														   "        xchg   %al,(%rdi)\n"
																														   "        ret\n"
																														   SIZE(STR(_(atomic_flag_clear))));

__asm__(
	"        .global " STR(_(atomic_flag_clear_explicit)) "\n"
														  TYPE(STR(_(atomic_flag_clear_explicit)))
																												"" STR(_(atomic_flag_clear_explicit)) ":\n"
																																					  "        xor    %eax,%eax\n"
																																					  "        xchg   %al,(%rdi)\n"
																																					  "        ret\n"
																																					  SIZE(STR(_(atomic_flag_clear_explicit))));

#endif

#if defined __x86_64__ && defined _WIN32
__asm__(
	"        .text\n");

__asm__(
	"        .global " STR(_(__atomic_load_1)) "\n"
											   TYPE(STR(_(__atomic_load_1)))
																						  "" STR(_(__atomic_load_1)) ":\n"
																													 "        movzbl (%rcx),%eax\n"
																													 "        ret\n"
																													 SIZE(STR(_(__atomic_load_1))));

__asm__(
	"        .global " STR(_(__atomic_load_2)) "\n"
											   TYPE(STR(_(__atomic_load_2)))
																						  "" STR(_(__atomic_load_2)) ":\n"
																													 "        movzwl (%rcx),%eax\n"
																													 "        ret\n"
																													 SIZE(STR(_(__atomic_load_2))));

__asm__(
	"        .global " STR(_(__atomic_load_4)) "\n"
											   TYPE(STR(_(__atomic_load_4)))
																						  "" STR(_(__atomic_load_4)) ":\n"
																													 "        mov    (%rcx),%eax\n"
																													 "        ret\n"
																													 SIZE(STR(_(__atomic_load_4))));

__asm__(
	"        .global " STR(_(__atomic_load_8)) "\n"
											   TYPE(STR(_(__atomic_load_8)))
																						  "" STR(_(__atomic_load_8)) ":\n"
																													 "        mov    (%rcx),%rax\n"
																													 "        ret\n"
																													 SIZE(STR(_(__atomic_load_8))));

__asm__(
	"        .global " STR(_(__atomic_store_1)) "\n"
												TYPE(STR(_(__atomic_store_1)))
																							"" STR(_(__atomic_store_1)) ":\n"
																														"        xchg   %dl,(%rcx)\n"
																														"        ret\n"
																														SIZE(STR(_(__atomic_store_1))));

__asm__(
	"        .global " STR(_(__atomic_store_2)) "\n"
												TYPE(STR(_(__atomic_store_2)))
																							"" STR(_(__atomic_store_2)) ":\n"
																														"        xchg   %dx,(%rcx)\n"
																														"        ret\n"
																														SIZE(STR(_(__atomic_store_2))));

__asm__(
	"        .global " STR(_(__atomic_store_4)) "\n"
												TYPE(STR(_(__atomic_store_4)))
																							"" STR(_(__atomic_store_4)) ":\n"
																														"        xchg   %edx,(%rcx)\n"
																														"        ret\n"
																														SIZE(STR(_(__atomic_store_4))));

__asm__(
	"        .global " STR(_(__atomic_store_8)) "\n"
												TYPE(STR(_(__atomic_store_8)))
																							"" STR(_(__atomic_store_8)) ":\n"
																														"        xchg   %rdx,(%rcx)\n"
																														"        ret\n"
																														SIZE(STR(_(__atomic_store_8))));

__asm__(
	"        .global " STR(_(__atomic_compare_exchange_1)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_1)))
																												  "" STR(_(__atomic_compare_exchange_1)) ":\n"
																																						 "        movzbl (%rdx),%eax\n"
																																						 "        lock cmpxchg %r8b,(%rcx)\n"
																																						 "        sete   %cl\n"
																																						 "        je      " LP "___atomic_compare_exchange_1_00f\n"
																																						 "        mov    %al,(%rdx)\n"
																																						 "" LP "___atomic_compare_exchange_1_00f:\n"
																																						 "        mov    %ecx,%eax\n"
																																						 "        ret\n"
																																						 SIZE(STR(_(__atomic_compare_exchange_1))));

__asm__(
	"        .global " STR(_(__atomic_compare_exchange_2)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_2)))
																												  "" STR(_(__atomic_compare_exchange_2)) ":\n"
																																						 "        movzwl (%rdx),%eax\n"
																																						 "        lock cmpxchg %r8w,(%rcx)\n"
																																						 "        sete   %cl\n"
																																						 "        je      " LP "___atomic_compare_exchange_2_011\n"
																																						 "        mov    %ax,(%rdx)\n"
																																						 "" LP "___atomic_compare_exchange_2_011:\n"
																																						 "        mov    %ecx,%eax\n"
																																						 "        ret\n"
																																						 SIZE(STR(_(__atomic_compare_exchange_2))));

__asm__(
	"        .global " STR(_(__atomic_compare_exchange_4)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_4)))
																												  "" STR(_(__atomic_compare_exchange_4)) ":\n"
																																						 "        mov    (%rdx),%eax\n"
																																						 "        lock cmpxchg %r8d,(%rcx)\n"
																																						 "        sete   %cl\n"
																																						 "        je      " LP "___atomic_compare_exchange_4_00e\n"
																																						 "        mov    %eax,(%rdx)\n"
																																						 "" LP "___atomic_compare_exchange_4_00e:\n"
																																						 "        mov    %ecx,%eax\n"
																																						 "        ret\n"
																																						 SIZE(STR(_(__atomic_compare_exchange_4))));

__asm__(
	"        .global " STR(_(__atomic_compare_exchange_8)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_8)))
																												  "" STR(_(__atomic_compare_exchange_8)) ":\n"
																																						 "        mov    (%rdx),%rax\n"
																																						 "        lock cmpxchg %r8,(%rcx)\n"
																																						 "        sete   %cl\n"
																																						 "        je      " LP "___atomic_compare_exchange_8_010\n"
																																						 "        mov    %rax,(%rdx)\n"
																																						 "" LP "___atomic_compare_exchange_8_010:\n"
																																						 "        mov    %ecx,%eax\n"
																																						 "        ret\n"
																																						 SIZE(STR(_(__atomic_compare_exchange_8))));

__asm__(
	"        .global " STR(_(__atomic_test_and_set_1)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_1)))
																										  "" STR(_(__atomic_test_and_set_1)) ":\n"
																																			 "        mov    $0x1,%eax\n"
																																			 "        xchg   %al,(%rcx)\n"
																																			 "        ret\n"
																																			 SIZE(STR(_(__atomic_test_and_set_1))));

__asm__(
	"        .global " STR(_(__atomic_test_and_set_2)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_2)))
																										  "" STR(_(__atomic_test_and_set_2)) ":\n"
																																			 "        mov    $0x1,%eax\n"
																																			 "        xchg   %al,(%rcx)\n"
																																			 "        ret\n"
																																			 SIZE(STR(_(__atomic_test_and_set_2))));

__asm__(
	"        .global " STR(_(__atomic_test_and_set_4)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_4)))
																										  "" STR(_(__atomic_test_and_set_4)) ":\n"
																																			 "        mov    $0x1,%eax\n"
																																			 "        xchg   %al,(%rcx)\n"
																																			 "        ret\n"
																																			 SIZE(STR(_(__atomic_test_and_set_4))));

__asm__(
	"        .global " STR(_(__atomic_test_and_set_8)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_8)))
																										  "" STR(_(__atomic_test_and_set_8)) ":\n"
																																			 "        mov    $0x1,%eax\n"
																																			 "        xchg   %al,(%rcx)\n"
																																			 "        ret\n"
																																			 SIZE(STR(_(__atomic_test_and_set_8))));

__asm__(
	"        .global " STR(_(atomic_thread_fence)) "\n"
												   TYPE(STR(_(atomic_thread_fence)))
																								  "" STR(_(atomic_thread_fence)) ":\n"
																																 "        lock orq $0x0,(%rsp)\n"
																																 "        ret\n"
																																 SIZE(STR(_(atomic_thread_fence))));

__asm__(
	"        .global " STR(_(atomic_signal_fence)) "\n"
												   TYPE(STR(_(atomic_signal_fence)))
																								  "" STR(_(atomic_signal_fence)) ":\n"
																																 "        ret\n"
																																 SIZE(STR(_(atomic_signal_fence))));

__asm__(
	"        .global " STR(_(atomic_flag_test_and_set)) "\n"
														TYPE(STR(_(atomic_flag_test_and_set)))
																											"" STR(_(atomic_flag_test_and_set)) ":\n"
																																				"        mov    $0x1,%eax\n"
																																				"        xchg   %al,(%rcx)\n"
																																				"        ret\n"
																																				SIZE(STR(_(atomic_flag_test_and_set))));

__asm__(
	"        .global " STR(_(atomic_flag_test_and_set_explicit)) "\n"
																 TYPE(STR(_(atomic_flag_test_and_set_explicit)))
																															  "" STR(_(atomic_flag_test_and_set_explicit)) ":\n"
																																										   "        mov    $0x1,%eax\n"
																																										   "        xchg   %al,(%rcx)\n"
																																										   "        ret\n"
																																										   SIZE(STR(_(atomic_flag_test_and_set_explicit))));

__asm__(
	"        .global " STR(_(atomic_flag_clear)) "\n"
												 TYPE(STR(_(atomic_flag_clear)))
																							  "" STR(_(atomic_flag_clear)) ":\n"
																														   "        xor    %eax,%eax\n"
																														   "        xchg   %al,(%rcx)\n"
																														   "        ret\n"
																														   SIZE(STR(_(atomic_flag_clear))));

__asm__(
	"        .global " STR(_(atomic_flag_clear_explicit)) "\n"
														  TYPE(STR(_(atomic_flag_clear_explicit)))
																												"" STR(_(atomic_flag_clear_explicit)) ":\n"
																																					  "        xor    %eax,%eax\n"
																																					  "        xchg   %al,(%rcx)\n"
																																					  "        ret\n"
																																					  SIZE(STR(_(atomic_flag_clear_explicit))));

#endif

#if defined __arm__
__asm__(
	"        .text\n");
#ifndef __MCC__
__asm__(
	"        .arch   armv6k\n"
	"        .syntax unified\n");
#endif

__asm__(
	"        .global " STR(_(__atomic_load_1)) "\n"
											   TYPE(STR(_(__atomic_load_1)))
																						  "" STR(_(__atomic_load_1)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3510000\n"
	"        .int 0x1a000002\n"
	"        .int 0xe5d00000\n"
	"        .int 0xe6ef0070\n"
	"        .int 0xe12fff1e\n"
	"        .int 0xee070fba\n"
	"        .int 0xe5d00000\n"
	"        .int 0xee070fba\n"
	"        .int 0xe6ef0070\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_load_1_014\n"
	"        ldrb    r0, [r0]\n"
	"        uxtb    r0, r0\n"
	"        bx  lr\n"
	"" LP "___atomic_load_1_014:\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        ldrb    r0, [r0]\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        uxtb    r0, r0\n"
	"        bx  lr\n"
	SIZE(STR(_(__atomic_load_1))));

#endif
__asm__(
	"        .global " STR(_(__atomic_load_2)) "\n"
											   TYPE(STR(_(__atomic_load_2)))
																						  "" STR(_(__atomic_load_2)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3510000\n"
	"        .int 0x1a000002\n"
	"        .int 0xe1d000b0\n"
	"        .int 0xe6ff0070\n"
	"        .int 0xe12fff1e\n"
	"        .int 0xee070fba\n"
	"        .int 0xe1d000b0\n"
	"        .int 0xee070fba\n"
	"        .int 0xe6ff0070\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_load_2_03c\n"
	"        ldrh    r0, [r0]\n"
	"        uxth    r0, r0\n"
	"        bx  lr\n"
	"" LP "___atomic_load_2_03c:\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        ldrh    r0, [r0]\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        uxth    r0, r0\n"
	"        bx  lr\n"
	SIZE(STR(_(__atomic_load_2))));

#endif
__asm__(
	"        .global " STR(_(__atomic_load_4)) "\n"
											   TYPE(STR(_(__atomic_load_4)))
																						  "" STR(_(__atomic_load_4)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3510000\n"
	"        .int 0x1a000001\n"
	"        .int 0xe5900000\n"
	"        .int 0xe12fff1e\n"
	"        .int 0xee070fba\n"
	"        .int 0xe5900000\n"
	"        .int 0xee070fba\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_load_4_060\n"
	"        ldr r0, [r0]\n"
	"        bx  lr\n"
	"" LP "___atomic_load_4_060:\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        ldr r0, [r0]\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        bx  lr\n"
	SIZE(STR(_(__atomic_load_4))));

#endif
__asm__(
	"        .global " STR(_(__atomic_load_8)) "\n"
											   TYPE(STR(_(__atomic_load_8)))
																						  "" STR(_(__atomic_load_8)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3510000\n"
	"        .int 0x1a000001\n"
	"        .int 0xe1b00f9f\n"
	"        .int 0xe12fff1e\n"
	"        .int 0xee070fba\n"
	"        .int 0xe1b00f9f\n"
	"        .int 0xee070fba\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_load_8_080\n"
	"        ldrexd  r0, [r0]\n"
	"        bx  lr\n"
	"" LP "___atomic_load_8_080:\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        ldrexd  r0, [r0]\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        bx  lr\n"
	SIZE(STR(_(__atomic_load_8))));

#endif
__asm__(
	"        .global " STR(_(__atomic_store_1)) "\n"
												TYPE(STR(_(__atomic_store_1)))
																							"" STR(_(__atomic_store_1)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3520000\n"
	"        .int 0x1a000001\n"
	"        .int 0xe5c01000\n"
	"        .int 0xe12fff1e\n"
	"        .int 0xee070fba\n"
	"        .int 0xe5c01000\n"
	"        .int 0xee070fba\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        cmp r2, #0\n"
	"        bne  " LP "___atomic_store_1_0a0\n"
	"        strb    r1, [r0]\n"
	"        bx  lr\n"
	"" LP "___atomic_store_1_0a0:\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        strb    r1, [r0]\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        bx  lr\n"
	SIZE(STR(_(__atomic_store_1))));

#endif
__asm__(
	"        .global " STR(_(__atomic_store_2)) "\n"
												TYPE(STR(_(__atomic_store_2)))
																							"" STR(_(__atomic_store_2)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3520000\n"
	"        .int 0x1a000001\n"
	"        .int 0xe1c010b0\n"
	"        .int 0xe12fff1e\n"
	"        .int 0xee070fba\n"
	"        .int 0xe1c010b0\n"
	"        .int 0xee070fba\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        cmp r2, #0\n"
	"        bne  " LP "___atomic_store_2_0c0\n"
	"        strh    r1, [r0]\n"
	"        bx  lr\n"
	"" LP "___atomic_store_2_0c0:\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        strh    r1, [r0]\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        bx  lr\n"
	SIZE(STR(_(__atomic_store_2))));

#endif
__asm__(
	"        .global " STR(_(__atomic_store_4)) "\n"
												TYPE(STR(_(__atomic_store_4)))
																							"" STR(_(__atomic_store_4)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3520000\n"
	"        .int 0x1a000001\n"
	"        .int 0xe5801000\n"
	"        .int 0xe12fff1e\n"
	"        .int 0xee070fba\n"
	"        .int 0xe5801000\n"
	"        .int 0xee070fba\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        cmp r2, #0\n"
	"        bne  " LP "___atomic_store_4_0e0\n"
	"        str r1, [r0]\n"
	"        bx  lr\n"
	"" LP "___atomic_store_4_0e0:\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        str r1, [r0]\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        bx  lr\n"
	SIZE(STR(_(__atomic_store_4))));

#endif
__asm__(
	"        .global " STR(_(__atomic_store_8)) "\n"
												TYPE(STR(_(__atomic_store_8)))
																							"" STR(_(__atomic_store_8)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe92d0030\n"
	"        .int 0xe1a04002\n"
	"        .int 0xe59d1008\n"
	"        .int 0xe1a05003\n"
	"        .int 0xe3510000\n"
	"        .int 0x1a000005\n"
	"        .int 0xe1b02f9f\n"
	"        .int 0xe1a01f94\n"
	"        .int 0xe3510000\n"
	"        .int 0x1afffffb\n"
	"        .int 0xe8bd0030\n"
	"        .int 0xe12fff1e\n"
	"        .int 0xee070fba\n"
	"        .int 0xe1b02f9f\n"
	"        .int 0xe1a01f94\n"
	"        .int 0xe3510000\n"
	"        .int 0x1afffffb\n"
	"        .int 0xee070fba\n"
	"        .int 0xe8bd0030\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        push    {r4, r5}\n"
	"        mov r4, r2\n"
	"        ldr r1, [sp, #8]\n"
	"        mov r5, r3\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_store_8_120\n"
	"" LP "___atomic_store_8_108:\n"
	"        ldrexd  r2, [r0]\n"
	"        strexd  r1, r4, [r0]\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_store_8_108\n"
	"        pop {r4, r5}\n"
	"        bx  lr\n"
	"" LP "___atomic_store_8_120:\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"" LP "___atomic_store_8_124:\n"
	"        ldrexd  r2, [r0]\n"
	"        strexd  r1, r4, [r0]\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_store_8_124\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        pop {r4, r5}\n"
	"        bx  lr\n"
	SIZE(STR(_(__atomic_store_8))));

#endif
__asm__(
	"        .global " STR(_(__atomic_compare_exchange_1)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_1)))
																												  "" STR(_(__atomic_compare_exchange_1)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe52de004\n"
	"        .int 0xe3530000\n"
	"        .int 0x1a00000a\n"
	"        .int 0xe5d13000\n"
	"        .int 0xe1d0cf9f\n"
	"        .int 0xe15c0003\n"
	"        .int 0x1a000002\n"
	"        .int 0xe1c0ef92\n"
	"        .int 0xe35e0000\n"
	"        .int 0x1afffff9\n"
	"        .int 0x03a00001\n"
	"        .int 0x13a00000\n"
	"        .int 0x15c1c000\n"
	"        .int 0xe49df004\n"
	"        .int 0xe5d13000\n"
	"        .int 0xee070fba\n"
	"        .int 0xe1d0cf9f\n"
	"        .int 0xe15c0003\n"
	"        .int 0x1afffff6\n"
	"        .int 0xe1c0ef92\n"
	"        .int 0xe35e0000\n"
	"        .int 0x1afffff9\n"
	"        .int 0xee070fba\n"
	"        .int 0xeafffff1\n");
#else
__asm__(
	"        push    {lr}        @ (str lr, [sp, #-4]!)\n"
	"        cmp r3, #0\n"
	"        bne  " LP "___atomic_compare_exchange_1_178\n"
	"        ldrb    r3, [r1]\n"
	"" LP "___atomic_compare_exchange_1_150:\n"
	"        ldrexb  ip, [r0]\n"
	"        cmp ip, r3\n"
	"        bne  " LP "___atomic_compare_exchange_1_168\n"
	"        strexb  lr, r2, [r0]\n"
	"        cmp lr, #0\n"
	"        bne  " LP "___atomic_compare_exchange_1_150\n"
	"" LP "___atomic_compare_exchange_1_168:\n"
	"        moveq   r0, #1\n"
	"        movne   r0, #0\n"
	"        strbne  ip, [r1]\n"
	"        pop {pc}        @ (ldr pc, [sp], #4)\n"
	"" LP "___atomic_compare_exchange_1_178:\n"
	"        ldrb    r3, [r1]\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"" LP "___atomic_compare_exchange_1_180:\n"
	"        ldrexb  ip, [r0]\n"
	"        cmp ip, r3\n"
	"        bne  " LP "___atomic_compare_exchange_1_168\n"
	"        strexb  lr, r2, [r0]\n"
	"        cmp lr, #0\n"
	"        bne  " LP "___atomic_compare_exchange_1_180\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        b    " LP "___atomic_compare_exchange_1_168\n"
	SIZE(STR(_(__atomic_compare_exchange_1))));

#endif
__asm__(
	"        .global " STR(_(__atomic_compare_exchange_2)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_2)))
																												  "" STR(_(__atomic_compare_exchange_2)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe52de004\n"
	"        .int 0xe3530000\n"
	"        .int 0x1a00000a\n"
	"        .int 0xe1d130b0\n"
	"        .int 0xe1f0cf9f\n"
	"        .int 0xe15c0003\n"
	"        .int 0x1a000002\n"
	"        .int 0xe1e0ef92\n"
	"        .int 0xe35e0000\n"
	"        .int 0x1afffff9\n"
	"        .int 0x03a00001\n"
	"        .int 0x13a00000\n"
	"        .int 0x11c1c0b0\n"
	"        .int 0xe49df004\n"
	"        .int 0xe1d130b0\n"
	"        .int 0xee070fba\n"
	"        .int 0xe1f0cf9f\n"
	"        .int 0xe15c0003\n"
	"        .int 0x1afffff6\n"
	"        .int 0xe1e0ef92\n"
	"        .int 0xe35e0000\n"
	"        .int 0x1afffff9\n"
	"        .int 0xee070fba\n"
	"        .int 0xeafffff1\n");
#else
__asm__(
	"        push    {lr}        @ (str lr, [sp, #-4]!)\n"
	"        cmp r3, #0\n"
	"        bne  " LP "___atomic_compare_exchange_2_1d8\n"
	"        ldrh    r3, [r1]\n"
	"" LP "___atomic_compare_exchange_2_1b0:\n"
	"        ldrexh  ip, [r0]\n"
	"        cmp ip, r3\n"
	"        bne  " LP "___atomic_compare_exchange_2_1c8\n"
	"        strexh  lr, r2, [r0]\n"
	"        cmp lr, #0\n"
	"        bne  " LP "___atomic_compare_exchange_2_1b0\n"
	"" LP "___atomic_compare_exchange_2_1c8:\n"
	"        moveq   r0, #1\n"
	"        movne   r0, #0\n"
	"        strhne  ip, [r1]\n"
	"        pop {pc}        @ (ldr pc, [sp], #4)\n"
	"" LP "___atomic_compare_exchange_2_1d8:\n"
	"        ldrh    r3, [r1]\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"" LP "___atomic_compare_exchange_2_1e0:\n"
	"        ldrexh  ip, [r0]\n"
	"        cmp ip, r3\n"
	"        bne  " LP "___atomic_compare_exchange_2_1c8\n"
	"        strexh  lr, r2, [r0]\n"
	"        cmp lr, #0\n"
	"        bne  " LP "___atomic_compare_exchange_2_1e0\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        b    " LP "___atomic_compare_exchange_2_1c8\n"
	SIZE(STR(_(__atomic_compare_exchange_2))));

#endif
__asm__(
	"        .global " STR(_(__atomic_compare_exchange_4)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_4)))
																												  "" STR(_(__atomic_compare_exchange_4)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe52d4004\n"
	"        .int 0xe3530000\n"
	"        .int 0x1a00000b\n"
	"        .int 0xe5913000\n"
	"        .int 0xe1904f9f\n"
	"        .int 0xe1540003\n"
	"        .int 0x1a000002\n"
	"        .int 0xe180cf92\n"
	"        .int 0xe35c0000\n"
	"        .int 0x1afffff9\n"
	"        .int 0x03a00001\n"
	"        .int 0x13a00000\n"
	"        .int 0x15814000\n"
	"        .int 0xe49d4004\n"
	"        .int 0xe12fff1e\n"
	"        .int 0xe5913000\n"
	"        .int 0xee070fba\n"
	"        .int 0xe1904f9f\n"
	"        .int 0xe1540003\n"
	"        .int 0x1afffff5\n"
	"        .int 0xe180cf92\n"
	"        .int 0xe35c0000\n"
	"        .int 0x1afffff9\n"
	"        .int 0xee070fba\n"
	"        .int 0xeafffff0\n");
#else
__asm__(
	"        push    {r4}        @ (str r4, [sp, #-4]!)\n"
	"        cmp r3, #0\n"
	"        bne  " LP "___atomic_compare_exchange_4_23c\n"
	"        ldr r3, [r1]\n"
	"" LP "___atomic_compare_exchange_4_210:\n"
	"        ldrex   r4, [r0]\n"
	"        cmp r4, r3\n"
	"        bne  " LP "___atomic_compare_exchange_4_228\n"
	"        strex   ip, r2, [r0]\n"
	"        cmp ip, #0\n"
	"        bne  " LP "___atomic_compare_exchange_4_210\n"
	"" LP "___atomic_compare_exchange_4_228:\n"
	"        moveq   r0, #1\n"
	"        movne   r0, #0\n"
	"        strne   r4, [r1]\n"
	"        pop {r4}        @ (ldr r4, [sp], #4)\n"
	"        bx  lr\n"
	"" LP "___atomic_compare_exchange_4_23c:\n"
	"        ldr r3, [r1]\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"" LP "___atomic_compare_exchange_4_244:\n"
	"        ldrex   r4, [r0]\n"
	"        cmp r4, r3\n"
	"        bne  " LP "___atomic_compare_exchange_4_228\n"
	"        strex   ip, r2, [r0]\n"
	"        cmp ip, #0\n"
	"        bne  " LP "___atomic_compare_exchange_4_244\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        b    " LP "___atomic_compare_exchange_4_228\n"
	SIZE(STR(_(__atomic_compare_exchange_4))));

#endif
__asm__(
	"        .global " STR(_(__atomic_compare_exchange_8)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_8)))
																												  "" STR(_(__atomic_compare_exchange_8)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe92d00f0\n"
	"        .int 0xe1a05003\n"
	"        .int 0xe59d3010\n"
	"        .int 0xe1a04002\n"
	"        .int 0xe3530000\n"
	"        .int 0x1a00000c\n"
	"        .int 0xe1c120d0\n"
	"        .int 0xe1b06f9f\n"
	"        .int 0xe1570003\n"
	"        .int 0x01560002\n"
	"        .int 0x1a000002\n"
	"        .int 0xe1a0cf94\n"
	"        .int 0xe35c0000\n"
	"        .int 0x1afffff8\n"
	"        .int 0x03a00001\n"
	"        .int 0x13a00000\n"
	"        .int 0x11c160f0\n"
	"        .int 0xe8bd00f0\n"
	"        .int 0xe12fff1e\n"
	"        .int 0xe1c120d0\n"
	"        .int 0xee070fba\n"
	"        .int 0xe1b06f9f\n"
	"        .int 0xe1570003\n"
	"        .int 0x01560002\n"
	"        .int 0x1afffff4\n"
	"        .int 0xe1a0cf94\n"
	"        .int 0xe35c0000\n"
	"        .int 0x1afffff8\n"
	"        .int 0xee070fba\n"
	"        .int 0xeaffffef\n");
#else
__asm__(
	"        push    {r4, r5, r6, r7}\n"
	"        mov r5, r3\n"
	"        ldr r3, [sp, #16]\n"
	"        mov r4, r2\n"
	"        cmp r3, #0\n"
	"        bne  " LP "___atomic_compare_exchange_8_2b0\n"
	"        ldrd    r2, [r1]\n"
	"" LP "___atomic_compare_exchange_8_280:\n"
	"        ldrexd  r6, [r0]\n"
	"        cmp r7, r3\n"
	"        cmpeq   r6, r2\n"
	"        bne  " LP "___atomic_compare_exchange_8_29c\n"
	"        strexd  ip, r4, [r0]\n"
	"        cmp ip, #0\n"
	"        bne  " LP "___atomic_compare_exchange_8_280\n"
	"" LP "___atomic_compare_exchange_8_29c:\n"
	"        moveq   r0, #1\n"
	"        movne   r0, #0\n"
	"        strdne  r6, [r1]\n"
	"        pop {r4, r5, r6, r7}\n"
	"        bx  lr\n"
	"" LP "___atomic_compare_exchange_8_2b0:\n"
	"        ldrd    r2, [r1]\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"" LP "___atomic_compare_exchange_8_2b8:\n"
	"        ldrexd  r6, [r0]\n"
	"        cmp r7, r3\n"
	"        cmpeq   r6, r2\n"
	"        bne  " LP "___atomic_compare_exchange_8_29c\n"
	"        strexd  ip, r4, [r0]\n"
	"        cmp ip, #0\n"
	"        bne  " LP "___atomic_compare_exchange_8_2b8\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        b    " LP "___atomic_compare_exchange_8_29c\n"
	SIZE(STR(_(__atomic_compare_exchange_8))));

#endif
__asm__(
	"        .global " STR(_(__atomic_test_and_set_1)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_1)))
																										  "" STR(_(__atomic_test_and_set_1)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3a02001\n"
	"        .int 0xe3510000\n"
	"        .int 0x1a000005\n"
	"        .int 0xe1d03f9f\n"
	"        .int 0xe1c01f92\n"
	"        .int 0xe3510000\n"
	"        .int 0x1afffffb\n"
	"        .int 0xe6ef0073\n"
	"        .int 0xe12fff1e\n"
	"        .int 0xee070fba\n"
	"        .int 0xe1d03f9f\n"
	"        .int 0xe1c01f92\n"
	"        .int 0xe3510000\n"
	"        .int 0x1afffffb\n"
	"        .int 0xe6ef0073\n"
	"        .int 0xee070fba\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        mov r2, #1\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_test_and_set_1_300\n"
	"" LP "___atomic_test_and_set_1_2e8:\n"
	"        ldrexb  r3, [r0]\n"
	"        strexb  r1, r2, [r0]\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_test_and_set_1_2e8\n"
	"        uxtb    r0, r3\n"
	"        bx  lr\n"
	"" LP "___atomic_test_and_set_1_300:\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"" LP "___atomic_test_and_set_1_304:\n"
	"        ldrexb  r3, [r0]\n"
	"        strexb  r1, r2, [r0]\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_test_and_set_1_304\n"
	"        uxtb    r0, r3\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        bx  lr\n"
	SIZE(STR(_(__atomic_test_and_set_1))));

#endif
__asm__(
	"        .global " STR(_(__atomic_test_and_set_2)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_2)))
																										  "" STR(_(__atomic_test_and_set_2)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3a02001\n"
	"        .int 0xe3510000\n"
	"        .int 0x1a000005\n"
	"        .int 0xe1d03f9f\n"
	"        .int 0xe1c01f92\n"
	"        .int 0xe3510000\n"
	"        .int 0x1afffffb\n"
	"        .int 0xe6ef0073\n"
	"        .int 0xe12fff1e\n"
	"        .int 0xee070fba\n"
	"        .int 0xe1d03f9f\n"
	"        .int 0xe1c01f92\n"
	"        .int 0xe3510000\n"
	"        .int 0x1afffffb\n"
	"        .int 0xe6ef0073\n"
	"        .int 0xee070fba\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        mov r2, #1\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_test_and_set_2_344\n"
	"" LP "___atomic_test_and_set_2_32c:\n"
	"        ldrexb  r3, [r0]\n"
	"        strexb  r1, r2, [r0]\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_test_and_set_2_32c\n"
	"        uxtb    r0, r3\n"
	"        bx  lr\n"
	"" LP "___atomic_test_and_set_2_344:\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"" LP "___atomic_test_and_set_2_348:\n"
	"        ldrexb  r3, [r0]\n"
	"        strexb  r1, r2, [r0]\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_test_and_set_2_348\n"
	"        uxtb    r0, r3\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        bx  lr\n"
	SIZE(STR(_(__atomic_test_and_set_2))));

#endif
__asm__(
	"        .global " STR(_(__atomic_test_and_set_4)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_4)))
																										  "" STR(_(__atomic_test_and_set_4)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3a02001\n"
	"        .int 0xe3510000\n"
	"        .int 0x1a000005\n"
	"        .int 0xe1d03f9f\n"
	"        .int 0xe1c01f92\n"
	"        .int 0xe3510000\n"
	"        .int 0x1afffffb\n"
	"        .int 0xe6ef0073\n"
	"        .int 0xe12fff1e\n"
	"        .int 0xee070fba\n"
	"        .int 0xe1d03f9f\n"
	"        .int 0xe1c01f92\n"
	"        .int 0xe3510000\n"
	"        .int 0x1afffffb\n"
	"        .int 0xe6ef0073\n"
	"        .int 0xee070fba\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        mov r2, #1\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_test_and_set_4_388\n"
	"" LP "___atomic_test_and_set_4_370:\n"
	"        ldrexb  r3, [r0]\n"
	"        strexb  r1, r2, [r0]\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_test_and_set_4_370\n"
	"        uxtb    r0, r3\n"
	"        bx  lr\n"
	"" LP "___atomic_test_and_set_4_388:\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"" LP "___atomic_test_and_set_4_38c:\n"
	"        ldrexb  r3, [r0]\n"
	"        strexb  r1, r2, [r0]\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_test_and_set_4_38c\n"
	"        uxtb    r0, r3\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        bx  lr\n"
	SIZE(STR(_(__atomic_test_and_set_4))));

#endif
__asm__(
	"        .global " STR(_(__atomic_test_and_set_8)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_8)))
																										  "" STR(_(__atomic_test_and_set_8)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3a02001\n"
	"        .int 0xe3510000\n"
	"        .int 0x1a000005\n"
	"        .int 0xe1d03f9f\n"
	"        .int 0xe1c01f92\n"
	"        .int 0xe3510000\n"
	"        .int 0x1afffffb\n"
	"        .int 0xe6ef0073\n"
	"        .int 0xe12fff1e\n"
	"        .int 0xee070fba\n"
	"        .int 0xe1d03f9f\n"
	"        .int 0xe1c01f92\n"
	"        .int 0xe3510000\n"
	"        .int 0x1afffffb\n"
	"        .int 0xe6ef0073\n"
	"        .int 0xee070fba\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        mov r2, #1\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_test_and_set_8_3cc\n"
	"" LP "___atomic_test_and_set_8_3b4:\n"
	"        ldrexb  r3, [r0]\n"
	"        strexb  r1, r2, [r0]\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_test_and_set_8_3b4\n"
	"        uxtb    r0, r3\n"
	"        bx  lr\n"
	"" LP "___atomic_test_and_set_8_3cc:\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"" LP "___atomic_test_and_set_8_3d0:\n"
	"        ldrexb  r3, [r0]\n"
	"        strexb  r1, r2, [r0]\n"
	"        cmp r1, #0\n"
	"        bne  " LP "___atomic_test_and_set_8_3d0\n"
	"        uxtb    r0, r3\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        bx  lr\n"
	SIZE(STR(_(__atomic_test_and_set_8))));

#endif
__asm__(
	"        .global " STR(_(atomic_thread_fence)) "\n"
												   TYPE(STR(_(atomic_thread_fence)))
																								  "" STR(_(atomic_thread_fence)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xee070fba\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        bx  lr\n"
	SIZE(STR(_(atomic_thread_fence))));

#endif
__asm__(
	"        .global " STR(_(atomic_signal_fence)) "\n"
												   TYPE(STR(_(atomic_signal_fence)))
																								  "" STR(_(atomic_signal_fence)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        bx  lr\n"
	SIZE(STR(_(atomic_signal_fence))));

#endif
__asm__(
	"        .global " STR(_(atomic_flag_test_and_set)) "\n"
														TYPE(STR(_(atomic_flag_test_and_set)))
																											"" STR(_(atomic_flag_test_and_set)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3a02001\n"
	"        .int 0xee070fba\n"
	"        .int 0xe1d03f9f\n"
	"        .int 0xe1c01f92\n"
	"        .int 0xe3510000\n"
	"        .int 0x1afffffb\n"
	"        .int 0xe6ef0073\n"
	"        .int 0xee070fba\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        mov r2, #1\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"" LP "_atomic_flag_test_and_set_400:\n"
	"        ldrexb  r3, [r0]\n"
	"        strexb  r1, r2, [r0]\n"
	"        cmp r1, #0\n"
	"        bne  " LP "_atomic_flag_test_and_set_400\n"
	"        uxtb    r0, r3\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        bx  lr\n"
	SIZE(STR(_(atomic_flag_test_and_set))));

#endif
__asm__(
	"        .global " STR(_(atomic_flag_test_and_set_explicit)) "\n"
																 TYPE(STR(_(atomic_flag_test_and_set_explicit)))
																															  "" STR(_(atomic_flag_test_and_set_explicit)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3a02001\n"
	"        .int 0xee070fba\n"
	"        .int 0xe1d03f9f\n"
	"        .int 0xe1c01f92\n"
	"        .int 0xe3510000\n"
	"        .int 0x1afffffb\n"
	"        .int 0xe6ef0073\n"
	"        .int 0xee070fba\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        mov r2, #1\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"" LP "_atomic_flag_test_and_set_explicit_424:\n"
	"        ldrexb  r3, [r0]\n"
	"        strexb  r1, r2, [r0]\n"
	"        cmp r1, #0\n"
	"        bne  " LP "_atomic_flag_test_and_set_explicit_424\n"
	"        uxtb    r0, r3\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        bx  lr\n"
	SIZE(STR(_(atomic_flag_test_and_set_explicit))));

#endif
__asm__(
	"        .global " STR(_(atomic_flag_clear)) "\n"
												 TYPE(STR(_(atomic_flag_clear)))
																							  "" STR(_(atomic_flag_clear)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3b03000\n"
	"        .int 0xee070fba\n"
	"        .int 0xe5c03000\n"
	"        .int 0xee070fba\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        movs    r3, #0\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        strb    r3, [r0]\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        bx  lr\n"
	SIZE(STR(_(atomic_flag_clear))));

#endif
__asm__(
	"        .global " STR(_(atomic_flag_clear_explicit)) "\n"
														  TYPE(STR(_(atomic_flag_clear_explicit)))
																												"" STR(_(atomic_flag_clear_explicit)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xe3b03000\n"
	"        .int 0xee070fba\n"
	"        .int 0xe5c03000\n"
	"        .int 0xee070fba\n"
	"        .int 0xe12fff1e\n");
#else
__asm__(
	"        movs    r3, #0\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        strb    r3, [r0]\n"
	"        mcr     p15, #0, r0, c7, c10, #5\n"
	"        bx  lr\n"
	SIZE(STR(_(atomic_flag_clear_explicit))));

#endif
#endif

#if defined __aarch64__
__asm__(
	"        .text\n");

__asm__(
	"        .global " STR(_(__atomic_load_1)) "\n"
											   TYPE(STR(_(__atomic_load_1)))
																						  "" STR(_(__atomic_load_1)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x35000081\n"
	"        .int 0x39400000\n"
	"        .int 0x12001c00\n"
	"        .int 0xd65f03c0\n"
	"        .int 0x08dffc00\n"
	"        .int 0x12001c00\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        cbnz    w1,  " LP "___atomic_load_1_010\n"
	"        ldrb    w0, [x0]\n"
	"        and w0, w0, #0xff\n"
	"        ret\n"
	"" LP "___atomic_load_1_010:\n"
	"        ldarb   w0, [x0]\n"
	"        and w0, w0, #0xff\n"
	"        ret\n"
	SIZE(STR(_(__atomic_load_1))));

#endif
__asm__(
	"        .global " STR(_(__atomic_load_2)) "\n"
											   TYPE(STR(_(__atomic_load_2)))
																						  "" STR(_(__atomic_load_2)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x35000081\n"
	"        .int 0x79400000\n"
	"        .int 0x12003c00\n"
	"        .int 0xd65f03c0\n"
	"        .int 0x48dffc00\n"
	"        .int 0x12003c00\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        cbnz    w1,  " LP "___atomic_load_2_010\n"
	"        ldrh    w0, [x0]\n"
	"        and w0, w0, #0xffff\n"
	"        ret\n"
	"" LP "___atomic_load_2_010:\n"
	"        ldarh   w0, [x0]\n"
	"        and w0, w0, #0xffff\n"
	"        ret\n"
	SIZE(STR(_(__atomic_load_2))));

#endif
__asm__(
	"        .global " STR(_(__atomic_load_4)) "\n"
											   TYPE(STR(_(__atomic_load_4)))
																						  "" STR(_(__atomic_load_4)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x35000061\n"
	"        .int 0xb9400000\n"
	"        .int 0xd65f03c0\n"
	"        .int 0x88dffc00\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        cbnz    w1,  " LP "___atomic_load_4_00c\n"
	"        ldr w0, [x0]\n"
	"        ret\n"
	"" LP "___atomic_load_4_00c:\n"
	"        ldar    w0, [x0]\n"
	"        ret\n"
	SIZE(STR(_(__atomic_load_4))));

#endif
__asm__(
	"        .global " STR(_(__atomic_load_8)) "\n"
											   TYPE(STR(_(__atomic_load_8)))
																						  "" STR(_(__atomic_load_8)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x35000061\n"
	"        .int 0xf9400000\n"
	"        .int 0xd65f03c0\n"
	"        .int 0xc8dffc00\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        cbnz    w1,  " LP "___atomic_load_8_00c\n"
	"        ldr x0, [x0]\n"
	"        ret\n"
	"" LP "___atomic_load_8_00c:\n"
	"        ldar    x0, [x0]\n"
	"        ret\n"
	SIZE(STR(_(__atomic_load_8))));

#endif
__asm__(
	"        .global " STR(_(__atomic_store_1)) "\n"
												TYPE(STR(_(__atomic_store_1)))
																							"" STR(_(__atomic_store_1)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x12001c21\n"
	"        .int 0x35000062\n"
	"        .int 0x39000001\n"
	"        .int 0xd65f03c0\n"
	"        .int 0x089ffc01\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        and w1, w1, #0xff\n"
	"        cbnz    w2,  " LP "___atomic_store_1_010\n"
	"        strb    w1, [x0]\n"
	"        ret\n"
	"" LP "___atomic_store_1_010:\n"
	"        stlrb   w1, [x0]\n"
	"        ret\n"
	SIZE(STR(_(__atomic_store_1))));

#endif
__asm__(
	"        .global " STR(_(__atomic_store_2)) "\n"
												TYPE(STR(_(__atomic_store_2)))
																							"" STR(_(__atomic_store_2)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x12003c21\n"
	"        .int 0x35000062\n"
	"        .int 0x79000001\n"
	"        .int 0xd65f03c0\n"
	"        .int 0x489ffc01\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        and w1, w1, #0xffff\n"
	"        cbnz    w2,  " LP "___atomic_store_2_010\n"
	"        strh    w1, [x0]\n"
	"        ret\n"
	"" LP "___atomic_store_2_010:\n"
	"        stlrh   w1, [x0]\n"
	"        ret\n"
	SIZE(STR(_(__atomic_store_2))));

#endif
__asm__(
	"        .global " STR(_(__atomic_store_4)) "\n"
												TYPE(STR(_(__atomic_store_4)))
																							"" STR(_(__atomic_store_4)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x35000062\n"
	"        .int 0xb9000001\n"
	"        .int 0xd65f03c0\n"
	"        .int 0x889ffc01\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        cbnz    w2,  " LP "___atomic_store_4_00c\n"
	"        str w1, [x0]\n"
	"        ret\n"
	"" LP "___atomic_store_4_00c:\n"
	"        stlr    w1, [x0]\n"
	"        ret\n"
	SIZE(STR(_(__atomic_store_4))));

#endif
__asm__(
	"        .global " STR(_(__atomic_store_8)) "\n"
												TYPE(STR(_(__atomic_store_8)))
																							"" STR(_(__atomic_store_8)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x35000062\n"
	"        .int 0xf9000001\n"
	"        .int 0xd65f03c0\n"
	"        .int 0xc89ffc01\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        cbnz    w2,  " LP "___atomic_store_8_00c\n"
	"        str x1, [x0]\n"
	"        ret\n"
	"" LP "___atomic_store_8_00c:\n"
	"        stlr    x1, [x0]\n"
	"        ret\n"
	SIZE(STR(_(__atomic_store_8))));

#endif
__asm__(
	"        .global " STR(_(__atomic_compare_exchange_1)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_1)))
																												  "" STR(_(__atomic_compare_exchange_1)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x12001c42\n"
	"        .int 0x35000143\n"
	"        .int 0x39400023\n"
	"        .int 0x085f7c04\n"
	"        .int 0x6b23009f\n"
	"        .int 0x54000061\n"
	"        .int 0x08057c02\n"
	"        .int 0x35ffff85\n"
	"        .int 0x1a9f17e0\n"
	"        .int 0x54000141\n"
	"        .int 0xd65f03c0\n"
	"        .int 0x39400023\n"
	"        .int 0x085ffc04\n"
	"        .int 0x6b23009f\n"
	"        .int 0x54000061\n"
	"        .int 0x0805fc02\n"
	"        .int 0x35ffff85\n"
	"        .int 0x1a9f17e0\n"
	"        .int 0x54ffff00\n"
	"        .int 0x39000024\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        and w2, w2, #0xff\n"
	"        cbnz    w3,  " LP "___atomic_compare_exchange_1_02c\n"
	"        ldrb    w3, [x1]\n"
	"" LP "___atomic_compare_exchange_1_00c:\n"
	"        ldxrb   w4, [x0]\n"
	"        cmp w4, w3, uxtb\n"
	"        b.ne     " LP "___atomic_compare_exchange_1_020\n"
	"        stxrb   w5, w2, [x0]\n"
	"        cbnz    w5,  " LP "___atomic_compare_exchange_1_00c\n"
	"" LP "___atomic_compare_exchange_1_020:\n"
	"        cset    w0, eq  // eq = none\n"
	"        b.ne     " LP "___atomic_compare_exchange_1_04c\n"
	"" LP "___atomic_compare_exchange_1_028:\n"
	"        ret\n"
	"" LP "___atomic_compare_exchange_1_02c:\n"
	"        ldrb    w3, [x1]\n"
	"" LP "___atomic_compare_exchange_1_030:\n"
	"        ldaxrb  w4, [x0]\n"
	"        cmp w4, w3, uxtb\n"
	"        b.ne     " LP "___atomic_compare_exchange_1_044\n"
	"        stlxrb  w5, w2, [x0]\n"
	"        cbnz    w5,  " LP "___atomic_compare_exchange_1_030\n"
	"" LP "___atomic_compare_exchange_1_044:\n"
	"        cset    w0, eq  // eq = none\n"
	"        b.eq     " LP "___atomic_compare_exchange_1_028\n"
	"" LP "___atomic_compare_exchange_1_04c:\n"
	"        strb    w4, [x1]\n"
	"        ret\n"
	SIZE(STR(_(__atomic_compare_exchange_1))));

#endif
__asm__(
	"        .global " STR(_(__atomic_compare_exchange_2)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_2)))
																												  "" STR(_(__atomic_compare_exchange_2)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x12003c42\n"
	"        .int 0x35000143\n"
	"        .int 0x79400023\n"
	"        .int 0x485f7c04\n"
	"        .int 0x6b23209f\n"
	"        .int 0x54000061\n"
	"        .int 0x48057c02\n"
	"        .int 0x35ffff85\n"
	"        .int 0x1a9f17e0\n"
	"        .int 0x54000141\n"
	"        .int 0xd65f03c0\n"
	"        .int 0x79400023\n"
	"        .int 0x485ffc04\n"
	"        .int 0x6b23209f\n"
	"        .int 0x54000061\n"
	"        .int 0x4805fc02\n"
	"        .int 0x35ffff85\n"
	"        .int 0x1a9f17e0\n"
	"        .int 0x54ffff00\n"
	"        .int 0x79000024\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        and w2, w2, #0xffff\n"
	"        cbnz    w3,  " LP "___atomic_compare_exchange_2_02c\n"
	"        ldrh    w3, [x1]\n"
	"" LP "___atomic_compare_exchange_2_00c:\n"
	"        ldxrh   w4, [x0]\n"
	"        cmp w4, w3, uxth\n"
	"        b.ne     " LP "___atomic_compare_exchange_2_020\n"
	"        stxrh   w5, w2, [x0]\n"
	"        cbnz    w5,  " LP "___atomic_compare_exchange_2_00c\n"
	"" LP "___atomic_compare_exchange_2_020:\n"
	"        cset    w0, eq  // eq = none\n"
	"        b.ne     " LP "___atomic_compare_exchange_2_04c\n"
	"" LP "___atomic_compare_exchange_2_028:\n"
	"        ret\n"
	"" LP "___atomic_compare_exchange_2_02c:\n"
	"        ldrh    w3, [x1]\n"
	"" LP "___atomic_compare_exchange_2_030:\n"
	"        ldaxrh  w4, [x0]\n"
	"        cmp w4, w3, uxth\n"
	"        b.ne     " LP "___atomic_compare_exchange_2_044\n"
	"        stlxrh  w5, w2, [x0]\n"
	"        cbnz    w5,  " LP "___atomic_compare_exchange_2_030\n"
	"" LP "___atomic_compare_exchange_2_044:\n"
	"        cset    w0, eq  // eq = none\n"
	"        b.eq     " LP "___atomic_compare_exchange_2_028\n"
	"" LP "___atomic_compare_exchange_2_04c:\n"
	"        strh    w4, [x1]\n"
	"        ret\n"
	SIZE(STR(_(__atomic_compare_exchange_2))));

#endif
__asm__(
	"        .global " STR(_(__atomic_compare_exchange_4)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_4)))
																												  "" STR(_(__atomic_compare_exchange_4)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x35000143\n"
	"        .int 0xb9400023\n"
	"        .int 0x885f7c04\n"
	"        .int 0x6b03009f\n"
	"        .int 0x54000061\n"
	"        .int 0x88057c02\n"
	"        .int 0x35ffff85\n"
	"        .int 0x1a9f17e0\n"
	"        .int 0x54000141\n"
	"        .int 0xd65f03c0\n"
	"        .int 0xb9400023\n"
	"        .int 0x885ffc04\n"
	"        .int 0x6b03009f\n"
	"        .int 0x54000061\n"
	"        .int 0x8805fc02\n"
	"        .int 0x35ffff85\n"
	"        .int 0x1a9f17e0\n"
	"        .int 0x54ffff00\n"
	"        .int 0xb9000024\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        cbnz    w3,  " LP "___atomic_compare_exchange_4_028\n"
	"        ldr w3, [x1]\n"
	"" LP "___atomic_compare_exchange_4_008:\n"
	"        ldxr    w4, [x0]\n"
	"        cmp w4, w3\n"
	"        b.ne     " LP "___atomic_compare_exchange_4_01c\n"
	"        stxr    w5, w2, [x0]\n"
	"        cbnz    w5,  " LP "___atomic_compare_exchange_4_008\n"
	"" LP "___atomic_compare_exchange_4_01c:\n"
	"        cset    w0, eq  // eq = none\n"
	"        b.ne     " LP "___atomic_compare_exchange_4_048\n"
	"" LP "___atomic_compare_exchange_4_024:\n"
	"        ret\n"
	"" LP "___atomic_compare_exchange_4_028:\n"
	"        ldr w3, [x1]\n"
	"" LP "___atomic_compare_exchange_4_02c:\n"
	"        ldaxr   w4, [x0]\n"
	"        cmp w4, w3\n"
	"        b.ne     " LP "___atomic_compare_exchange_4_040\n"
	"        stlxr   w5, w2, [x0]\n"
	"        cbnz    w5,  " LP "___atomic_compare_exchange_4_02c\n"
	"" LP "___atomic_compare_exchange_4_040:\n"
	"        cset    w0, eq  // eq = none\n"
	"        b.eq     " LP "___atomic_compare_exchange_4_024\n"
	"" LP "___atomic_compare_exchange_4_048:\n"
	"        str w4, [x1]\n"
	"        ret\n"
	SIZE(STR(_(__atomic_compare_exchange_4))));

#endif
__asm__(
	"        .global " STR(_(__atomic_compare_exchange_8)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_8)))
																												  "" STR(_(__atomic_compare_exchange_8)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x35000143\n"
	"        .int 0xf9400023\n"
	"        .int 0xc85f7c04\n"
	"        .int 0xeb03009f\n"
	"        .int 0x54000061\n"
	"        .int 0xc8057c02\n"
	"        .int 0x35ffff85\n"
	"        .int 0x1a9f17e0\n"
	"        .int 0x54000141\n"
	"        .int 0xd65f03c0\n"
	"        .int 0xf9400023\n"
	"        .int 0xc85ffc04\n"
	"        .int 0xeb03009f\n"
	"        .int 0x54000061\n"
	"        .int 0xc805fc02\n"
	"        .int 0x35ffff85\n"
	"        .int 0x1a9f17e0\n"
	"        .int 0x54ffff00\n"
	"        .int 0xf9000024\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        cbnz    w3,  " LP "___atomic_compare_exchange_8_028\n"
	"        ldr x3, [x1]\n"
	"" LP "___atomic_compare_exchange_8_008:\n"
	"        ldxr    x4, [x0]\n"
	"        cmp x4, x3\n"
	"        b.ne     " LP "___atomic_compare_exchange_8_01c\n"
	"        stxr    w5, x2, [x0]\n"
	"        cbnz    w5,  " LP "___atomic_compare_exchange_8_008\n"
	"" LP "___atomic_compare_exchange_8_01c:\n"
	"        cset    w0, eq  // eq = none\n"
	"        b.ne     " LP "___atomic_compare_exchange_8_048\n"
	"" LP "___atomic_compare_exchange_8_024:\n"
	"        ret\n"
	"" LP "___atomic_compare_exchange_8_028:\n"
	"        ldr x3, [x1]\n"
	"" LP "___atomic_compare_exchange_8_02c:\n"
	"        ldaxr   x4, [x0]\n"
	"        cmp x4, x3\n"
	"        b.ne     " LP "___atomic_compare_exchange_8_040\n"
	"        stlxr   w5, x2, [x0]\n"
	"        cbnz    w5,  " LP "___atomic_compare_exchange_8_02c\n"
	"" LP "___atomic_compare_exchange_8_040:\n"
	"        cset    w0, eq  // eq = none\n"
	"        b.eq     " LP "___atomic_compare_exchange_8_024\n"
	"" LP "___atomic_compare_exchange_8_048:\n"
	"        str x4, [x1]\n"
	"        ret\n"
	SIZE(STR(_(__atomic_compare_exchange_8))));

#endif
__asm__(
	"        .global " STR(_(__atomic_test_and_set_1)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_1)))
																										  "" STR(_(__atomic_test_and_set_1)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x52800022\n"
	"        .int 0x350000c1\n"
	"        .int 0x085f7c01\n"
	"        .int 0x08037c02\n"
	"        .int 0x35ffffc3\n"
	"        .int 0x12001c20\n"
	"        .int 0xd65f03c0\n"
	"        .int 0x085ffc01\n"
	"        .int 0x0803fc02\n"
	"        .int 0x35ffffc3\n"
	"        .int 0x12001c20\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        mov w2, #0x1                    // #1\n"
	"        cbnz    w1,  " LP "___atomic_test_and_set_1_01c\n"
	"" LP "___atomic_test_and_set_1_008:\n"
	"        ldxrb   w1, [x0]\n"
	"        stxrb   w3, w2, [x0]\n"
	"        cbnz    w3,  " LP "___atomic_test_and_set_1_008\n"
	"        and w0, w1, #0xff\n"
	"        ret\n"
	"" LP "___atomic_test_and_set_1_01c:\n"
	"        ldaxrb  w1, [x0]\n"
	"        stlxrb  w3, w2, [x0]\n"
	"        cbnz    w3,  " LP "___atomic_test_and_set_1_01c\n"
	"        and w0, w1, #0xff\n"
	"        ret\n"
	SIZE(STR(_(__atomic_test_and_set_1))));

#endif
__asm__(
	"        .global " STR(_(__atomic_test_and_set_2)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_2)))
																										  "" STR(_(__atomic_test_and_set_2)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x52800022\n"
	"        .int 0x350000c1\n"
	"        .int 0x085f7c01\n"
	"        .int 0x08037c02\n"
	"        .int 0x35ffffc3\n"
	"        .int 0x12001c20\n"
	"        .int 0xd65f03c0\n"
	"        .int 0x085ffc01\n"
	"        .int 0x0803fc02\n"
	"        .int 0x35ffffc3\n"
	"        .int 0x12001c20\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        mov w2, #0x1                    // #1\n"
	"        cbnz    w1,  " LP "___atomic_test_and_set_2_01c\n"
	"" LP "___atomic_test_and_set_2_008:\n"
	"        ldxrb   w1, [x0]\n"
	"        stxrb   w3, w2, [x0]\n"
	"        cbnz    w3,  " LP "___atomic_test_and_set_2_008\n"
	"        and w0, w1, #0xff\n"
	"        ret\n"
	"" LP "___atomic_test_and_set_2_01c:\n"
	"        ldaxrb  w1, [x0]\n"
	"        stlxrb  w3, w2, [x0]\n"
	"        cbnz    w3,  " LP "___atomic_test_and_set_2_01c\n"
	"        and w0, w1, #0xff\n"
	"        ret\n"
	SIZE(STR(_(__atomic_test_and_set_2))));

#endif
__asm__(
	"        .global " STR(_(__atomic_test_and_set_4)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_4)))
																										  "" STR(_(__atomic_test_and_set_4)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x52800022\n"
	"        .int 0x350000c1\n"
	"        .int 0x085f7c01\n"
	"        .int 0x08037c02\n"
	"        .int 0x35ffffc3\n"
	"        .int 0x12001c20\n"
	"        .int 0xd65f03c0\n"
	"        .int 0x085ffc01\n"
	"        .int 0x0803fc02\n"
	"        .int 0x35ffffc3\n"
	"        .int 0x12001c20\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        mov w2, #0x1                    // #1\n"
	"        cbnz    w1,  " LP "___atomic_test_and_set_4_01c\n"
	"" LP "___atomic_test_and_set_4_008:\n"
	"        ldxrb   w1, [x0]\n"
	"        stxrb   w3, w2, [x0]\n"
	"        cbnz    w3,  " LP "___atomic_test_and_set_4_008\n"
	"        and w0, w1, #0xff\n"
	"        ret\n"
	"" LP "___atomic_test_and_set_4_01c:\n"
	"        ldaxrb  w1, [x0]\n"
	"        stlxrb  w3, w2, [x0]\n"
	"        cbnz    w3,  " LP "___atomic_test_and_set_4_01c\n"
	"        and w0, w1, #0xff\n"
	"        ret\n"
	SIZE(STR(_(__atomic_test_and_set_4))));

#endif
__asm__(
	"        .global " STR(_(__atomic_test_and_set_8)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_8)))
																										  "" STR(_(__atomic_test_and_set_8)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x52800022\n"
	"        .int 0x350000c1\n"
	"        .int 0x085f7c01\n"
	"        .int 0x08037c02\n"
	"        .int 0x35ffffc3\n"
	"        .int 0x12001c20\n"
	"        .int 0xd65f03c0\n"
	"        .int 0x085ffc01\n"
	"        .int 0x0803fc02\n"
	"        .int 0x35ffffc3\n"
	"        .int 0x12001c20\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        mov w2, #0x1                    // #1\n"
	"        cbnz    w1,  " LP "___atomic_test_and_set_8_01c\n"
	"" LP "___atomic_test_and_set_8_008:\n"
	"        ldxrb   w1, [x0]\n"
	"        stxrb   w3, w2, [x0]\n"
	"        cbnz    w3,  " LP "___atomic_test_and_set_8_008\n"
	"        and w0, w1, #0xff\n"
	"        ret\n"
	"" LP "___atomic_test_and_set_8_01c:\n"
	"        ldaxrb  w1, [x0]\n"
	"        stlxrb  w3, w2, [x0]\n"
	"        cbnz    w3,  " LP "___atomic_test_and_set_8_01c\n"
	"        and w0, w1, #0xff\n"
	"        ret\n"
	SIZE(STR(_(__atomic_test_and_set_8))));

#endif
__asm__(
	"        .global " STR(_(atomic_thread_fence)) "\n"
												   TYPE(STR(_(atomic_thread_fence)))
																								  "" STR(_(atomic_thread_fence)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xd5033bbf\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        dmb ish\n"
	"        ret\n"
	SIZE(STR(_(atomic_thread_fence))));

#endif
__asm__(
	"        .global " STR(_(atomic_signal_fence)) "\n"
												   TYPE(STR(_(atomic_signal_fence)))
																								  "" STR(_(atomic_signal_fence)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        ret\n"
	SIZE(STR(_(atomic_signal_fence))));

#endif
__asm__(
	"        .global " STR(_(atomic_flag_test_and_set)) "\n"
														TYPE(STR(_(atomic_flag_test_and_set)))
																											"" STR(_(atomic_flag_test_and_set)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xaa0003e1\n"
	"        .int 0x52800022\n"
	"        .int 0x085ffc20\n"
	"        .int 0x0803fc22\n"
	"        .int 0x35ffffc3\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        mov x1, x0\n"
	"        mov w2, #0x1                    // #1\n"
	"" LP "_atomic_flag_test_and_set_008:\n"
	"        ldaxrb  w0, [x1]\n"
	"        stlxrb  w3, w2, [x1]\n"
	"        cbnz    w3,  " LP "_atomic_flag_test_and_set_008\n"
	"        ret\n"
	SIZE(STR(_(atomic_flag_test_and_set))));

#endif
__asm__(
	"        .global " STR(_(atomic_flag_test_and_set_explicit)) "\n"
																 TYPE(STR(_(atomic_flag_test_and_set_explicit)))
																															  "" STR(_(atomic_flag_test_and_set_explicit)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0xaa0003e1\n"
	"        .int 0x52800022\n"
	"        .int 0x085ffc20\n"
	"        .int 0x0803fc22\n"
	"        .int 0x35ffffc3\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        mov x1, x0\n"
	"        mov w2, #0x1                    // #1\n"
	"" LP "_atomic_flag_test_and_set_explicit_020:\n"
	"        ldaxrb  w0, [x1]\n"
	"        stlxrb  w3, w2, [x1]\n"
	"        cbnz    w3,  " LP "_atomic_flag_test_and_set_explicit_020\n"
	"        ret\n"
	SIZE(STR(_(atomic_flag_test_and_set_explicit))));

#endif
__asm__(
	"        .global " STR(_(atomic_flag_clear)) "\n"
												 TYPE(STR(_(atomic_flag_clear)))
																							  "" STR(_(atomic_flag_clear)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x089ffc1f\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        stlrb   wzr, [x0]\n"
	"        ret\n"
	SIZE(STR(_(atomic_flag_clear))));

#endif
__asm__(
	"        .global " STR(_(atomic_flag_clear_explicit)) "\n"
														  TYPE(STR(_(atomic_flag_clear_explicit)))
																												"" STR(_(atomic_flag_clear_explicit)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x089ffc1f\n"
	"        .int 0xd65f03c0\n");
#else
__asm__(
	"        stlrb   wzr, [x0]\n"
	"        ret\n"
	SIZE(STR(_(atomic_flag_clear_explicit))));

#endif
#endif

#if defined __riscv
__asm__(
	"        .text\n");

__asm__(
	"        .global " STR(_(__atomic_load_1)) "\n"
											   TYPE(STR(_(__atomic_load_1)))
																						  "" STR(_(__atomic_load_1)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x0330000f\n"
	"        .int 0x00054503\n"
	"        .int 0x0330000f\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        fence   rw,rw\n"
	"        lbu a0,0(a0)\n"
	"        fence   rw,rw\n"
	"        ret\n"
	SIZE(STR(_(__atomic_load_1))));

#endif
__asm__(
	"        .global " STR(_(__atomic_load_2)) "\n"
											   TYPE(STR(_(__atomic_load_2)))
																						  "" STR(_(__atomic_load_2)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x0330000f\n"
	"        .int 0x00055503\n"
	"        .int 0x0330000f\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        fence   rw,rw\n"
	"        lhu a0,0(a0)\n"
	"        fence   rw,rw\n"
	"        ret\n"
	SIZE(STR(_(__atomic_load_2))));

#endif
__asm__(
	"        .global " STR(_(__atomic_load_4)) "\n"
											   TYPE(STR(_(__atomic_load_4)))
																						  "" STR(_(__atomic_load_4)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x0330000f\n"
	"        .short 0x4108\n"
	"        .int 0x0230000f\n"
	"        .short 0x2501\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        fence   rw,rw\n"
	"        lw  a0,0(a0)\n"
	"        fence   r,rw\n"
	"        sext.w  a0,a0\n"
	"        ret\n"
	SIZE(STR(_(__atomic_load_4))));

#endif
__asm__(
	"        .global " STR(_(__atomic_load_8)) "\n"
											   TYPE(STR(_(__atomic_load_8)))
																						  "" STR(_(__atomic_load_8)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x0330000f\n"
	"        .short 0x6108\n"
	"        .int 0x0230000f\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        fence   rw,rw\n"
	"        ld  a0,0(a0)\n"
	"        fence   r,rw\n"
	"        ret\n"
	SIZE(STR(_(__atomic_load_8))));

#endif
__asm__(
	"        .global " STR(_(__atomic_store_1)) "\n"
												TYPE(STR(_(__atomic_store_1)))
																							"" STR(_(__atomic_store_1)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x0330000f\n"
	"        .int 0x00b50023\n"
	"        .int 0x0330000f\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        fence   rw,rw\n"
	"        sb  a1,0(a0)\n"
	"        fence   rw,rw\n"
	"        ret\n"
	SIZE(STR(_(__atomic_store_1))));

#endif
__asm__(
	"        .global " STR(_(__atomic_store_2)) "\n"
												TYPE(STR(_(__atomic_store_2)))
																							"" STR(_(__atomic_store_2)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x0330000f\n"
	"        .int 0x00b51023\n"
	"        .int 0x0330000f\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        fence   rw,rw\n"
	"        sh  a1,0(a0)\n"
	"        fence   rw,rw\n"
	"        ret\n"
	SIZE(STR(_(__atomic_store_2))));

#endif
__asm__(
	"        .global " STR(_(__atomic_store_4)) "\n"
												TYPE(STR(_(__atomic_store_4)))
																							"" STR(_(__atomic_store_4)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x0310000f\n"
	"        .short 0xc10c\n"
	"        .int 0x0330000f\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        fence   rw,w\n"
	"        sw  a1,0(a0)\n"
	"        fence   rw,rw\n"
	"        ret\n"
	SIZE(STR(_(__atomic_store_4))));

#endif
__asm__(
	"        .global " STR(_(__atomic_store_8)) "\n"
												TYPE(STR(_(__atomic_store_8)))
																							"" STR(_(__atomic_store_8)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x0310000f\n"
	"        .short 0xe10c\n"
	"        .int 0x0330000f\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        fence   rw,w\n"
	"        sd  a1,0(a0)\n"
	"        fence   rw,rw\n"
	"        ret\n"
	SIZE(STR(_(__atomic_store_8))));

#endif
__asm__(
	"        .global " STR(_(__atomic_compare_exchange_1)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_1)))
																												  "" STR(_(__atomic_compare_exchange_1)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x0005c683\n"
	"        .int 0x00357793\n"
	"        .int 0x0037979b\n"
	"        .int 0x0ff00713\n"
	"        .int 0x00f7173b\n"
	"        .int 0x00f698bb\n"
	"        .int 0x00f6163b\n"
	"        .short 0x9971\n"
	"        .int 0xfff74313\n"
	"        .int 0x00e8f8b3\n"
	"        .short 0x8e79\n"
	"        .int 0x1605282f\n"
	"        .int 0x00e87e33\n"
	"        .int 0x011e1a63\n"
	"        .int 0x00687e33\n"
	"        .int 0x00ce6e33\n"
	"        .int 0x1bc52e2f\n"
	"        .int 0xfe0e14e3\n"
	"        .int 0x40f8583b\n"
	"        .int 0x0188179b\n"
	"        .int 0x0186969b\n"
	"        .int 0x4187d79b\n"
	"        .int 0x4186d69b\n"
	"        .short 0x9f95\n"
	"        .int 0x0017b513\n"
	"        .short 0xc399\n"
	"        .int 0x01058023\n"
	"        .short 0x8905\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        lbu a3,0(a1)\n"
	"        andi    a5,a0,3\n"
	"        slliw   a5,a5,0x3\n"
	"        li  a4,255\n"
	"        sllw    a4,a4,a5\n"
	"        sllw    a7,a3,a5\n"
	"        sllw    a2,a2,a5\n"
	"        andi    a0,a0,-4\n"
	"        not t1,a4\n"
	"        and a7,a7,a4\n"
	"        and a2,a2,a4\n"
	"" LP "___atomic_compare_exchange_1_028:\n"
	"        lr.w.aqrl   a6,(a0)\n"
	"        and t3,a6,a4\n"
	"        bne t3,a7, " LP "___atomic_compare_exchange_1_044\n"
	"        and t3,a6,t1\n"
	"        or  t3,t3,a2\n"
	"        sc.w.rl t3,t3,(a0)\n"
	"        bnez    t3, " LP "___atomic_compare_exchange_1_028\n"
	"" LP "___atomic_compare_exchange_1_044:\n"
	"        sraw    a6,a6,a5\n"
	"        slliw   a5,a6,0x18\n"
	"        slliw   a3,a3,0x18\n"
	"        sraiw   a5,a5,0x18\n"
	"        sraiw   a3,a3,0x18\n"
	"        subw    a5,a5,a3\n"
	"        seqz    a0,a5\n"
	"        beqz    a5, " LP "___atomic_compare_exchange_1_064\n"
	"        sb  a6,0(a1)\n"
	"" LP "___atomic_compare_exchange_1_064:\n"
	"        andi    a0,a0,1\n"
	"        ret\n"
	SIZE(STR(_(__atomic_compare_exchange_1))));

#endif
__asm__(
	"        .global " STR(_(__atomic_compare_exchange_2)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_2)))
																												  "" STR(_(__atomic_compare_exchange_2)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x0005d683\n"
	"        .int 0x00357713\n"
	"        .short 0x67c1\n"
	"        .int 0x0037171b\n"
	"        .short 0x37fd\n"
	"        .int 0x00e797bb\n"
	"        .int 0x00e698bb\n"
	"        .int 0x00e6163b\n"
	"        .short 0x9971\n"
	"        .int 0xfff7c313\n"
	"        .int 0x00f8f8b3\n"
	"        .short 0x8e7d\n"
	"        .int 0x1605282f\n"
	"        .int 0x00f87e33\n"
	"        .int 0x011e1a63\n"
	"        .int 0x00687e33\n"
	"        .int 0x00ce6e33\n"
	"        .int 0x1bc52e2f\n"
	"        .int 0xfe0e14e3\n"
	"        .int 0x40e8583b\n"
	"        .int 0x0108179b\n"
	"        .int 0x0106969b\n"
	"        .int 0x4107d79b\n"
	"        .int 0x4106d69b\n"
	"        .short 0x9f95\n"
	"        .int 0x0017b513\n"
	"        .short 0xc399\n"
	"        .int 0x01059023\n"
	"        .short 0x8905\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        lhu a3,0(a1)\n"
	"        andi    a4,a0,3\n"
	"        lui a5,0x10\n"
	"        slliw   a4,a4,0x3\n"
	"        addiw   a5,a5,-1 # ffff <" LP "ASF16+0xfec8>\n"
	"        sllw    a5,a5,a4\n"
	"        sllw    a7,a3,a4\n"
	"        sllw    a2,a2,a4\n"
	"        andi    a0,a0,-4\n"
	"        not t1,a5\n"
	"        and a7,a7,a5\n"
	"        and a2,a2,a5\n"
	"" LP "___atomic_compare_exchange_2_028:\n"
	"        lr.w.aqrl   a6,(a0)\n"
	"        and t3,a6,a5\n"
	"        bne t3,a7, " LP "___atomic_compare_exchange_2_044\n"
	"        and t3,a6,t1\n"
	"        or  t3,t3,a2\n"
	"        sc.w.rl t3,t3,(a0)\n"
	"        bnez    t3, " LP "___atomic_compare_exchange_2_028\n"
	"" LP "___atomic_compare_exchange_2_044:\n"
	"        sraw    a6,a6,a4\n"
	"        slliw   a5,a6,0x10\n"
	"        slliw   a3,a3,0x10\n"
	"        sraiw   a5,a5,0x10\n"
	"        sraiw   a3,a3,0x10\n"
	"        subw    a5,a5,a3\n"
	"        seqz    a0,a5\n"
	"        beqz    a5, " LP "___atomic_compare_exchange_2_064\n"
	"        sh  a6,0(a1)\n"
	"" LP "___atomic_compare_exchange_2_064:\n"
	"        andi    a0,a0,1\n"
	"        ret\n"
	SIZE(STR(_(__atomic_compare_exchange_2))));

#endif
__asm__(
	"        .global " STR(_(__atomic_compare_exchange_4)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_4)))
																												  "" STR(_(__atomic_compare_exchange_4)) ":\n");
#ifdef __MCC__
__asm__(
	"        .short 0x419c\n"
	"        .int 0x1605272f\n"
	"        .int 0x00f71563\n"
	"        .int 0x1ac526af\n"
	"        .short 0xfaf5\n"
	"        .int 0x40f707bb\n"
	"        .int 0x0017b513\n"
	"        .short 0xc391\n"
	"        .short 0xc198\n"
	"        .short 0x8905\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        lw  a5,0(a1)\n"
	"" LP "___atomic_compare_exchange_4_002:\n"
	"        lr.w.aqrl   a4,(a0)\n"
	"        bne a4,a5, " LP "___atomic_compare_exchange_4_010\n"
	"        sc.w.rl a3,a2,(a0)\n"
	"        bnez    a3, " LP "___atomic_compare_exchange_4_002\n"
	"" LP "___atomic_compare_exchange_4_010:\n"
	"        subw    a5,a4,a5\n"
	"        seqz    a0,a5\n"
	"        beqz    a5, " LP "___atomic_compare_exchange_4_01c\n"
	"        sw  a4,0(a1)\n"
	"" LP "___atomic_compare_exchange_4_01c:\n"
	"        andi    a0,a0,1\n"
	"        ret\n"
	SIZE(STR(_(__atomic_compare_exchange_4))));

#endif
__asm__(
	"        .global " STR(_(__atomic_compare_exchange_8)) "\n"
														   TYPE(STR(_(__atomic_compare_exchange_8)))
																												  "" STR(_(__atomic_compare_exchange_8)) ":\n");
#ifdef __MCC__
__asm__(
	"        .short 0x619c\n"
	"        .int 0x1605372f\n"
	"        .int 0x00f71563\n"
	"        .int 0x1ac536af\n"
	"        .short 0xfaf5\n"
	"        .int 0x40f707b3\n"
	"        .int 0x0017b513\n"
	"        .short 0xc391\n"
	"        .short 0xe198\n"
	"        .short 0x8905\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        ld  a5,0(a1)\n"
	"" LP "___atomic_compare_exchange_8_002:\n"
	"        lr.d.aqrl   a4,(a0)\n"
	"        bne a4,a5, " LP "___atomic_compare_exchange_8_010\n"
	"        sc.d.rl a3,a2,(a0)\n"
	"        bnez    a3, " LP "___atomic_compare_exchange_8_002\n"
	"" LP "___atomic_compare_exchange_8_010:\n"
	"        sub a5,a4,a5\n"
	"        seqz    a0,a5\n"
	"        beqz    a5, " LP "___atomic_compare_exchange_8_01c\n"
	"        sd  a4,0(a1)\n"
	"" LP "___atomic_compare_exchange_8_01c:\n"
	"        andi    a0,a0,1\n"
	"        ret\n"
	SIZE(STR(_(__atomic_compare_exchange_8))));

#endif
__asm__(
	"        .global " STR(_(__atomic_test_and_set_1)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_1)))
																										  "" STR(_(__atomic_test_and_set_1)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x00357793\n"
	"        .int 0x0037979b\n"
	"        .short 0x4685\n"
	"        .short 0x9971\n"
	"        .int 0x00f696bb\n"
	"        .int 0x46d5272f\n"
	"        .int 0x00f7553b\n"
	"        .int 0x0ff57513\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        andi    a5,a0,3\n"
	"        slliw   a5,a5,0x3\n"
	"        li  a3,1\n"
	"        andi    a0,a0,-4\n"
	"        sllw    a3,a3,a5\n"
	"        amoor.w.aqrl    a4,a3,(a0)\n"
	"        srlw    a0,a4,a5\n"
	"        zext.b  a0,a0\n"
	"        ret\n"
	SIZE(STR(_(__atomic_test_and_set_1))));

#endif
__asm__(
	"        .global " STR(_(__atomic_test_and_set_2)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_2)))
																										  "" STR(_(__atomic_test_and_set_2)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x00357793\n"
	"        .int 0x0037979b\n"
	"        .short 0x4685\n"
	"        .short 0x9971\n"
	"        .int 0x00f696bb\n"
	"        .int 0x46d5272f\n"
	"        .int 0x00f7553b\n"
	"        .int 0x0ff57513\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        andi    a5,a0,3\n"
	"        slliw   a5,a5,0x3\n"
	"        li  a3,1\n"
	"        andi    a0,a0,-4\n"
	"        sllw    a3,a3,a5\n"
	"        amoor.w.aqrl    a4,a3,(a0)\n"
	"        srlw    a0,a4,a5\n"
	"        zext.b  a0,a0\n"
	"        ret\n"
	SIZE(STR(_(__atomic_test_and_set_2))));

#endif
__asm__(
	"        .global " STR(_(__atomic_test_and_set_4)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_4)))
																										  "" STR(_(__atomic_test_and_set_4)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x00357793\n"
	"        .int 0x0037979b\n"
	"        .short 0x4685\n"
	"        .short 0x9971\n"
	"        .int 0x00f696bb\n"
	"        .int 0x46d5272f\n"
	"        .int 0x00f7553b\n"
	"        .int 0x0ff57513\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        andi    a5,a0,3\n"
	"        slliw   a5,a5,0x3\n"
	"        li  a3,1\n"
	"        andi    a0,a0,-4\n"
	"        sllw    a3,a3,a5\n"
	"        amoor.w.aqrl    a4,a3,(a0)\n"
	"        srlw    a0,a4,a5\n"
	"        zext.b  a0,a0\n"
	"        ret\n"
	SIZE(STR(_(__atomic_test_and_set_4))));

#endif
__asm__(
	"        .global " STR(_(__atomic_test_and_set_8)) "\n"
													   TYPE(STR(_(__atomic_test_and_set_8)))
																										  "" STR(_(__atomic_test_and_set_8)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x00357793\n"
	"        .int 0x0037979b\n"
	"        .short 0x4685\n"
	"        .short 0x9971\n"
	"        .int 0x00f696bb\n"
	"        .int 0x46d5272f\n"
	"        .int 0x00f7553b\n"
	"        .int 0x0ff57513\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        andi    a5,a0,3\n"
	"        slliw   a5,a5,0x3\n"
	"        li  a3,1\n"
	"        andi    a0,a0,-4\n"
	"        sllw    a3,a3,a5\n"
	"        amoor.w.aqrl    a4,a3,(a0)\n"
	"        srlw    a0,a4,a5\n"
	"        zext.b  a0,a0\n"
	"        ret\n"
	SIZE(STR(_(__atomic_test_and_set_8))));

#endif
__asm__(
	"        .global " STR(_(atomic_thread_fence)) "\n"
												   TYPE(STR(_(atomic_thread_fence)))
																								  "" STR(_(atomic_thread_fence)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x0330000f\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        fence   rw,rw\n"
	"        ret\n"
	SIZE(STR(_(atomic_thread_fence))));

#endif
__asm__(
	"        .global " STR(_(atomic_signal_fence)) "\n"
												   TYPE(STR(_(atomic_signal_fence)))
																								  "" STR(_(atomic_signal_fence)) ":\n");
#ifdef __MCC__
__asm__(
	"        .short 0x8082\n");
#else
__asm__(
	"        ret\n"
	SIZE(STR(_(atomic_signal_fence))));

#endif
__asm__(
	"        .global " STR(_(atomic_flag_test_and_set)) "\n"
														TYPE(STR(_(atomic_flag_test_and_set)))
																											"" STR(_(atomic_flag_test_and_set)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x00357793\n"
	"        .int 0x0037979b\n"
	"        .short 0x4685\n"
	"        .short 0x9971\n"
	"        .int 0x00f696bb\n"
	"        .int 0x46d5272f\n"
	"        .int 0x00f7553b\n"
	"        .int 0x0ff57513\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        andi    a5,a0,3\n"
	"        slliw   a5,a5,0x3\n"
	"        li  a3,1\n"
	"        andi    a0,a0,-4\n"
	"        sllw    a3,a3,a5\n"
	"        amoor.w.aqrl    a4,a3,(a0)\n"
	"        srlw    a0,a4,a5\n"
	"        zext.b  a0,a0\n"
	"        ret\n"
	SIZE(STR(_(atomic_flag_test_and_set))));

#endif
__asm__(
	"        .global " STR(_(atomic_flag_test_and_set_explicit)) "\n"
																 TYPE(STR(_(atomic_flag_test_and_set_explicit)))
																															  "" STR(_(atomic_flag_test_and_set_explicit)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x00357793\n"
	"        .int 0x0037979b\n"
	"        .short 0x4685\n"
	"        .short 0x9971\n"
	"        .int 0x00f696bb\n"
	"        .int 0x46d5272f\n"
	"        .int 0x00f7553b\n"
	"        .int 0x0ff57513\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        andi    a5,a0,3\n"
	"        slliw   a5,a5,0x3\n"
	"        li  a3,1\n"
	"        andi    a0,a0,-4\n"
	"        sllw    a3,a3,a5\n"
	"        amoor.w.aqrl    a4,a3,(a0)\n"
	"        srlw    a0,a4,a5\n"
	"        zext.b  a0,a0\n"
	"        ret\n"
	SIZE(STR(_(atomic_flag_test_and_set_explicit))));

#endif
__asm__(
	"        .global " STR(_(atomic_flag_clear)) "\n"
												 TYPE(STR(_(atomic_flag_clear)))
																							  "" STR(_(atomic_flag_clear)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x0330000f\n"
	"        .int 0x00050023\n"
	"        .int 0x0330000f\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        fence   rw,rw\n"
	"        sb  zero,0(a0)\n"
	"        fence   rw,rw\n"
	"        ret\n"
	SIZE(STR(_(atomic_flag_clear))));

#endif
__asm__(
	"        .global " STR(_(atomic_flag_clear_explicit)) "\n"
														  TYPE(STR(_(atomic_flag_clear_explicit)))
																												"" STR(_(atomic_flag_clear_explicit)) ":\n");
#ifdef __MCC__
__asm__(
	"        .int 0x0330000f\n"
	"        .int 0x00050023\n"
	"        .int 0x0330000f\n"
	"        .short 0x8082\n");
#else
__asm__(
	"        fence   rw,rw\n"
	"        sb  zero,0(a0)\n"
	"        fence   rw,rw\n"
	"        ret\n"
	SIZE(STR(_(atomic_flag_clear_explicit))));

#endif
#endif

typedef __SIZE_TYPE__ __mcc_usize;

static volatile unsigned char __mcc_atomic_locks[64];

static volatile unsigned char *__mcc_atomic_lock_for(const volatile void *p) {
	return &__mcc_atomic_locks[((__mcc_usize)p >> 4) & 63];
}

static void __mcc_atomic_lock(const volatile void *p) {
	volatile unsigned char *l = __mcc_atomic_lock_for(p);
	while (__atomic_exchange_n(l, (unsigned char)1, __ATOMIC_ACQUIRE))
		;
}

static void __mcc_atomic_unlock(const volatile void *p) {
	__atomic_store_n(__mcc_atomic_lock_for(p), (unsigned char)0, __ATOMIC_RELEASE);
}

static void __mcc_byte_copy(volatile void *dst, const volatile void *src, __mcc_usize n) {
	volatile unsigned char *d = dst;
	const volatile unsigned char *s = src;
	while (n--)
		*d++ = *s++;
}

void __atomic_load(__mcc_usize size, const volatile void *mem, void *ret, int order) {
	(void)order;
	__mcc_atomic_lock(mem);
	__mcc_byte_copy(ret, mem, size);
	__mcc_atomic_unlock(mem);
}

void __atomic_store(__mcc_usize size, volatile void *mem, void *val, int order) {
	(void)order;
	__mcc_atomic_lock(mem);
	__mcc_byte_copy(mem, val, size);
	__mcc_atomic_unlock(mem);
}

void __atomic_exchange(__mcc_usize size, volatile void *mem, void *val,
					   void *ret, int order) {
	(void)order;
	__mcc_atomic_lock(mem);
	__mcc_byte_copy(ret, mem, size);
	__mcc_byte_copy(mem, val, size);
	__mcc_atomic_unlock(mem);
}

int __atomic_compare_exchange(__mcc_usize size, volatile void *mem,
							  void *expected, void *desired,
							  int success, int failure) {
	int ok = 1;
	__mcc_usize i;
	const volatile unsigned char *m = mem;
	const unsigned char *e = expected;
	(void)success;
	(void)failure;
	__mcc_atomic_lock(mem);
	for (i = 0; i < size; i++)
		if (m[i] != e[i]) {
			ok = 0;
			break;
		}
	if (ok)
		__mcc_byte_copy(mem, desired, size);
	else
		__mcc_byte_copy(expected, mem, size);
	__mcc_atomic_unlock(mem);
	return ok;
}

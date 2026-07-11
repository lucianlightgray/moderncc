/* mccasan.c — native AddressSanitizer shadow runtime for -fasan-shadow (x86_64).
 *
 * ASan 1/8 shadow scheme: shadow(addr) = (addr>>3) + 0x7fff8000. The compiler
 * (x86_64-gen.c gen_asan_shadow_check) emits an inline shadow probe before every
 * pointer dereference; a poisoned slot traps (UD2), caught here by a SIGILL
 * handler that prints an AddressSanitizer-style diagnostic.
 *
 * This runtime maps the full ASan Linux/x86_64 shadow layout (LowShadow +
 * HighShadow, sparse via MAP_NORESERVE) so it works with real allocations at
 * any address (heap/PIE), and intercepts malloc/free/calloc/realloc to redzone
 * allocations and poison freed regions -> detects heap out-of-bounds AND
 * use-after-free, with no false positives on ordinary stack/global/heap access.
 *
 * Global redzones: the compiler emits a {addr,size} table into the
 * __asan_globals section and asan_register_globals() poisons each global's right
 * redzone at startup (required for bss globals with no compile-time bytes).
 *
 * Stack redzones: the compiler emits a per-function {rbp-offset,size} table into
 * .asan_lstack and calls __asan_stack_enter/leave (passing rbp) in the prologue/
 * epilogue; enter poisons each local's right redzone then unpoisons the objects
 * (redzones-first => no false positives on slot reuse), leave clears the span.
 *
 * The SIGILL handler classifies the fault from the shadow-poison byte left in
 * rax at the ud2 (heap-buffer-overflow / heap-use-after-free / stack- and
 * global-buffer-overflow / partial). Remaining follow-ups: the faulting address
 * + a shadow-byte dump (needs the address preserved to the trap) and
 * arm64/riscv64 stack instrumentation. Complements the shipped bcheck-based
 * -fsanitize=address (self-contained, clang-verified).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/mman.h>
#include <signal.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <ucontext.h>
#define OFF 0x7fff8000UL
#define RZ 16
#define GRZ 0xf9
struct asan_global { void *addr; size_t size; };
extern struct asan_global __start___asan_globals[] __attribute__((weak));
extern struct asan_global __stop___asan_globals[] __attribute__((weak));
static unsigned char *shadow(void *a){ return (unsigned char*)(((uintptr_t)a>>3)+OFF); }
static void set_sh(void*a,size_t n,unsigned char v){ uintptr_t p=(uintptr_t)a; for(size_t i=0;i<n;i+=8) *shadow((void*)(p+i))=v; }
static void unpoison(void*a,long n){ uintptr_t p=(uintptr_t)a; size_t full=((size_t)n/8)*8; set_sh(a,full,0); if((size_t)n%8) *shadow((void*)(p+full))=(unsigned char)((size_t)n%8); }
static void wstr(const char*s){ long n=0; while(s[n])n++; (void)!write(2,s,(size_t)n); }
static void whexn(uintptr_t v,int nyb,int nl){ char b[19]; int i; for(i=0;i<nyb;i++){int d=(int)((v>>((nyb-1-i)*4))&0xf); b[i]=(char)(d<10?'0'+d:'a'+d-10);} if(nl)b[nyb]='\n'; (void)!write(2,b,(size_t)(nyb+(nl?1:0))); }
static void whex(uintptr_t v){ wstr("0x"); whexn(v,16,1); }
static const char *asan_class(int sh){
    switch(sh&0xff){
    case 0xfa: return "heap-buffer-overflow";
    case 0xfd: return "heap-use-after-free";
    case 0xf2: return "stack-buffer-overflow";
    case GRZ:  return "global-buffer-overflow";
    default:   return (sh>=1&&sh<=7) ? "buffer-overflow" : "bad memory access";
    }
}
static void on_sigill(int sig,siginfo_t*si,void*ucv){
    ucontext_t *uc=(ucontext_t*)ucv; (void)sig;
    long sh = uc ? (long)uc->uc_mcontext.gregs[REG_RAX] : 0;
    long off = uc ? (long)uc->uc_mcontext.gregs[REG_RDX] : 0;
    wstr("=================================================================\n");
    wstr("==ERROR: AddressSanitizer: "); wstr(asan_class((int)sh));
    wstr(" (mcc native shadow)\n    pc "); whex((uintptr_t)si->si_addr);
    wstr("    shadow byte 0x"); whexn((uintptr_t)(sh&0xff),2,0);
    wstr("  granule offset "); whexn((uintptr_t)(off&0xff),2,1);
    _exit(1);
}
void __asan_stack_enter(void *tab,void *fpv){
    size_t *q; size_t fp=(size_t)fpv;
    for(q=tab; q[0]; q+=2){ char*obj=(char*)(fp+q[0]); size_t sz=q[1]; set_sh(obj+((sz+7)&~(size_t)7),RZ,0xf2); }
    for(q=tab; q[0]; q+=2){ unpoison((char*)(fp+q[0]),(long)q[1]); }
}
void __asan_stack_leave(void *tab,void *fpv){
    size_t *q; size_t fp=(size_t)fpv;
    for(q=tab; q[0]; q+=2){ char*obj=(char*)(fp+q[0]); size_t sz=q[1]; set_sh(obj,((sz+7)&~(size_t)7)+RZ,0); }
}
static void asan_register_globals(void){
    struct asan_global *g;
    for(g=__start___asan_globals; g<__stop___asan_globals; g++){
        size_t rounded=(g->size+7)&~(size_t)7;
        unpoison(g->addr,(long)g->size);
        set_sh((char*)g->addr+rounded,RZ,GRZ);
    }
}
__attribute__((constructor)) static void asan_init(void){
    mmap((void*)0x7fff8000UL,(size_t)(0x8fff7000UL-0x7fff8000UL),PROT_READ|PROT_WRITE,MAP_FIXED|MAP_NORESERVE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    mmap((void*)0x2008fff7000UL,(size_t)(0x10007fff8000UL-0x2008fff7000UL),PROT_READ|PROT_WRITE,MAP_FIXED|MAP_NORESERVE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    struct sigaction sa; for(size_t i=0;i<sizeof sa;i++) ((char*)&sa)[i]=0;
    sa.sa_sigaction=on_sigill; sa.sa_flags=SA_SIGINFO; sigaction(SIGILL,&sa,0);
    asan_register_globals();
}
void *malloc(size_t n){
    size_t usable=(n+7)&~(size_t)7; size_t tot=(RZ+usable+RZ+4095)&~(size_t)4095;
    char *base=mmap(0,tot,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(base==(void*)-1) return 0;
    char *user=base+RZ; set_sh(base,tot,0xfa); unpoison(user,(long)n);
    ((size_t*)base)[0]=tot; ((size_t*)base)[1]=usable; return user;
}
void free(void*p){ if(!p)return; char*base=(char*)p-RZ; size_t tot=((size_t*)base)[0],usable=((size_t*)base)[1]; set_sh(p,usable,0xfd); munmap(base,tot); }
void *calloc(size_t a,size_t b){ char*p=malloc(a*b); if(p)for(size_t i=0;i<a*b;i++)p[i]=0; return p; }
void *realloc(void*p,size_t n){ char*q=malloc(n); if(q&&p){size_t u=((size_t*)((char*)p-RZ))[1]; for(size_t i=0;i<(u<n?u:n);i++)q[i]=((char*)p)[i]; free(p);} return q; }

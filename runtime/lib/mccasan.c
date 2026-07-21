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
#define PW ((int)(2*sizeof(uintptr_t)))
static void whex(uintptr_t v){ wstr("0x"); whexn(v,PW,1); }
static void whexa(uintptr_t v){ wstr("0x"); whexn(v,PW,0); }
static void wdec(uintptr_t v){ char b[24]; int i=24; if(!v){ (void)!write(2,"0",1); return; } while(v){ b[--i]=(char)('0'+(int)(v%10)); v/=10; } (void)!write(2,b+i,(size_t)(24-i)); }
static const char *asan_class(int sh){
    switch(sh&0xff){
    case 0xfa: return "heap-buffer-overflow";
    case 0xfd: return "heap-use-after-free";
    case 0xf2: return "stack-buffer-overflow";
    case GRZ:  return "global-buffer-overflow";
    default:   return (sh>=1&&sh<=7) ? "buffer-overflow" : "bad memory access";
    }
}
/* Walk the shadow outward from the faulting granule to find the nearest run of
   addressable bytes (shadow 0 or a 1..7 partial) that is bounded by poison (a
   redzone) on its far side: that run is a K-byte region, and the fault's offset
   from the nearer edge gives the gcc/clang-style locator line (right/left/
   inside). Walks are bounded so a wild address can't loop; a run that reaches
   the bound without hitting a redzone is treated as unbounded (not a region)
   so untouched shadow-0 memory and freed blocks fall back to no locator. */
static int asan_locate(uintptr_t addr,uintptr_t*rbeg,uintptr_t*rend,uintptr_t*roff,int*rdir){
    uintptr_t g=addr&~(uintptr_t)7; const int MAXG=1<<16;
    int fd=0,fu=0; uintptr_t dg=0; int dv=0; uintptr_t ug=0;
    for(int i=0;i<MAXG;i++){ uintptr_t gg=g-(uintptr_t)i*8; unsigned v=*shadow((void*)gg); if(v==0||(v>=1&&v<=7)){ dg=gg; dv=(int)v; fd=1; break; } if(gg<8) break; }
    for(int i=0;i<MAXG;i++){ uintptr_t gg=g+(uintptr_t)i*8; unsigned v=*shadow((void*)gg); if(v==0||(v>=1&&v<=7)){ ug=gg; fu=1; break; } }
    uintptr_t begL=0,endL=0,begR=0,endR=0;
    if(fd){ endL=dg+(uintptr_t)(dv?dv:8); uintptr_t b=dg; int i; for(i=0;i<MAXG&&b>=8;i++){ if(*shadow((void*)(b-8))==0) b-=8; else break; } begL=b; if(b<8||i>=MAXG) fd=0; }
    if(fu){ begR=ug; uintptr_t e=ug; int i,bnd=0; for(i=0;i<MAXG;i++){ unsigned v=*shadow((void*)e); if(v==0){ e+=8; continue; } if(v<=7) e+=v; bnd=1; break; } endR=e; if(!bnd) fu=0; }
    if(fd && addr<endL){ *rbeg=begL; *rend=endL; *roff=addr-begL; *rdir=2; return 1; }
    uintptr_t db=fd?(addr-endL):(uintptr_t)-1, da=fu?(begR-addr):(uintptr_t)-1;
    if(!fd&&!fu) return 0;
    if(db<=da){ *rbeg=begL; *rend=endL; *roff=db; *rdir=0; }
    else { *rbeg=begR; *rend=endR; *roff=da; *rdir=1; }
    return 1;
}
static void on_sigill(int sig,siginfo_t*si,void*ucv){
    ucontext_t *uc=(ucontext_t*)ucv; (void)sig;
#if defined(__aarch64__)
    long sh = uc ? (long)uc->uc_mcontext.regs[17] : 0;
    long off = uc ? (long)uc->uc_mcontext.regs[16] : 0;
    uintptr_t addr = uc ? (uintptr_t)uc->uc_mcontext.regs[15] : 0;
#elif defined(__riscv)
    long sh = uc ? (long)uc->uc_mcontext.__gregs[5] : 0;
    long off = uc ? (long)uc->uc_mcontext.__gregs[6] : 0;
    uintptr_t addr = uc ? (uintptr_t)uc->uc_mcontext.__gregs[7] : 0;
#elif defined(__i386__)
    long sh = uc ? (long)uc->uc_mcontext.gregs[REG_EAX] : 0;
    long off = uc ? (long)uc->uc_mcontext.gregs[REG_EDX] : 0;
    uintptr_t addr = uc ? (uintptr_t)(unsigned)uc->uc_mcontext.gregs[REG_ECX] : 0;
#elif defined(__arm__)
    long sh = uc ? (long)uc->uc_mcontext.arm_r0 : 0;
    long off = uc ? (long)uc->uc_mcontext.arm_r1 : 0;
    uintptr_t addr = uc ? (uintptr_t)uc->uc_mcontext.arm_r2 : 0;
#else
    long sh = uc ? (long)uc->uc_mcontext.gregs[REG_RAX] : 0;
    long off = uc ? (long)uc->uc_mcontext.gregs[REG_RDX] : 0;
    uintptr_t addr = uc ? (uintptr_t)uc->uc_mcontext.gregs[REG_RCX] : 0;
#endif
    wstr("=================================================================\n");
    wstr("==ERROR: AddressSanitizer: "); wstr(asan_class((int)sh));
    wstr(" (mcc native shadow)\n");
    if(addr){
        /* granule offset = (addr&7) + size-1  ->  size = off - (addr&7) + 1 */
        long asz = off - (long)(addr&7) + 1;
        unsigned char *s = shadow((void*)addr);
        wstr("    at faulting address "); whex(addr);
        wstr("    access size "); wdec((uintptr_t)(asz>0?asz:0)); wstr("\n");
        { uintptr_t rb,re,ro; int rd;
          if(asan_locate(addr,&rb,&re,&ro,&rd)){
            wstr("    "); whexa(addr); wstr(" is located "); wdec(ro);
            wstr(rd==0?" bytes to the right of a ":rd==1?" bytes to the left of a ":" bytes inside a ");
            wdec(re-rb); wstr("-byte region ["); whexa(rb); wstr(", "); whexa(re); wstr(")\n");
          } }
        wstr("  shadow bytes around 0x"); whexn((uintptr_t)s,PW,1);
        wstr("   ");
        for(int c=-8;c<8;c++){ if(c==0)wstr("["); whexn((uintptr_t)(s[c]&0xff),2,0); if(c==0)wstr("]"); else wstr(" "); }
        wstr("\n");
    }
    wstr("    pc "); whex((uintptr_t)si->si_addr);
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
#if defined(__aarch64__)
    /* arm64/Linux (top-down, 48-bit VA): shadow((a>>3)+OFF) of the entire 48-bit
       address space lands in [OFF, 2^45+OFF) ~ [2GB, 32TB), below where PIE/heap/
       stack (top-down) live, so one sparse NORESERVE region covers all of it.
       (Robustness for 39-bit VA / bottom-up mmap is a follow-up.) The trap is a
       brk -> SIGTRAP, and w17/w16 (shadow/granule) map to regs[17]/regs[16]. */
    mmap((void*)0x7fff8000UL,(size_t)(0x210000000000UL-0x7fff8000UL),PROT_READ|PROT_WRITE,MAP_FIXED|MAP_NORESERVE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
#elif defined(__riscv)
    /* riscv64/Linux (Sv48-class user VA): qemu-riscv64 user mode places libc/heap/
       stack/mmap near the top of a 47-bit space (~0x7effffffffff), so shadow
       ((a>>3)+OFF) of the accessible user range lands in [OFF, 2^44+OFF), the same
       LowShadow+HighShadow span the x86_64 ASan layout reserves; one sparse
       NORESERVE region covers it (the low 2GB..16TB guest window is otherwise
       empty). (Sv39-only / bottom-up-mmap tightening is a follow-up.) The trap is
       an ebreak -> SIGTRAP, and t0/t1/t2 (x5/x6/x7 = shadow/granule/faulting-addr)
       map to __gregs[5]/[6]/[7]. */
    mmap((void*)0x7fff8000UL,(size_t)(0x10007fff8000UL-0x7fff8000UL),PROT_READ|PROT_WRITE,MAP_FIXED|MAP_NORESERVE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
#elif defined(__i386__) || defined(__arm__)
    /* i386/arm Linux (32-bit VA): shadow((a>>3)+OFF) of the whole 4GB space
       [0,2^32) lands in [OFF, 2^29+OFF) = [0x7fff8000, 0x9fff8000), a single 512MB
       window at ~2GB that qemu-i386/qemu-arm leaves empty (program/heap sit low,
       stack near the 3-4GB top). One sparse NORESERVE region covers all of heap/
       stack/global. The trap is ud2/udf -> SIGILL; on i386 eax/edx/ecx and on arm
       r0/r1/r2 (shadow/granule/faulting-addr) map to gregs[REG_EAX]/[REG_EDX]/
       [REG_ECX] and uc_mcontext.arm_r0/arm_r1/arm_r2 respectively. */
    mmap((void*)0x7fff8000UL,(size_t)(0x9fff8000UL-0x7fff8000UL),PROT_READ|PROT_WRITE,MAP_FIXED|MAP_NORESERVE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
#else
    mmap((void*)0x7fff8000UL,(size_t)(0x10007fff8000UL-0x7fff8000UL),PROT_READ|PROT_WRITE,MAP_FIXED|MAP_NORESERVE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
#endif
    struct sigaction sa; for(size_t i=0;i<sizeof sa;i++) ((char*)&sa)[i]=0;
    sa.sa_sigaction=on_sigill; sa.sa_flags=SA_SIGINFO;
#if defined(__aarch64__) || defined(__riscv)
    sigaction(SIGTRAP,&sa,0);
#else
    sigaction(SIGILL,&sa,0);
#endif
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

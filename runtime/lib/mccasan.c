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
 * Remaining follow-ups: stack/global redzones (compiler-side frame layout),
 * richer __asan_report_* diagnostics, and CMake auto-link. Complements the
 * shipped bcheck-based -fsanitize=address (self-contained, clang-verified).
 */
#include <sys/mman.h>
#include <signal.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#define OFF 0x7fff8000UL
#define RZ 16
static unsigned char *shadow(void *a){ return (unsigned char*)(((uintptr_t)a>>3)+OFF); }
static void wstr(const char*s){ long n=0; while(s[n])n++; (void)!write(2,s,(size_t)n); }
static void whex(uintptr_t v){ char b[19]; b[0]='0';b[1]='x'; for(int i=0;i<16;i++){int d=(int)((v>>((15-i)*4))&0xf); b[2+i]=(char)(d<10?'0'+d:'a'+d-10);} b[18]='\n'; (void)!write(2,b,19); }
static void on_sigill(int sig,siginfo_t*si,void*uc){ (void)sig;(void)uc; wstr("AddressSanitizer: bad memory access (mcc native shadow) at pc "); whex((uintptr_t)si->si_addr); _exit(1); }
__attribute__((constructor)) static void asan_init(void){
    mmap((void*)0x7fff8000UL,(size_t)(0x8fff7000UL-0x7fff8000UL),PROT_READ|PROT_WRITE,MAP_FIXED|MAP_NORESERVE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    mmap((void*)0x2008fff7000UL,(size_t)(0x10007fff8000UL-0x2008fff7000UL),PROT_READ|PROT_WRITE,MAP_FIXED|MAP_NORESERVE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    struct sigaction sa; for(size_t i=0;i<sizeof sa;i++) ((char*)&sa)[i]=0;
    sa.sa_sigaction=on_sigill; sa.sa_flags=SA_SIGINFO; sigaction(SIGILL,&sa,0);
}
static void set_sh(void*a,size_t n,unsigned char v){ uintptr_t p=(uintptr_t)a; for(size_t i=0;i<n;i+=8) *shadow((void*)(p+i))=v; }
static void unpoison(void*a,long n){ uintptr_t p=(uintptr_t)a; size_t full=((size_t)n/8)*8; set_sh(a,full,0); if((size_t)n%8) *shadow((void*)(p+full))=(unsigned char)((size_t)n%8); }
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

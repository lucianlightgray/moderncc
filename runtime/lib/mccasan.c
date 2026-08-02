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
#define SH_LO 0x7fff8000UL
#if defined(__aarch64__)
#define SH_HI 0x210000000000UL
#elif defined(__i386__) || defined(__arm__)
#define SH_HI 0x9fff8000UL
#else
#define SH_HI 0x10007fff8000UL
#endif
#define RZ 16
#define GRZ 0xf9
struct asan_global { void *addr; size_t size; };
extern struct asan_global __start___asan_globals[] __attribute__((weak));
extern struct asan_global __stop___asan_globals[] __attribute__((weak));
static unsigned char *shadow(void *a){ return (unsigned char*)(((uintptr_t)a>>3)+OFF); }
static int sh_mapped(const unsigned char *s,long lo,long hi){
    uintptr_t p=(uintptr_t)s;
    if(p<SH_LO||p>=SH_HI) return 0;
    if(lo<0&&p<SH_LO+(uintptr_t)(-lo)) return 0;
    if(hi>0&&p+(uintptr_t)hi>=SH_HI) return 0;
    return 1;
}
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
static int asan_locate(uintptr_t addr,uintptr_t*rbeg,uintptr_t*rend,uintptr_t*roff,int*rdir){
    uintptr_t g=addr&~(uintptr_t)7; const int MAXG=1<<16;
    int fd=0,fu=0; uintptr_t dg=0; int dv=0; uintptr_t ug=0;
    for(int i=0;i<MAXG;i++){ uintptr_t gg=g-(uintptr_t)i*8; unsigned v=*shadow((void*)gg); if(v==0||(v>=1&&v<=7)){ dg=gg; dv=(int)v; fd=1; break; } if(gg<8) break; }
    for(int i=0;i<MAXG;i++){ uintptr_t gg=g+(uintptr_t)i*8; unsigned v=*shadow((void*)gg); if(v==0||(v>=1&&v<=7)){ ug=gg; fu=1; break; } }
    uintptr_t begL=0,endL=0,begR=0,endR=0;
    if(fd){ endL=dg+(uintptr_t)(dv?dv:8); uintptr_t b=dg; int i; for(i=0;i<MAXG&&b>=8;i++){ if(*shadow((void*)(b-8))==0) b-=8; else break; } begL=b; if(b<8||i>=MAXG) fd=0; }
    if(fu){ begR=ug; uintptr_t e=ug; int i,bnd=0; for(i=0;i<MAXG;i++){ unsigned v=*shadow((void*)e); if(v==0){ e+=8; continue; } if(v<=7) e+=v; bnd=1; break; } endR=e; if(!bnd) fu=0; }
    if(fd && addr<endL){ *rbeg=begL; *rend=endL; *roff=addr-begL; *rdir=2; return 1; }
    if(fu && begR<addr) fu=0;
    if(fu && begR-addr>(uintptr_t)MAXG*8) fu=0;
    if(fd && addr<endL) fd=0;
    if(fd && addr-endL>(uintptr_t)MAXG*8) fd=0;
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
    int atk = (off>>6)&1, atw = (off>>7)&1;
    off &= 0x3f;
    wstr("=================================================================\n");
    wstr("==ERROR: AddressSanitizer: "); wstr(asan_class((int)sh));
    wstr(" (mcc native shadow)\n");
    if(addr){
        long asz = off - (long)(addr&7) + 1;
        unsigned char *s = shadow((void*)addr);
        int sok = sh_mapped(s,-8,8);
        wstr("    at faulting address "); whex(addr);
        if(atk){ wstr("    "); wstr(atw?"WRITE":"READ"); wstr(" of size "); wdec((uintptr_t)(asz>0?asz:0)); wstr("\n"); }
        else { wstr("    access size "); wdec((uintptr_t)(asz>0?asz:0)); wstr("\n"); }
        { uintptr_t rb,re,ro; int rd;
          if(asan_locate(addr,&rb,&re,&ro,&rd)){
            wstr("    "); whexa(addr); wstr(" is located "); wdec(ro);
            wstr(rd==0?" bytes to the right of a ":rd==1?" bytes to the left of a ":" bytes inside a ");
            wdec(re-rb); wstr("-byte region ["); whexa(rb); wstr(", "); whexa(re); wstr(")\n");
          } }
        if(sok){
            wstr("  shadow bytes around 0x"); whexn((uintptr_t)s,PW,1);
            wstr("   ");
            for(int c=-8;c<8;c++){ if(c==0)wstr("["); whexn((uintptr_t)(s[c]&0xff),2,0); if(c==0)wstr("]"); else wstr(" "); }
            wstr("\n");
        } else {
            wstr("  shadow address 0x"); whexn((uintptr_t)s,PW,0);
            wstr(" is outside the mapped shadow region ["); whexn(SH_LO,PW,0);
            wstr(", "); whexn(SH_HI,PW,0); wstr("); dump suppressed\n");
        }
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
    mmap((void*)SH_LO,(size_t)(SH_HI-SH_LO),PROT_READ|PROT_WRITE,MAP_FIXED|MAP_NORESERVE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
#elif defined(__riscv)
    mmap((void*)SH_LO,(size_t)(SH_HI-SH_LO),PROT_READ|PROT_WRITE,MAP_FIXED|MAP_NORESERVE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
#elif defined(__i386__) || defined(__arm__)
    mmap((void*)SH_LO,(size_t)(SH_HI-SH_LO),PROT_READ|PROT_WRITE,MAP_FIXED|MAP_NORESERVE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
#else
    mmap((void*)SH_LO,(size_t)(SH_HI-SH_LO),PROT_READ|PROT_WRITE,MAP_FIXED|MAP_NORESERVE|MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
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

/* mccasan.c — native AddressSanitizer shadow runtime for -fasan-shadow (x86_64).
 *
 * Implements the ASan 1/8 shadow scheme: shadow(addr) = (addr>>3) + 0x7fff8000.
 * The compiler (x86_64-gen.c gen_asan_shadow_check) emits an inline shadow probe
 * before every pointer dereference; this runtime maps the shadow and poisons
 * heap redzones + freed regions so out-of-bounds and use-after-free trap (BRK/UD2).
 *
 * First increment: a fixed-address bump pool keeps allocations in a shadow range
 * this runtime maps precisely (validated end-to-end under -no-pie). Follow-ups:
 * full 16TB sparse shadow map, stack/global redzones, __asan_report_* diagnostics,
 * and CMake auto-link. This is the native-shadow alternative to the shipped,
 * clang-verified bcheck-based -fsanitize=address (which already detects OOB+UAF).
 */
#include <sys/mman.h>
#include <stdint.h>
#include <stddef.h>
#define OFF 0x7fff8000UL
#define RZ 16
#define POOL_ADDR 0x100000000UL
#define POOL_SIZE 0x4000000UL
static unsigned char *shadow(void *a){ return (unsigned char*)(((uintptr_t)a>>3)+OFF); }
static char *pool; static size_t pool_off;
__attribute__((constructor)) static void asan_init(void){
    pool = mmap((void*)POOL_ADDR, POOL_SIZE, PROT_READ|PROT_WRITE,
                MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    uintptr_t sh_lo = ((POOL_ADDR>>3)+OFF) & ~4095UL;
    size_t sh_sz = ((POOL_SIZE>>3)+8192) & ~4095UL;
    mmap((void*)sh_lo, sh_sz, PROT_READ|PROT_WRITE,
         MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    for(size_t i=0;i<POOL_SIZE;i+=8) *shadow(pool+i)=0xfa; /* pool starts all-poisoned */
}
static void set_sh(void *a,size_t n,unsigned char v){ uintptr_t p=(uintptr_t)a; for(size_t i=0;i<n;i+=8) *shadow((void*)(p+i))=v; }
static void unpoison(void *a,long n){ uintptr_t p=(uintptr_t)a; size_t full=((size_t)n/8)*8; set_sh(a,full,0); if((size_t)n%8) *shadow((void*)(p+full))=(unsigned char)((size_t)n%8); }
void *malloc(size_t n){
    size_t usable=(n+7)&~(size_t)7;
    size_t need=RZ+usable+RZ;
    if(pool_off+need>POOL_SIZE) return 0;
    char *base=pool+pool_off; pool_off+=need;
    char *user=base+RZ;
    unpoison(user,(long)n);           /* only [user,user+n) addressable */
    ((size_t*)base)[0]=usable;
    return user;
}
void free(void *p){ if(!p)return; char*base=(char*)p-RZ; size_t usable=((size_t*)base)[0]; set_sh(p,usable,0xfd); }
void *calloc(size_t a,size_t b){ char*p=malloc(a*b); if(p)for(size_t i=0;i<a*b;i++)p[i]=0; return p; }
void *realloc(void *p,size_t n){ char*q=malloc(n); if(q&&p){size_t u=((size_t*)((char*)p-RZ))[0]; for(size_t i=0;i<(u<n?u:n);i++)q[i]=((char*)p)[i];} return q; }

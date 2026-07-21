#include <windows.h>
#include <stdint.h>
#include <stddef.h>

#define OFF 0x7fff8000ULL
#define RZ 16
#define GRZ 0xf9
#define SHADOW_LO 0x7fff8000ULL
#define SHADOW_HI 0x100000000ULL

struct asan_global { void *addr; size_t size; };
extern struct asan_global __start___asan_globals[] __attribute__((weak));
extern struct asan_global __stop___asan_globals[] __attribute__((weak));

static HANDLE g_heap;

static unsigned char *shadow(void *a) { return (unsigned char *)(((uintptr_t)a >> 3) + OFF); }

static void set_sh(void *a, size_t n, unsigned char v) {
	uintptr_t p = (uintptr_t)a;
	for (size_t i = 0; i < n; i += 8) *shadow((void *)(p + i)) = v;
}
static void unpoison(void *a, long n) {
	uintptr_t p = (uintptr_t)a;
	size_t full = ((size_t)n / 8) * 8;
	set_sh(a, full, 0);
	if ((size_t)n % 8) *shadow((void *)(p + full)) = (unsigned char)((size_t)n % 8);
}

static void wstr(const char *s) {
	DWORD w; long n = 0; while (s[n]) n++;
	WriteFile(GetStdHandle(STD_ERROR_HANDLE), s, (DWORD)n, &w, 0);
}
static void wbuf(const char *b, int n) { DWORD w; WriteFile(GetStdHandle(STD_ERROR_HANDLE), b, (DWORD)n, &w, 0); }
static void whexn(uintptr_t v, int nyb, int nl) {
	char b[19]; int i;
	for (i = 0; i < nyb; i++) { int d = (int)((v >> ((nyb - 1 - i) * 4)) & 0xf); b[i] = (char)(d < 10 ? '0' + d : 'a' + d - 10); }
	if (nl) b[nyb] = '\n';
	wbuf(b, nyb + (nl ? 1 : 0));
}
#define PW ((int)(2 * sizeof(uintptr_t)))
static void whex(uintptr_t v) { wstr("0x"); whexn(v, PW, 1); }
static void whexa(uintptr_t v) { wstr("0x"); whexn(v, PW, 0); }
static void wdec(uintptr_t v) {
	char b[24]; int i = 24;
	if (!v) { wbuf("0", 1); return; }
	while (v) { b[--i] = (char)('0' + (int)(v % 10)); v /= 10; }
	wbuf(b + i, 24 - i);
}
static const char *asan_class(int sh) {
	switch (sh & 0xff) {
	case 0xfa: return "heap-buffer-overflow";
	case 0xfd: return "heap-use-after-free";
	case 0xf2: return "stack-buffer-overflow";
	case GRZ:  return "global-buffer-overflow";
	default:   return (sh >= 1 && sh <= 7) ? "buffer-overflow" : "bad memory access";
	}
}
static int asan_locate(uintptr_t addr, uintptr_t *rbeg, uintptr_t *rend, uintptr_t *roff, int *rdir) {
	uintptr_t g = addr & ~(uintptr_t)7; const int MAXG = 1 << 16;
	int fd = 0, fu = 0; uintptr_t dg = 0; int dv = 0; uintptr_t ug = 0;
	for (int i = 0; i < MAXG; i++) { uintptr_t gg = g - (uintptr_t)i * 8; unsigned v = *shadow((void *)gg); if (v == 0 || (v >= 1 && v <= 7)) { dg = gg; dv = (int)v; fd = 1; break; } if (gg < 8) break; }
	for (int i = 0; i < MAXG; i++) { uintptr_t gg = g + (uintptr_t)i * 8; unsigned v = *shadow((void *)gg); if (v == 0 || (v >= 1 && v <= 7)) { ug = gg; fu = 1; break; } }
	uintptr_t begL = 0, endL = 0, begR = 0, endR = 0;
	if (fd) { endL = dg + (uintptr_t)(dv ? dv : 8); uintptr_t b = dg; int i; for (i = 0; i < MAXG && b >= 8; i++) { if (*shadow((void *)(b - 8)) == 0) b -= 8; else break; } begL = b; if (b < 8 || i >= MAXG) fd = 0; }
	if (fu) { begR = ug; uintptr_t e = ug; int i, bnd = 0; for (i = 0; i < MAXG; i++) { unsigned v = *shadow((void *)e); if (v == 0) { e += 8; continue; } if (v <= 7) e += v; bnd = 1; break; } endR = e; if (!bnd) fu = 0; }
	if (fd && addr < endL) { *rbeg = begL; *rend = endL; *roff = addr - begL; *rdir = 2; return 1; }
	uintptr_t db = fd ? (addr - endL) : (uintptr_t)-1, da = fu ? (begR - addr) : (uintptr_t)-1;
	if (!fd && !fu) return 0;
	if (db <= da) { *rbeg = begL; *rend = endL; *roff = db; *rdir = 0; }
	else { *rbeg = begR; *rend = endR; *roff = da; *rdir = 1; }
	return 1;
}

static LONG CALLBACK on_ill(PEXCEPTION_POINTERS ep) {
	CONTEXT *c = ep->ContextRecord;
	if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION)
		return EXCEPTION_CONTINUE_SEARCH;
	long sh = (long)c->Rax;
	long off = (long)c->Rdx;
	uintptr_t addr = (uintptr_t)c->Rcx;
	wstr("=================================================================\n");
	wstr("==ERROR: AddressSanitizer: "); wstr(asan_class((int)sh));
	wstr(" (mcc native shadow)\n");
	if (addr) {
		long asz = off - (long)(addr & 7) + 1;
		unsigned char *s = shadow((void *)addr);
		wstr("    at faulting address "); whex(addr);
		wstr("    access size "); wdec((uintptr_t)(asz > 0 ? asz : 0)); wstr("\n");
		{ uintptr_t rb, re, ro; int rd;
		  if (asan_locate(addr, &rb, &re, &ro, &rd)) {
			wstr("    "); whexa(addr); wstr(" is located "); wdec(ro);
			wstr(rd == 0 ? " bytes to the right of a " : rd == 1 ? " bytes to the left of a " : " bytes inside a ");
			wdec(re - rb); wstr("-byte region ["); whexa(rb); wstr(", "); whexa(re); wstr(")\n");
		  } }
		wstr("  shadow bytes around 0x"); whexn((uintptr_t)s, PW, 1);
		wstr("   ");
		for (int cc = -8; cc < 8; cc++) { if (cc == 0) wstr("["); whexn((uintptr_t)(s[cc] & 0xff), 2, 0); if (cc == 0) wstr("]"); else wstr(" "); }
		wstr("\n");
	}
	wstr("    pc "); whex((uintptr_t)c->Rip);
	wstr("    shadow byte 0x"); whexn((uintptr_t)(sh & 0xff), 2, 0);
	wstr("  granule offset "); whexn((uintptr_t)(off & 0xff), 2, 1);
	ExitProcess(1);
	return EXCEPTION_CONTINUE_SEARCH;
}

void __asan_stack_enter(void *tab, void *fpv) {
	size_t *q; size_t fp = (size_t)fpv;
	for (q = tab; q[0]; q += 2) { char *obj = (char *)(fp + q[0]); size_t sz = q[1]; set_sh(obj + ((sz + 7) & ~(size_t)7), RZ, 0xf2); }
	for (q = tab; q[0]; q += 2) { unpoison((char *)(fp + q[0]), (long)q[1]); }
}
void __asan_stack_leave(void *tab, void *fpv) {
	size_t *q; size_t fp = (size_t)fpv;
	for (q = tab; q[0]; q += 2) { char *obj = (char *)(fp + q[0]); size_t sz = q[1]; set_sh(obj, ((sz + 7) & ~(size_t)7) + RZ, 0); }
}
static void asan_register_globals(void) {
	struct asan_global *g;
	for (g = __start___asan_globals; g < __stop___asan_globals; g++) {
		size_t rounded = (g->size + 7) & ~(size_t)7;
		unpoison(g->addr, (long)g->size);
		set_sh((char *)g->addr + rounded, RZ, GRZ);
	}
}

__attribute__((constructor)) static void asan_init(void) {
	g_heap = GetProcessHeap();
	VirtualAlloc((void *)SHADOW_LO, (SIZE_T)(SHADOW_HI - SHADOW_LO), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	AddVectoredExceptionHandler(1, on_ill);
	asan_register_globals();
}

void *malloc(size_t n) {
	size_t usable = (n + 7) & ~(size_t)7; size_t tot = RZ + usable + RZ;
	char *base = (char *)HeapAlloc(g_heap ? g_heap : (g_heap = GetProcessHeap()), 0, tot + 16);
	if (!base) return 0;
	char *user = base + RZ;
	set_sh(base, RZ, 0xfa); unpoison(user, (long)n); set_sh(user + usable, RZ, 0xfa);
	((size_t *)base)[0] = tot; ((size_t *)base)[1] = usable;
	return user;
}
void free(void *p) {
	if (!p) return;
	char *base = (char *)p - RZ; size_t usable = ((size_t *)base)[1];
	set_sh(p, usable, 0xfd);
	HeapFree(g_heap ? g_heap : (g_heap = GetProcessHeap()), 0, base);
}
void *calloc(size_t a, size_t b) { char *p = (char *)malloc(a * b); if (p) for (size_t i = 0; i < a * b; i++) p[i] = 0; return p; }
void *realloc(void *p, size_t n) {
	char *q = (char *)malloc(n);
	if (q && p) { size_t u = ((size_t *)((char *)p - RZ))[1]; for (size_t i = 0; i < (u < n ? u : n); i++) q[i] = ((char *)p)[i]; free(p); }
	return q;
}

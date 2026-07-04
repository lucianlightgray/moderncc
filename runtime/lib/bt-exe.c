#define CONFIG_MCC_BACKTRACE_ONLY
#define SINGLE_SOURCE 1
#define pstrcpy mcc_pstrcpy
#include "../mcchost.c"
#include "../mccrun.c"

#ifndef _WIN32
#define __declspec(n)
#endif

#ifdef _WIN64
static void bt_init_pe_prog_base(rt_context *p) {
	MEMORY_BASIC_INFORMATION mbi;
	addr_t imagebase;

	if (!p->prog_base)
		return;
	if (!VirtualQuery(p, &mbi, sizeof(mbi)) || !mbi.AllocationBase)
		return;
	imagebase = (addr_t)mbi.AllocationBase - p->prog_base;
	p->prog_base = (addr_t)mbi.AllocationBase - (imagebase & 0xffffffffu);
}
#endif

__declspec(dllexport) void __bt_init(rt_context *p, int is_exe) {
	__attribute__((weak)) int main();
	__attribute__((weak)) void __bound_init(void *, int);

	if (p->bounds_start)
		__bound_init(p->bounds_start, -1);

#ifdef _WIN64
	bt_init_pe_prog_base(p);
#endif

	rt_wait_sem();
	p->next = g_rc, g_rc = p;
	rt_post_sem();
	if (is_exe) {
		p->top_func = main;
		set_exception_handler();
	}
}

__declspec(dllexport) void __bt_exit(rt_context *p) {
	struct rt_context *rc, **pp;
	__attribute__((weak)) void __bound_exit_dll(void *);

	if (p->bounds_start)
		__bound_exit_dll(p->bounds_start);

	rt_wait_sem();
	for (pp = &g_rc; rc = *pp, rc; pp = &rc->next)
		if (rc == p) {
			*pp = rc->next;
			break;
		}
	rt_post_sem();
}

ST_FUNC char *pstrcpy(char *buf, size_t buf_size, const char *s) {
	int l = strlen(s);
	if (l >= buf_size)
		l = buf_size - 1;
	memcpy(buf, s, l);
	buf[l] = 0;
	return buf;
}

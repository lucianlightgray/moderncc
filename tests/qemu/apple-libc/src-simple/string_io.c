






















#include <stdlib.h>
#include <unistd.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <errno.h>

#include "os/internal.h"
#include "_simple.h"
#include "platform/string.h"
#include "platform/compat.h"

#ifndef VM_PAGE_SIZE
#define VM_PAGE_SIZE	4096
#endif

#define BUF_SIZE(s)	(((BUF *)(s))->end - ((BUF *)(s))->buf + 1)
#if DEBUG
#define MYBUFSIZE	256
#else

#define MYBUFSIZE	32
#endif

typedef struct _BUF {
	char *buf;
	char *ptr;
	char *end;
	int fd;
	int (*full)(struct _BUF *);
} BUF;


static int
_flush(BUF *b)
{
	char *buf = b->buf;
	ssize_t n = b->ptr - buf;
	ssize_t w;

	while (n > 0) {
		w = write(b->fd, buf, n);
		if (w < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			break;
		}
		n -= w;
		buf += n;
	}
	return 1;
}


static int
_flush_reset(BUF *b)
{
	_flush(b);
	b->ptr = b->buf;
	return 1;
}


static int
_enlarge(BUF *b)
{
	vm_address_t new;
	vm_size_t sold, snew;
	intptr_t diff;
	kern_return_t kr;

	new = (vm_address_t)(b->end + 1);
	if(vm_allocate(mach_task_self(), &new, VM_PAGE_SIZE, 0) == 0) {

		b->end += VM_PAGE_SIZE;
		return 1;
	}
	sold = BUF_SIZE(b);
	snew = (sold + VM_PAGE_SIZE) & ~(VM_PAGE_SIZE - 1);
	if ((kr = vm_allocate(mach_task_self(), &new, snew, 1)) != 0) {
		__LIBPLATFORM_CLIENT_CRASH__(kr, "Failed to allocate memory for buffer");
	}
	diff = new - (vm_address_t)b->buf;
	memmove((void *)new, b->buf, sold);
	if((intptr_t)(b->buf) & (VM_PAGE_SIZE - 1)) {
		sold &= ~(VM_PAGE_SIZE - 1);
		b->buf = (char *)((intptr_t)(b->buf + VM_PAGE_SIZE) & ~(VM_PAGE_SIZE - 1));
		b->end = (char *)(new + snew - 1);
	} else
		b->end += diff + VM_PAGE_SIZE;
	if(sold > 0) {
		vm_deallocate(mach_task_self(), (vm_address_t)b->buf, sold);
	}
	b->buf = (char *)new;
	b->ptr += diff;
	return 1;
}

static int
_snprintf_out_of_space(BUF *b)
{


	if(b->fd < INT_MAX) {
		b->fd++;
	}
	return 0;
}

static inline void put_s(BUF *, _esc_func, const char *);

static inline void
put_c(BUF *b, _esc_func esc, unsigned char c)
{
	const char *cp;

	if(esc && (cp = esc(c)) != NULL)
		put_s(b, NULL, cp);
	else {
		if(b->ptr >= b->end)
			if(!b->full(b)) {







				return;
			}
		*b->ptr++ = c;
	}
}


static inline void
put_s(BUF *b, _esc_func esc, const char *str)
{
	while(*str)
		put_c(b, esc, *str++);
}


static inline void
put_n(BUF *b, _esc_func esc, const char *str, ssize_t n)
{
	while(n-- > 0)
		put_c(b, esc, *str++);
}

#if __LP64__ || defined(__arm64__)
static unsigned long long
udiv10(unsigned long long a, unsigned long long *rem)
{
	*rem = a % 10;
	return a / 10;
}
#else
unsigned long long
udiv10(unsigned long long a, unsigned long long *rem_out)
{
	if (a <= UINT_MAX) {
		*rem_out = (unsigned long long)((unsigned int)a % 10);
		return (unsigned long long)((unsigned int)a / 10);
	}


	unsigned long long divisor  = 0xa000000000000000;
	unsigned long long dividend = a;
	unsigned long long quotient = 0;

	while (divisor >= 0xa) {
		quotient = quotient << 1;
		if (dividend >= divisor) {
			dividend -= divisor;
			quotient += 1;
		}
		divisor = divisor >> 1;
	}

	*rem_out = dividend;
	return quotient;
}
#endif






static void
dec(BUF *b, _esc_func esc, long long in, int width, int zero)
{
	char buf[32];
	char *cp = buf + sizeof(buf);
	ssize_t pad;
	int neg = 0;
	unsigned long long n = (unsigned long long)in;
	unsigned long long rem;

	if(in < 0) {
		neg++;
		width--;
		n = ~n + 1;
	}
	*--cp = 0;
	if(n) {
		while(n) {
			n = udiv10(n, &rem);
			*--cp = rem + '0';
		}
	} else
		*--cp = '0';
	if(neg && zero) {
		put_c(b, esc, '-');
		neg = 0;
	}
	pad = width - strlen(cp);
	zero = zero ? '0' : ' ';
	while(pad-- > 0)
		put_c(b, esc, zero);
	if(neg)
		put_c(b, esc, '-');
	put_s(b, esc, cp);
}






static void
oct(BUF *b, _esc_func esc, unsigned long long n, int width, int zero)
{
	char buf[32];
	char *cp = buf + sizeof(buf);
	ssize_t pad;

	*--cp = 0;
	if (n) {
		while (n) {
			*--cp = (n % 8) + '0';
			n /= 8;
		}
	} else {
		*--cp = '0';
	}
	pad = width - strlen(cp);
	zero = zero ? '0' : ' ';
	while (pad-- > 0) {
		put_c(b, esc, zero);
	}
	put_s(b, esc, cp);
}








static const char _h[] = "0123456789abcdef";
static const char _H[] = "0123456789ABCDEF";
static const char _0x[] = "0x";

static void
hex(BUF *b, _esc_func esc, unsigned long long n, int width, int zero, int upper, int p)
{
	char buf[32];
	char *cp = buf + sizeof(buf);
	const char *h = upper ? _H : _h;

	*--cp = 0;
	if(n) {
		while(n) {
			*--cp = h[n & 0xf];
			n >>= 4;
		}
	} else
		*--cp = '0';
	if(p) {
		width -= 2;
		if(zero) {
			put_s(b, esc, _0x);
			p = 0;
		}
	}
	width -= strlen(cp);
	zero = zero ? '0' : ' ';
	while(width-- > 0)
		put_c(b, esc, zero);
	if(p)
		put_s(b, esc, _0x);
	put_s(b, esc, cp);
}






static void
udec(BUF *b, _esc_func esc, unsigned long long n, int width, int zero)
{
	char buf[32];
	char *cp = buf + sizeof(buf);
	unsigned long long rem;
	ssize_t pad;

	*--cp = 0;
	if(n) {
		while(n) {
			n = udiv10(n, &rem);
			*--cp = rem + '0';
		}
	} else
		*--cp = '0';
	pad = width - strlen(cp);
	zero = zero ? '0' : ' ';
	while(pad-- > 0)
		put_c(b, esc, zero);
	put_s(b, esc, cp);
}






static void
ydec(BUF *b, _esc_func esc, unsigned long long n, int width, int zero)
{
	if(n >= 10 * (1 << 20)) {
		n += (1 << 19);
		udec(b, esc, n >> 20, width, zero);
		put_s(b, esc, "MB");
	} else if (n >= 10 * (1 << 10)) {
		n += (1 << 9);
		udec(b, esc, n >> 10, width, zero);
		put_s(b, esc, "KB");
	} else {
		udec(b, esc, n, width, zero);
		put_s(b, esc, "b");
	}
}




static void
__simple_bprintf(BUF *b, _esc_func esc, const char *fmt, va_list ap)
{
	while(*fmt) {
		int lflag, zero, width;
		char *cp;
		if(!(cp = strchr(fmt, '%'))) {
			put_s(b, esc, fmt);
			break;
		}
		put_n(b, esc, fmt, cp - fmt);
		fmt = cp + 1;
		if(*fmt == '%') {
			put_c(b, esc, '%');
			fmt++;
			continue;
		}
		lflag = zero = width = 0;
		for(;;) {
			if ( strncmp(fmt, ".*s", 3) == 0 ) {

				width = va_arg(ap, int);
				cp = va_arg(ap, char *);
				while(width-- > 0)
					put_c(b, esc, *cp++);
				fmt += 2;
				break;
			}
			switch(*fmt) {
			case '0':
				zero++;
				fmt++;

			case '1': case '2': case '3': case '4': case '5':
			case '6': case '7': case '8': case '9':
				while(*fmt >= '0' && *fmt <= '9')
					width = 10 * width + (*fmt++ - '0');
				continue;
			case 'c':
				zero = zero ? '0' : ' ';
				width--;
				while(width-- > 0)
					put_c(b, esc, zero);
				put_c(b, esc, va_arg(ap, int));
				break;
			case 'd': case 'i':
				switch(lflag) {
				case 0:
					dec(b, esc, va_arg(ap, int), width, zero);
					break;
				case 1:
					dec(b, esc, va_arg(ap, long), width, zero);
					break;
				default:
					dec(b, esc, va_arg(ap, long long), width, zero);
					break;
				}
				break;
			case 'l':
				lflag++;
				fmt++;
				continue;
			case 'o':
				switch (lflag) {
				case 0:
					oct(b, esc, va_arg(ap, int), width, zero);
					break;
				case 1:
					oct(b, esc, va_arg(ap, long), width, zero);
					break;
				default:
					oct(b, esc, va_arg(ap, long long), width, zero);
					break;
				}
				break;
			case 'p':
				hex(b, esc, (unsigned long)va_arg(ap, void *), width, zero, 0, 1);
				break;
			case 's':
				cp = va_arg(ap, char *);
				cp = cp ? cp : "(null)";
				width -= strlen(cp);
				zero = zero ? '0' : ' ';
				while(width-- > 0)
					put_c(b, esc, zero);
				put_s(b, esc, cp);
				break;
			case 'u':
				switch(lflag) {
				case 0:
					udec(b, esc, va_arg(ap, unsigned int), width, zero);
					break;
				case 1:
					udec(b, esc, va_arg(ap, unsigned long), width, zero);
					break;
				default:
					udec(b, esc, va_arg(ap, unsigned long long), width, zero);
					break;
				}
				break;
			case 'X': case 'x':
				switch(lflag) {
				case 0:
					hex(b, esc, va_arg(ap, unsigned int), width, zero,
						*fmt == 'X', 0);
					break;
				case 1:
					hex(b, esc, va_arg(ap, unsigned long), width, zero,
						*fmt == 'X', 0);
					break;
				default:
					hex(b, esc, va_arg(ap, unsigned long long), width, zero,
						*fmt == 'X', 0);
					break;
				}
				break;
			case 'y':
				switch(lflag) {
				case 0:
					ydec(b, esc, va_arg(ap, unsigned int), width, zero);
					break;
				case 1:
					ydec(b, esc, va_arg(ap, unsigned long), width, zero);
					break;
				default:
					ydec(b, esc, va_arg(ap, unsigned long long), width, zero);
					break;
				}
				break;
			default:
				put_c(b, esc, *fmt);
				break;
			}
			break;
		}
		fmt++;
	}
}






void
_simple_vdprintf(int fd, const char *fmt, va_list ap)
{
	BUF b;
	char buf[MYBUFSIZE];

	b.buf = buf;
	b.fd = fd;
	b.ptr = b.buf;
	b.end = b.buf + MYBUFSIZE;
	b.full = _flush_reset;
	__simple_bprintf(&b, NULL, fmt, ap);
	_flush(&b);
}






void
_simple_dprintf(int fd, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	_simple_vdprintf(fd, fmt, ap);
	va_end(ap);
}








_SIMPLE_STRING
_simple_salloc(void)
{
	BUF *b;

	if(vm_allocate(mach_task_self(), (vm_address_t *)&b, VM_PAGE_SIZE, 1))
		return NULL;
	b->ptr = b->buf = (char *)b + sizeof(BUF);
	b->end = (char *)b + VM_PAGE_SIZE - 1;
	b->full = _enlarge;
	return (_SIMPLE_STRING)b;
}







int
_simple_vsprintf(_SIMPLE_STRING b, const char *fmt, va_list ap)
{
	return _simple_vesprintf(b, NULL, fmt, ap);
}







int
_simple_sprintf(_SIMPLE_STRING b, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = _simple_vesprintf(b, NULL, fmt, ap);
	va_end(ap);
	return ret;
}






int
_simple_vesprintf(_SIMPLE_STRING b, _esc_func esc, const char *fmt, va_list ap)
{
	__simple_bprintf((BUF *)b, esc, fmt, ap);
	return 0;
}






int _simple_esprintf(_SIMPLE_STRING b, _esc_func esc, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = _simple_vesprintf(b, esc, fmt, ap);
	va_end(ap);
	return ret;
}





char *
_simple_string(_SIMPLE_STRING b)
{
	*((BUF *)b)->ptr = 0;
	return ((BUF *)b)->buf;
}





void
_simple_sresize(_SIMPLE_STRING b)
{
	((BUF *)b)->ptr = ((BUF *)b)->buf + strlen(((BUF *)b)->buf);
}





int
_simple_sappend(_SIMPLE_STRING b, const char *str)
{
	return _simple_esappend(b, NULL, str);
}






int _simple_esappend(_SIMPLE_STRING b, _esc_func esc, const char *str)
{
	put_s((BUF *)b, esc, str);
	return 0;
}




void
_simple_put(_SIMPLE_STRING b, int fd)
{
	((BUF *)b)->fd = fd;
	_flush((BUF *)b);
}





void
_simple_putline(_SIMPLE_STRING b, int fd)
{
	((BUF *)b)->fd = fd;
	*((BUF *)b)->ptr++ = '\n';
	_flush((BUF *)b);
	((BUF *)b)->ptr--;
}




void
_simple_sfree(_SIMPLE_STRING b)
{
	vm_size_t s;

	if(b == NULL) return;
	if(((intptr_t)(((BUF *)b)->buf) & (VM_PAGE_SIZE - 1)) == 0) {
		vm_deallocate(mach_task_self(), (vm_address_t)((BUF *)b)->buf, BUF_SIZE(b));
		s = VM_PAGE_SIZE;
	} else {
		s = ((BUF *)b)->end - (char *)b + 1;
	}
	vm_deallocate(mach_task_self(), (vm_address_t)b, s);
}





int
_simple_vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
	if(size == 0 || size >= INT_MAX) {
		return -1;
	}

	BUF b;
	b.buf = str;
	b.ptr = b.buf;


	b.end = b.buf + size - 1;

	b.fd = 0;
	b.full = _snprintf_out_of_space;

	__simple_bprintf(&b, NULL, fmt, ap);

	if(b.ptr < str || (b.ptr - str) >= size) {
		__LIBPLATFORM_INTERNAL_CRASH__((uintptr_t)(b.ptr - str),
				"Overflow in _simple_snprintf");
	}

	*(b.ptr) = '\0';
	size_t ret = (size_t)(b.ptr - b.buf) + (size_t)b.fd;
	if(ret > INT_MAX) {
		return INT_MAX;
	} else {
		return (int)ret;
	}
}

int
_simple_snprintf(char *str, size_t size, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = _simple_vsnprintf(str, size, fmt, ap);
	va_end(ap);
	return ret;
}

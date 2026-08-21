typedef struct
{
	const char *name, *req, *cmd, *expect;
} cli_case_t;

static const cli_case_t cli_cases[] = {

		{"builtin_f128_inf_nan", "cpu=arm64",
		 "printf 'int main(void){__float128 i=__builtin_infq(),h=__builtin_huge_valq(),j=__builtin_inff128();"
		 "__float128 n=__builtin_nanq(\"\"),s=__builtin_nansq(\"\"),m=__builtin_nanf128(\"\"),hf=__builtin_huge_valf128();"
		 "return ((i==h)&&(i==j)&&(i==hf)&&(i>(__float128)1e300)&&(n!=n)&&(s!=s)&&(m!=m))?0:1;}\\n' > {W}/f128b.c && "
		 "{MCC} -B{B} -I{I} -run {W}/f128b.c && echo OK",
		 "OK\n"},

		{"arm64_ubsan_divrem_int_min_traps", "cpu=arm64",
		 "printf 'int f(int a,int b){return a/b;}int main(){volatile int x=(-2147483647-1),y=-1;return f(x,y);}\\n' > {W}/dov.c && "
		 "printf 'int f(int a,int b){return a%%b;}int main(){volatile int x=(-2147483647-1),y=-1;return f(x,y);}\\n' > {W}/rov.c && "
		 "printf 'long f(long a,long b){return a/b;}int main(){volatile long x=(-9223372036854775807L-1),y=-1;return f(x,y)!=0;}\\n' > {W}/lov.c && "
		 "printf 'int f(int a,int b){return a/b;}int main(){volatile int x=10,y=3;return f(x,y);}\\n' > {W}/uok.c && "
		 "printf 'int f(int a,int b){return a/b;}int main(){volatile int x=(-2147483647-1),y=-2;return f(x,y)==1073741824?7:99;}\\n' > {W}/nm.c && "
		 "{MCC} -B{B} -I{I} -fsanitize=undefined {W}/dov.c -o {W}/dov && {W}/dov ; echo dov=$? ; "
		 "{MCC} -B{B} -I{I} -fsanitize=undefined {W}/rov.c -o {W}/rov && {W}/rov ; echo rov=$? ; "
		 "{MCC} -B{B} -I{I} -fsanitize=undefined {W}/lov.c -o {W}/lov && {W}/lov ; echo lov=$? ; "
		 "{MCC} -B{B} -I{I} -fsanitize=undefined {W}/uok.c -o {W}/uok && {W}/uok ; echo ok=$? ; "
		 "{MCC} -B{B} -I{I} -fsanitize=undefined {W}/nm.c -o {W}/nm && {W}/nm ; echo nm=$?",
		 "dov=133\nrov=133\nlov=133\nok=3\nnm=7\n"},

		{"float32_float64_type_keywords", "",
		 "printf 'int main(void){_Float32 a=1.5f;float b=a;_Float64 c=2.5;double d=c;"
		 "_Float32 e=a+1.0f;_Float32 h=1.5f32;_Float64 g=2.5f64;"
		 "_Float32x xa=1.5;double xad=xa;_Float64x xb=2.5;long double xbl=xb;"
		 "return (sizeof(_Float32)==4&&sizeof(_Float64)==8&&b==1.5f&&d==2.5"
		 "&&(double)e==2.5&&(double)h==1.5&&(double)g==2.5"
		 "&&sizeof(_Float32x)==sizeof(double)&&sizeof(_Float64x)==sizeof(long double)"
		 "&&xad==1.5&&(double)xbl==2.5)?0:1;}\\n' > {W}/f3264.c && "
		 "{MCC} -B{B} -I{I} -run {W}/f3264.c && echo OK",
		 "OK\n"},

		{"generic_floatn_distinctness", "",
		 "printf 'int main(void){float f;double d;long double L;"
		 "_Float32 a;_Float64 b;_Float32x c;_Float64x e;"
		 "int ok=_Generic(f,float:1,_Float32:0,default:9)==1"
		 "&&_Generic(a,float:0,_Float32:1,default:9)==1"
		 "&&_Generic(b,double:0,_Float64:1,default:9)==1"
		 "&&_Generic(c,double:0,_Float32x:1,_Float64:0,default:9)==1"
		 "&&_Generic(e,double:0,_Float64x:1,default:9)==1"
		 "&&__builtin_types_compatible_p(_Float32,float)==0"
		 "&&__builtin_types_compatible_p(_Float32x,_Float64)==0"
		 "&&__builtin_types_compatible_p(_Float64x,long double)==0"
		 "&&__builtin_types_compatible_p(_Float32,_Float32)==1"
		 "&&sizeof(_Float32)==4&&sizeof(_Float64)==8;"
		 "_Float32 g=1.5f;float h=g;ok=ok&&(h==1.5f);"
		 "return ok?0:1;}\\n' > {W}/gfn.c && "
		 "{MCC} -B{B} -I{I} -run {W}/gfn.c && echo OK",
		 "OK\n"},

		{"generic_float128_alias", "os!=WIN32:no __float128 on PE (MCC_HAVE_FLOAT128 off, mcc.h)",
		 "printf 'int main(void){"
		 "int ok=__builtin_types_compatible_p(_Float128,__float128)==1"
		 "&&__builtin_types_compatible_p(_Float128,double)==0"
		 "&&sizeof(__float128)==16;"
		 "return ok?0:1;}\\n' > {W}/g128.c && "
		 "{MCC} -B{B} -I{I} -run {W}/g128.c && echo OK",
		 "OK\n"},

		{"complex_bitint", "",
		 "printf 'int main(void){"
		 "_Complex _BitInt(20) z=2+3i,w=4+5i,s=z+w,p=z*w;"
		 "_Complex _BitInt(20) a=1000+0i,b=1000+0i,c=a*b;"
		 "_Complex unsigned _BitInt(4) u=10+0i,v=10+0i,uw=u+v;"
		 "int ok=__real__ s==6&&__imag__ s==8"
		 "&&__real__ p==-7&&__imag__ p==22"
		 "&&(int)__real__ c==-48576"
		 "&&(int)__real__ uw==4"
		 "&&sizeof(_Complex _BitInt(20))==8"
		 "&&_Generic(z,_Complex _BitInt(20):1,_Complex int:2,default:0)==1"
		 "&&_Generic(z,_Complex _BitInt(21):2,default:9)==9;"
		 "return ok?0:1;}\\n' > {W}/cbi.c && "
		 "{MCC} -B{B} -I{I} -run {W}/cbi.c && echo OK",
		 "OK\n"},

		{"decimal_types_slice1", "",
		 "printf '_Static_assert(sizeof(_Decimal32)==4,\"\");"
		 "_Static_assert(sizeof(_Decimal64)==8,\"\");"
		 "_Static_assert(sizeof(_Decimal128)==16,\"\");"
		 "int main(void){_Decimal32 a;_Decimal64 b;_Decimal128 c;"
		 "int ok=sizeof(a)==4&&sizeof(b)==8&&sizeof(c)==16&&_Alignof(_Decimal64)==8"
		 "&&_Generic(a,_Decimal32:1,_Decimal64:0,double:0,default:9)==1"
		 "&&_Generic(b,_Decimal64:1,double:0,default:9)==1"
		 "&&_Generic(c,_Decimal128:1,default:9)==1"
		 "&&__builtin_types_compatible_p(_Decimal64,double)==0"
		 "&&__builtin_types_compatible_p(_Decimal32,_Decimal64)==0;"
		 "return ok?0:1;}\\n' > {W}/dec1.c && "
		 "{MCC} -B{B} -I{I} -run {W}/dec1.c && echo OK",
		 "OK\n"},

		{"fixedpoint_types_slice1", "",
		 "printf '_Static_assert(sizeof(short _Fract)==1,\"\");"
		 "_Static_assert(sizeof(_Fract)==2,\"\");"
		 "_Static_assert(sizeof(long _Fract)==4,\"\");"
		 "_Static_assert(sizeof(short _Accum)==2,\"\");"
		 "_Static_assert(sizeof(_Accum)==4,\"\");"
		 "_Static_assert(sizeof(long _Accum)==8,\"\");"
		 "int main(void){short _Fract a;_Fract b;_Accum e;_Sat _Fract sb;"
		 "int ok=sizeof(a)==1&&sizeof(b)==2&&sizeof(e)==4&&sizeof(unsigned long _Accum)==8"
		 "&&_Generic(b,_Fract:1,_Accum:0,short _Fract:0,default:9)==1"
		 "&&_Generic(a,short _Fract:1,_Fract:0,default:9)==1"
		 "&&_Generic(sb,_Sat _Fract:1,_Fract:0,default:9)==1"
		 "&&__builtin_types_compatible_p(_Fract,short _Fract)==0"
		 "&&__builtin_types_compatible_p(_Sat _Fract,_Fract)==0"
		 "&&__builtin_types_compatible_p(unsigned _Fract,_Fract)==0;"
		 "return ok?0:1;}\\n' > {W}/fx1.c && "
		 "{MCC} -B{B} -I{I} -run {W}/fx1.c && echo OK",
		 "OK\n"},

		{"arm64_disasm_fp_families", "cpu=arm64",
		 "printf '.text\\n.globl _t\\n_t:\\n frintn s0, s1\\n frintp s2, s3\\n frintm s4, s5\\n frintz s6, s7\\n frinta s8, s9\\n frintx s10, s11\\n frinti s12, s13\\n frintz d2, d3\\n fcsel s0, s1, s2, eq\\n fcsel d3, d4, d5, ne\\n fcmpe s2, s3\\n fcmpe s4, #0.0\\n fcmpe d0, d1\\n fcvtns w0, s1\\n fcvtau w6, s7\\n fcvtps w8, s9\\n fcvtms x2, d3\\n fmov s0, #1.0\\n fmov s6, #-0.5\\n fmov d0, #2.0\\n ldr x1, Lp\\n ldr s2, Lp\\n ret\\n .p2align 3\\nLp: .quad 0\\n' > {W}/fpf.s && "
		 "clang -c {W}/fpf.s -o {W}/fpf.o 2>/dev/null && "
		 "{MCC} -B{B} -S {W}/fpf.o -o {W}/fpf.dis.s 2>/dev/null && "
		 "grep '//' {W}/fpf.dis.s | sed 's#.*// ##; s#[[:space:]]\\+# #g' | LC_ALL=C sort",
		 "fcmpe d0, d1\nfcmpe s2, s3\nfcmpe s4, #0.0\nfcsel d3, d4, d5, ne\nfcsel s0, s1, s2, eq\nfcvtau w6, s7\nfcvtms x2, d3\nfcvtns w0, s1\nfcvtps w8, s9\nfmov d0, #2.00000000\nfmov s0, #1.00000000\nfmov s6, #-0.50000000\nfrinta s8, s9\nfrinti s12, s13\nfrintm s4, s5\nfrintn s0, s1\nfrintp s2, s3\nfrintx s10, s11\nfrintz d2, d3\nfrintz s6, s7\nldr s2, 0x60\nldr x1, 0x60\n"},

		{"arm64_disasm_families", "cpu=arm64",
		 "printf '.text\\n.globl _t\\n_t:\\n clz x0, x1\\n rbit x2, x3\\n rev x4, x5\\n smull x0, w1, w2\\n umulh x6, x7, x8\\n bfxil x9, x10, #8, #16\\n clrex\\n yield\\n ldxr x11, [x12]\\n stlr x13, [x14]\\n svc #0\\n ret\\n' > {W}/fam.s && "
		 "clang -c {W}/fam.s -o {W}/fam.o 2>/dev/null && "
		 "{MCC} -B{B} -S {W}/fam.o -o {W}/fam.dis.s 2>/dev/null && "
		 "grep '//' {W}/fam.dis.s | sed 's#.*// ##; s#[[:space:]]\\+# #g' | LC_ALL=C sort",
		 "bfxil x9, x10, #8, #16\nclrex\nclz x0, x1\nldxr x11, [x12]\nrbit x2, x3\nrev x4, x5\nsmull x0, w1, w2\nstlr x13, [x14]\nsvc #0\numulh x6, x7, x8\nyield\n"},

		{"coff_reloc_count_overflow_65535", "cpu=x86_64,os=WIN32",
		 "printf 'int g=7;\\n"
		 "#define A &g,\\n"
		 "#define B A A A A A A A A\\n"
		 "#define C B B B B B B B B\\n"
		 "#define D C C C C C C C C\\n"
		 "#define E D D D D D D D D\\n"
		 "#define F E E E E E E E E\\n"
		 "void*arr[]={F F E};\\n"
		 "int main(void){int n=(int)(sizeof arr/sizeof arr[0]);"
		 "int ok=arr[0]==(void*)&g&&arr[40000]==(void*)&g&&arr[n-1]==(void*)&g;"
		 "return ok?(n==69632?42:7):1;}\\n' > {W}/relovfl.c && "
		 "{MCC} -B{B} -I{I} -c {W}/relovfl.c -o {W}/relovfl.obj && "
		 "{MCC} -B{B} -I{I} {W}/relovfl.obj -o {W}/relovfl.exe && "
		 "{W}/relovfl.exe ; echo rc=$?",
		 "rc=42\n"},

		{"coff_weak_def_no_multidef", "cpu=x86_64,os=WIN32",
		 "printf 'int foo=5;\\n' > {W}/wa.c && "
		 "printf 'int foo __attribute__((weak))=2;\\nint helper(void){return foo;}\\n' > {W}/wb.c && "
		 "printf 'int helper(void);\\nint main(void){return helper()==5?42:1;}\\n' > {W}/wms.c && "
		 "printf 'int helper(void);\\nint main(void){return helper()==2?42:1;}\\n' > {W}/wmw.c && "
		 "{MCC} -B{B} -I{I} -c {W}/wa.c -o {W}/wa.obj && "
		 "{MCC} -B{B} -I{I} -c {W}/wb.c -o {W}/wb.obj && "
		 "{MCC} -B{B} -I{I} -c {W}/wms.c -o {W}/wms.obj && "
		 "{MCC} -B{B} -I{I} -c {W}/wmw.c -o {W}/wmw.obj && "
		 "{MCC} -B{B} -I{I} {W}/wa.obj {W}/wb.obj {W}/wms.obj -o {W}/sw.exe && {W}/sw.exe; echo sw=$? ; "
		 "{MCC} -B{B} -I{I} {W}/wb.obj {W}/wmw.obj -o {W}/wo.exe && {W}/wo.exe; echo wo=$?",
		 "sw=42\nwo=42\n"},

		{"fno_common_multidef", "",
		 "printf 'int g; int seta(void){g=44;return 0;}\\n' > {W}/cma.c && "
		 "printf 'int g; int seta(void); int main(void){seta();return g;}\\n' > {W}/cmm.c && "
		 "{MCC} -B{B} -I{I} -c {W}/cma.c -o {W}/cma.o && "
		 "{MCC} -B{B} -I{I} -c {W}/cmm.c -o {W}/cmm.o && "
		 "if {MCC} -B{B} -I{I} {W}/cma.o {W}/cmm.o -o {W}/cmd.exe 2>/dev/null; "
		 "then echo def=LINKED; else echo def=MULTIDEF; fi ; "
		 "{MCC} -B{B} -I{I} -fcommon -c {W}/cma.c -o {W}/cmac.o && "
		 "{MCC} -B{B} -I{I} -fcommon -c {W}/cmm.c -o {W}/cmmc.o && "
		 "{MCC} -B{B} -I{I} -fcommon {W}/cmac.o {W}/cmmc.o -o {W}/cmc.exe && {W}/cmc.exe ; echo fcommon=$?",
		 "def=MULTIDEF\nfcommon=44\n"},

		{"coop_mn_win32_multiworker", "cpu=x86_64,os=WIN32",
		 "printf '#include <threads.h>\\n"
		 "static mtx_t L;static long c=0;\\n"
		 "static int w(void*a){(void)a;for(int i=0;i<50000;i++){mtx_lock(&L);c++;mtx_unlock(&L);}return 0;}\\n"
		 "int main(void){thrd_t t[8];mtx_init(&L,mtx_plain);"
		 "for(int i=0;i<8;i++)thrd_create(&t[i],w,0);"
		 "for(int i=0;i<8;i++)thrd_join(t[i],0);return c==400000?42:1;}\\n' > {W}/coopmn.c && "
		 "{MCC} -B{B} -I{I} -DMCC_THREADS_COOP -DMCC_COOP_MN -pthread {W}/coopmn.c -o {W}/coopmn.exe && "
		 "{W}/coopmn.exe ; echo rc=$?",
		 "rc=42\n"},
		{"debug_dwarf_struct_decl_line", "os=darwin",
		 "printf 'struct Point {\\nint px;\\nint py;\\n};\\nunion Wrap {\\nint wi;\\nfloat wf;\\n};\\nenum Color {\\nCLR_A,\\nCLR_B\\n};\\nint main(void) {\\nstruct Point p;\\nunion Wrap w;\\nenum Color c;\\np.px = 1; p.py = 2; w.wi = 3; c = CLR_A;\\nreturn p.px + p.py + w.wi + (int)c;\\n}\\n' > {W}/dl.c && "
		 "{MCC} -B{B} -I{I} -gdwarf-5 -c {W}/dl.c -o {W}/dl.o && "
		 "dwarfdump {W}/dl.o | awk '/DW_TAG_/{n=\"\"} /DW_AT_name/{gsub(/.*\\(\"|\"\\).*/,\"\");n=$0} /DW_AT_decl_line/{gsub(/.*\\(|\\).*/,\"\");print n\" \"$0}' | grep -E '^(Point|px|py|Wrap|wi|wf|Color) ' | LC_ALL=C sort",
		 "Color 9\nPoint 1\nWrap 5\npx 2\npy 3\nwf 7\nwi 6\n"},

		{"float128_static_init_const_fold", "cpu=arm64",
		 "printf '__float128 g=2.5;__float128 z=0.0;__float128 n=-1.5;"
		 "__float128 big=1234567890123LL;__float128 ql=1.5q;_Float128 h=2.5;"
		 "__float128 arr[2]={1.0,100.0};"
		 "int main(void){static __float128 sl=100.0;"
		 "return ((double)g==2.5&&(double)z==0.0&&(double)n==-1.5"
		 "&&(double)big==1234567890123.0&&(double)ql==1.5&&(double)h==2.5"
		 "&&(double)arr[0]==1.0&&(double)arr[1]==100.0&&(double)sl==100.0)?0:1;}\\n' > {W}/f128si.c && "
		 "{MCC} -B{B} -I{I} -run {W}/f128si.c && echo OK",
		 "OK\n"},

		{"float128_literal_suffix_q_f128", "cpu=arm64",
		 "printf 'int main(void){__float128 a=1.5q,b=1.5Q,c=1.5f128,d=1.5F128;"
		 "__float128 s=1.5q+2.5q;__float128 h=0x1.8p0q;"
		 "return ((double)a==1.5&&(double)b==1.5&&(double)c==1.5&&(double)d==1.5"
		 "&&(double)s==4.0&&(double)h==1.5)?0:1;}\\n' > {W}/q128.c && "
		 "{MCC} -B{B} -I{I} -run {W}/q128.c && echo OK",
		 "OK\n"},

		{"builtin_overflow_p_ice_fold", "",
		 "printf 'int a1[__builtin_add_overflow_p(2147483647,1,(int)0)==1?1:-1];\\n"
		 "int a2[__builtin_add_overflow_p(1,1,(int)0)==0?1:-1];\\n"
		 "int a3[__builtin_mul_overflow_p(65536,65536,(int)0)==1?1:-1];\\n"
		 "int a6[__builtin_add_overflow_p(255,1,(unsigned char)0)==1?1:-1];\\n"
		 "int main(void){volatile int x=2147483647,y=1;"
		 "return __builtin_add_overflow_p(x,y,(int)0)==1?0:1;}\\n' > {W}/ovp.c && "
		 "{MCC} -B{B} -I{I} -run {W}/ovp.c && echo OK",
		 "OK\n"},

		{"builtin_constant_p_foldstr", "",
		 "printf 'int a1[__builtin_constant_p(__builtin_strlen(\"hello\"))?1:-1];\\n"
		 "int a2[__builtin_constant_p(__builtin_memcmp(\"abc\",\"abc\",3))?1:-1];\\n"
		 "int a3[__builtin_constant_p(__builtin_strcmp(\"a\",\"b\"))?1:-1];\\n"
		 "int a4[__builtin_constant_p((1,7))?-1:1];\\n"
		 "int main(void){volatile int x=5;char *p=\"xy\";\\n"
		 "return (__builtin_constant_p(__builtin_strlen(\"hi\"))"
		 "&&!__builtin_constant_p((1,7))&&!__builtin_constant_p(x)"
		 "&&!__builtin_constant_p(__builtin_strlen(p)))?0:1;}\\n' > {W}/bcp.c && "
		 "{MCC} -B{B} -I{I} -run {W}/bcp.c && echo OK",
		 "OK\n"},

		{"attr_error_warning_poison", "",
		 "printf '__attribute__((error(\"do not call\"))) void foo(void); void u(void){foo();}\\n' > {W}/pe.c && "
		 "printf '__attribute__((warning(\"legacy api\"))) int bar(void); int bar(void){return 3;} int main(void){return bar()-3;}\\n' > {W}/pw.c && "
		 "printf 'extern void g(void) __attribute__((__error__(\"glibc form\"))); void u(void){g();}\\n' > {W}/pg.c && "
		 "printf '__attribute__((error(\"no\"))) void foo(void); void(*p)(void)=foo; void u(void){if(0)foo();}\\n' > {W}/pn.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/pe.c -o {W}/pe.o 2>&1 | grep -o 'declared with attribute error: do not call' ; "
		 "{MCC} -B{B} -I{I} -run {W}/pw.c 2>&1 | grep -o 'declared with attribute warning: legacy api' ; "
		 "{MCC} -B{B} -I{I} -c {W}/pg.c -o {W}/pg.o 2>&1 | grep -o 'attribute error: glibc form' ; "
		 "{MCC} -B{B} -I{I} -c {W}/pn.c -o {W}/pn.o 2>/dev/null && echo NOFIRE_OK ; }",
		 "declared with attribute error: do not call\n"
		 "declared with attribute warning: legacy api\n"
		 "attribute error: glibc form\n"
		 "NOFIRE_OK\n"},

		{"address_of_packed_member_warn", "",
		 "printf 'struct __attribute__((packed)) S{char c;int x;};int*f(struct S*s){return &s->x;}\\n' > {W}/pm.c && "
		 "printf 'struct __attribute__((packed)) S{int x;char c;};char*f(struct S*s){return &s->c;}\\nstruct T{char c;int y;};int*g(struct T*t){return &t->y;}\\n' > {W}/pnp.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/pm.c -o {W}/pm.o 2>&1 | grep -o 'packed member may result in an unaligned' ; "
		 "{MCC} -B{B} -I{I} -c {W}/pnp.c -o {W}/pnp.o 2>&1 | grep -c 'packed' ; "
		 "{MCC} -B{B} -I{I} -Wno-address-of-packed-member -c {W}/pm.c -o {W}/pm2.o 2>&1 | grep -c 'packed' ; echo OK ; }",
		 "packed member may result in an unaligned\n0\n0\nOK\n"},

		{"implicit_fallthrough_warn", "",
		 "printf 'int f(int x){int r=0;switch(x){case 1: r=1; case 2: r=2; break;}return r;}\\n' > {W}/ift.c && "
		 "printf 'int g(int x){int r=0;switch(x){case 1: r=1; break; case 2: r=2; break;}return r;}\\n' > {W}/ifb.c && "
		 "printf 'int h(int x){int r=0;switch(x){case 1: r=1; __attribute__((fallthrough)); case 2: r=2; break;}return r;}\\n' > {W}/ifa.c && "
		 "{ {MCC} -B{B} -I{I} -Wimplicit-fallthrough -c {W}/ift.c -o {W}/ift.o 2>&1 | grep -o 'this statement may fall through' ; "
		 "{MCC} -B{B} -I{I} -c {W}/ift.c -o {W}/ift2.o 2>&1 | grep -c 'fall through' ; "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/ift.c -o {W}/ift3.o 2>&1 | grep -c 'fall through' ; "
		 "{MCC} -B{B} -I{I} -Wimplicit-fallthrough -c {W}/ifb.c -o {W}/ifb.o 2>&1 | grep -c 'fall through' ; "
		 "{MCC} -B{B} -I{I} -Wimplicit-fallthrough -c {W}/ifa.c -o {W}/ifa.o 2>&1 | grep -c 'fall through' ; echo OK ; }",
		 "this statement may fall through\n0\n0\n0\n0\nOK\n"},

		{"nonnull_null_arg_warn", "",
		 "printf '__attribute__((nonnull(1))) void h(int*);\\n__attribute__((nonnull)) void h2(int*,int*);\\n__attribute__((nonnull(2))) void h3(int,int*);\\nvoid u(void){ h(0); h2(0,0); h3(0,0); int a; h(&a); int*p=0; h(p); }\\n' > {W}/nn.c && "
		 "{ {MCC} -B{B} -I{I} -Wall -c {W}/nn.c -o {W}/nn.o 2>&1 | grep -c 'null where non-null' ; echo OK ; }",
		 "4\nOK\n"},

		{"dangling_else_warn", "",
		 "printf 'int a,b;\\nint f1(void){ if(a) if(b) return 1; else return 2; return 0; }\\nint f2(void){ if(a){ if(b) return 1; else return 2; } return 0; }\\nint f3(void){ if(a) return 1; else if(b) return 2; return 0; }\\nint f4(void){ for(;a;) if(b) return 1; else return 2; return 0; }\\n' > {W}/de.c && "
		 "{ {MCC} -B{B} -I{I} -Wall -c {W}/de.c -o {W}/de.o 2>&1 | grep -c \"ambiguous 'else'\" ; "
		 "{MCC} -B{B} -I{I} -c {W}/de.c -o {W}/de2.o 2>&1 | grep -c 'ambiguous' ; echo OK ; }",
		 "1\n0\nOK\n"},

		{"shift_paren_warn", "",
		 "printf 'int f1(int a,int b,int c){return a + b << c;}\\nint f2(int a,int b,int c){return a - b >> c;}\\nint f3(int a,int b,int c){return a * b << c;}\\nint f4(int a,int b,int c){return (a+b) << c;}\\nint f5(int a,int b,int c){return a << b < c;}\\n' > {W}/sp.c && "
		 "{ {MCC} -B{B} -I{I} -Wall -c {W}/sp.c -o {W}/sp.o 2>&1 | grep -oE \"around '.' inside '..'\" ; "
		 "{MCC} -B{B} -I{I} -c {W}/sp.c -o {W}/sp2.o 2>&1 | grep -c 'inside' ; echo OK ; }",
		 "around '+' inside '<<'\naround '-' inside '>>'\n0\nOK\n"},

		{"logical_paren_warn", "",
		 "printf 'int a,b,c,d;\\nint f1(void){return a && b || c;}\\nint f2(void){return c || a && b;}\\nint f3(void){return a && b || c && d;}\\nint f4(void){return (a && b) || c;}\\nint f5(void){return a || b || c;}\\nint f6(void){return a && b && c;}\\n' > {W}/lp.c && "
		 "{ {MCC} -B{B} -I{I} -Wall -c {W}/lp.c -o {W}/lp.o 2>&1 | grep -c \"around '&&' within '||'\" ; "
		 "{MCC} -B{B} -I{I} -c {W}/lp.c -o {W}/lp2.o 2>&1 | grep -c 'within' ; echo OK ; }",
		 "3\n0\nOK\n"},

		{"switch_deadcode_dce", "",
		 "printf 'extern int undefined_function(void);\\nint g(void){return 3;}\\nint main(void){ if(0){ switch(g()){ case 0: undefined_function(); break; } } return 0; }\\n' > {W}/dd.c && "
		 "printf 'extern int undefined_function(void);\\nint g(void){return 3;}\\nint main(void){ switch(g()){ case 0: undefined_function(); break; default: break; } return 0; }\\n' > {W}/dl.c && "
		 "{ {MCC} -B{B} -nostdinc {W}/dd.c -o {W}/dd 2>/dev/null && echo DEAD_OK || echo DEAD_FAIL; "
		 "{MCC} -B{B} -nostdinc {W}/dl.c -o {W}/dl 2>/dev/null && echo LIVE_OK || echo LIVE_FAIL; }",
		 "DEAD_OK\nLIVE_FAIL\n"},

		{"overflow_const_conv_warn", "",
		 "printf 'char c1 = 300;\\nchar c2 = -200;\\nchar c3 = 200;\\nunsigned char u = 256;\\nshort s = 70000;\\nchar c4 = (char)300;\\nlong l = 300;\\nenum E{A}; enum E e = 300;\\n' > {W}/ov.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/ov.c -o {W}/ov.o 2>&1 | grep -c 'overflow in conversion' ; "
		 "{MCC} -B{B} -I{I} -c {W}/ov.c -o {W}/ov.o 2>&1 | grep -oE \"from 'int' to 'char' changes value from '300' to '44'\" | head -1 ; "
		 "{MCC} -B{B} -I{I} -Wno-overflow -c {W}/ov.c -o {W}/ov2.o 2>&1 | grep -c 'overflow' ; echo OK ; }",
		 "3\nfrom 'int' to 'char' changes value from '300' to '44'\n0\nOK\n"},

		{"ctor_dtor_priority_aot", "os=Darwin",
		 "printf 'extern long write(int,const void*,unsigned long);\\n"
		 "#ifdef C\\n"
		 "__attribute__((constructor(200))) static void c2(void){write(1,\"2\",1);}\\n"
		 "__attribute__((constructor(100))) static void c1(void){write(1,\"1\",1);}\\n"
		 "__attribute__((constructor)) static void cd(void){write(1,\"D\",1);}\\n"
		 "#endif\\n"
		 "#ifdef D\\n"
		 "__attribute__((destructor(200))) static void e2(void){write(1,\"y\",1);}\\n"
		 "__attribute__((destructor(100))) static void e1(void){write(1,\"x\",1);}\\n"
		 "__attribute__((destructor)) static void ed(void){write(1,\"z\",1);}\\n"
		 "#endif\\n"
		 "int main(void){write(1,\"M\",1);return 0;}\\n' > {W}/cd.c && "
		 "{MCC} -B{B} -nostdinc -w -DC -DD {W}/cd.c -o {W}/cdab && "
		 "{MCC} -B{B} -nostdinc -w -DC {W}/cd.c -o {W}/cdco && "
		 "{MCC} -B{B} -nostdinc -w -DD {W}/cd.c -o {W}/cddo && "
		 "printf 'a=%s b=%s c=%s\\n' \"$({W}/cdab)\" \"$({W}/cdco)\" \"$({W}/cddo)\"",
		 "a=12DMzyx b=12DM c=Mzyx\n"},

		{"arm64_register_asm_local_binds_physical_reg", "cpu=arm64",
		 "printf 'int main(void){\\n"
		 "register long a asm(\"x19\")=0x11; register long b asm(\"x20\")=0x22;\\n"
		 "register long c asm(\"x28\")=0x33; register long d asm(\"x30\")=0x44;\\n"
		 "register long e asm(\"x9\")=0x55; long oa,ob,oc,od,oe;\\n"
		 "asm volatile(\"mov %%0, x19\":\"=r\"(oa):\"r\"(a));\\n"
		 "asm volatile(\"mov %%0, x20\":\"=r\"(ob):\"r\"(b));\\n"
		 "asm volatile(\"mov %%0, x28\":\"=r\"(oc):\"r\"(c));\\n"
		 "asm volatile(\"mov %%0, x30\":\"=r\"(od):\"r\"(d));\\n"
		 "asm volatile(\"mov %%0, x9\":\"=r\"(oe):\"r\"(e));\\n"
		 "return !(oa==0x11 && ob==0x22 && oc==0x33 && od==0x44 && oe==0x55);}\\n' > {W}/rv.c && "
		 "{MCC} -B{B} {W}/rv.c -o {W}/rv && {W}/rv && echo BIND_OK",
		 "BIND_OK\n"},

		{"o4_search_does_not_repeat_diagnostics", "cpu=x86_64,os=linux",
		 "printf '_Alignas(16) i3;\\nint main(void){return 0;}\\n' > {W}/od.c && "
		 "XDG_CACHE_HOME={W}/c1 {MCC} -B{B} -I{I} -O1 -c {W}/od.c -o {W}/o1.o 2>{W}/e1.txt; "
		 "XDG_CACHE_HOME={W}/c4 {MCC} -B{B} -I{I} -O13 -c {W}/od.c -o {W}/o4.o 2>{W}/e4.txt; "
		 "printf 'O1=%s O4=%s\\n' "
		 "$(grep -c 'type defaults to' {W}/e1.txt) "
		 "$(grep -c 'type defaults to' {W}/e4.txt)",
		 "O1=1 O4=1"},

		{"fast_math_implies_no_math_errno", "cpu=x86_64,os=linux",
		 "printf 'double sqrt(double);\\ndouble f(double x){return sqrt(x);}\\n' > {W}/fm.c && "
		 "{MCC} -B{B} -I{I} -w -O2 -S {W}/fm.c -o {W}/plain.s && "
		 "{MCC} -B{B} -I{I} -w -O2 -ffast-math -S {W}/fm.c -o {W}/fast.s && "
		 "{MCC} -B{B} -I{I} -w -O2 -ffast-math -fno-fast-math -S {W}/fm.c -o {W}/off.s && "
		 "printf 'plain=%s fast=%s off=%s\\n' "
		 "$(grep -c 'call.*sqrt' {W}/plain.s) $(grep -c 'call.*sqrt' {W}/fast.s) "
		 "$(grep -c 'call.*sqrt' {W}/off.s)",
		 "plain=1 fast=0 off=1"},

		{"intrinsics_no_helper_calls", "cpu=x86_64,os=linux",
		 "printf 'extern void *alloca(__SIZE_TYPE__);\\n"
		 "unsigned s16(unsigned short x){return __builtin_bswap16(x);}\\n"
		 "unsigned s32(unsigned x){return __builtin_bswap32(x);}\\n"
		 "int c1(unsigned x){return __builtin_clz(x);}\\n"
		 "int c2(unsigned long long x){return __builtin_ctzll(x);}\\n"
		 "int c3(int x){return __builtin_ffs(x);}\\n"
		 "int c4(unsigned x){return __builtin_popcount(x);}\\n"
		 "int c5(unsigned x){return __builtin_parity(x);}\\n"
		 "int c6(int x){return __builtin_clrsb(x);}\\n"
		 "int c7(double x){return __builtin_signbit(x);}\\n"
		 "long long o1(long long a,long long b){long long r;return __builtin_add_overflow(a,b,&r)?0:r;}\\n"
		 "int a1(int *p,int v){return __atomic_load_n(p,5)+__atomic_fetch_add(p,v,5);}\\n"
		 "int a2(int *p,int *e,int v){__atomic_store_n(p,v,5);return __atomic_compare_exchange_n(p,e,v,0,5,5);}\\n"
		 "void *al(unsigned long n){return alloca(n);}\\n' > {W}/ni.c && "
		 "{MCC} -B{B} -I{I} -w -O2 -c {W}/ni.c -o {W}/ni.o && "
		 "readelf -sW {W}/ni.o | awk '$7==\"UND\" && $8!=\"\"{print $8}' | sort -u",
		 ""},

		{"version_script_hides", "cpu=x86_64,os=linux",
		 "printf 'int public_fn(void){return 1;}\\nint private_fn(void){return 2;}\\n' > {W}/vs.c && "
		 "printf '{ global: public_fn; local: *; };\\n' > {W}/vs.map && "
		 "{MCC} -B{B} -I{I} -shared -Wl,--version-script={W}/vs.map {W}/vs.c -o {W}/vs.so && "
		 "readelf --dyn-syms -W {W}/vs.so | grep -oE 'public_fn|private_fn' | sort -u && "
		 "{MCC} -B{B} -I{I} -shared {W}/vs.c -o {W}/vs2.so && "
		 "readelf --dyn-syms -W {W}/vs2.so | grep -oE 'public_fn|private_fn' | sort -u",
		 "public_fn\nprivate_fn\npublic_fn\n"},

		{"shared_dyn_soname", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -shared -Wl,-soname,libfoo.so.1 {D}/lib.c -o {W}/x.so && "
		 "readelf -h {W}/x.so | grep -oE 'DYN' && readelf -d {W}/x.so | grep -oE 'libfoo\\.so\\.1'",
		 "DYN\nlibfoo.so.1\n"},

		{"relocatable_partial_link", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -r {D}/lib.c {D}/sec.c -o {W}/m.o && "
		 "readelf -h {W}/m.o | grep -oE 'REL' && "
		 "nm {W}/m.o | grep -oE 'exported_fn|second_fn|placed_var' | sort -u",
		 "REL\nexported_fn\nplaced_var\nsecond_fn\n"},

		{"strip_symbols", "cpu=x86_64,os=linux",
		 "printf 'int f(int x){return x+1;}\\nint main(void){return f(0);}\\n' > {W}/st.c && "
		 "{MCC} -B{B} -I{I} -Werror -s {W}/st.c -o {W}/st && "
		 "readelf -S {W}/st | grep -c '\\.symtab'",
		 "0\n"},

		{"fpic_pie_dyn", "cpu=x86_64,os=linux",
		 "printf 'extern int g; int f(void){return g;}\\nint g=41; int main(void){return f()-g+1;}\\n' > {W}/pc.c && "
		 "{MCC} -B{B} -I{I} -fPIC -pie {W}/pc.c -o {W}/pc && "
		 "readelf -h {W}/pc | grep -oE 'DYN' && readelf -d {W}/pc | grep -c TEXTREL",
		 "DYN\n0\n"},

		{"fno_pic_exec", "cpu=x86_64,os=linux",
		 "printf 'int main(void){return 0;}\\n' > {W}/np.c && "
		 "{MCC} -B{B} -I{I} -fno-pic -no-pie {W}/np.c -o {W}/np && readelf -h {W}/np | grep -oE 'EXEC'",
		 "EXEC\n"},

		{"macho_framework_link", "os=Darwin",
		 "printf '#include <CoreFoundation/CoreFoundation.h>\\nint main(void){ CFStringRef s=CFStringCreateWithCString(0,\"ok\",0x08000100); long n=CFStringGetLength(s); CFRelease(s); return n==2?0:1; }\\n' > {W}/fw.c && "
		 "{MCC} -B{B} -I{I} -framework CoreFoundation {W}/fw.c -o {W}/fw && {W}/fw && echo RAN",
		 "RAN\n"},

		{"macho_framework_run", "os=Darwin",
		 "printf '#include <CoreFoundation/CoreFoundation.h>\\nint main(void){ CFStringRef s=CFStringCreateWithCString(0,\"ok\",0x08000100); long n=CFStringGetLength(s); CFRelease(s); return n==2?0:1; }\\n' > {W}/fwr.c && "
		 "{MCC} -B{B} -I{I} -framework CoreFoundation -run {W}/fwr.c && echo RAN",
		 "RAN\n"},

		{"macho_framework_dashF", "os=Darwin",
		 "mkdir -p {W}/fw/MyKit.framework/Headers && "
		 "printf '#define MYKIT_OK 1\\n' > {W}/fw/MyKit.framework/Headers/MyKit.h && "
		 "printf '#include <MyKit/MyKit.h>\\nint main(void){ return MYKIT_OK?0:1; }\\n' > {W}/mk.c && "
		 "{MCC} -B{B} -I{I} -F{W}/fw {W}/mk.c -o {W}/mk && {W}/mk && echo RAN",
		 "RAN\n"},

		{"macho_extern_tls_unsupported", "os=Darwin",
		 "printf 'extern __thread int c;\\nint main(void){ return c; }\\n' > {W}/et.c && "
		 "{MCC} -B{B} -I{I} {W}/et.c -o {W}/et 2>&1 | grep -oE 'external thread-local .* unsupported' | head -1",
		 "external thread-local '_c' is unsupported\n"},

		{"vector_size_elementwise", "",
		 "printf 'typedef double v2 __attribute__((vector_size(16)));\\n"
		 "typedef int v4 __attribute__((vector_size(16)));\\n"
		 "int main(void){ v2 a={1,2},b={3,4},c=a+b; v4 x={1,2,3,4},y={4,3,2,1},e=x<y;\\n"
		 "return !(c[0]==4&&c[1]==6&&e[0]==-1&&e[3]==0&&x[1]==2\\n"
		 "&&sizeof(v2)==16&&__alignof__(v2)==16); }\\n' > {W}/vs.c && "
		 "{MCC} -B{B} -I{I} {W}/vs.c -o {W}/vs && {W}/vs && echo VEC_OK",
		 "VEC_OK\n"},

		{"visibility_attribute", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -c {D}/vis.c -o {W}/v.o && "
		 "readelf -s {W}/v.o | grep -E 'hidden_att|shown_one|plain_one' | awk '{print $6, $8}' | sort",
		 "DEFAULT plain_one\nDEFAULT shown_one\nHIDDEN hidden_att\n"},

		{"fvisibility_hidden_default_wins", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -fvisibility=hidden -c {D}/vis.c -o {W}/vh.o && "
		 "readelf -s {W}/vh.o | grep -E 'hidden_att|shown_one|plain_one' | awk '{print $6, $8}' | sort",
		 "DEFAULT shown_one\nHIDDEN hidden_att\nHIDDEN plain_one\n"},

		{"section_attribute", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -c {D}/sec.c -o {W}/s.o && readelf -S {W}/s.o | grep -oE '\\.mysec' | head -1",
		 ".mysec\n"},

		{"leading_underscore", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -fleading-underscore -c {D}/lib.c -o {W}/lu.o && nm {W}/lu.o | grep -oE '_exported_fn'",
		 "_exported_fn\n"},

		{"rdynamic_exports_main", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -rdynamic {D}/hello.c -o {W}/hr && readelf --dyn-syms {W}/hr | grep -cE ' main$'",
		 "1\n"},

		{"function_data_sections_accepted", "os!=WIN32",
		 "{MCC} -B{B} -I{I} -ffunction-sections -fdata-sections -c {D}/lib.c -o {W}/fsd.o && echo OK",
		 "OK\n"},

		{"stack_protector_on", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -fstack-protector-all -c {D}/sp.c -o {W}/sp.o && nm {W}/sp.o | grep -oE '__stack_chk_fail' | head -1",
		 "__stack_chk_fail\n"},

		{"stack_protector_off", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -fno-stack-protector -c {D}/sp.c -o {W}/sp2.o && nm {W}/sp2.o | grep -c __stack_chk_fail",
		 "0\n"},

		{"O1_libm_builtin_fold", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double sqrt(double);\\ndouble f(void){return sqrt(2.0);}\\n' > {W}/bf.c && "
		 "{MCC} -B{B} -I{I} -O0 -c {W}/bf.c -o {W}/bf0.o && "
		 "{MCC} -B{B} -I{I} -O1 -c {W}/bf.c -o {W}/bf1.o && "
		 "readelf -r {W}/bf0.o | grep -c sqrt && readelf -r {W}/bf1.o | grep -c sqrt",
		 "1\n0\n"},

		{"foldmath_O0_O1_equal", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double sin(double);double cos(double);double exp(double);int okc(double a,double b){double d=a-b;if(d<0)d=-d;return d<1e-9;}int main(void){if(!okc(sin(0.5),0.47942553860420301))return 1;if(!okc(cos(1.0),0.54030230586813977))return 2;if(!okc(exp(1.0),2.7182818284590451))return 3;if(!okc(sin(100.0),-0.50636564110975879))return 4;return 0;}\\n' > {W}/fm.c && "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -c {W}/fm.c -o {W}/fm0.o && "
		 "{MCC} -B{B} -I{I} -ffold-math -O1 -c {W}/fm.c -o {W}/fm1.o && "
		 "readelf -r {W}/fm0.o | grep -c sin ; readelf -r {W}/fm1.o | grep -c sin ; "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -run {W}/fm.c && echo O0OK ; "
		 "{MCC} -B{B} -I{I} -ffold-math -O1 -run {W}/fm.c && echo O1OK",
		 "0\n0\nO0OK\nO1OK\n"},

		{"foldmath_off_keeps_call", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double sin(double);int main(void){return sin(0.0)==0.0?0:1;}\\n' > {W}/fmc.c && "
		 "{MCC} -B{B} -I{I} -O0 -c {W}/fmc.c -o {W}/fmc.o && readelf -r {W}/fmc.o | grep -c sin",
		 "1\n"},

		{"foldmath_more_funcs", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double log(double);double log2(double);double log10(double);double tan(double);double pow(double,double);double sinh(double);double cosh(double);double tanh(double);int okc(double a,double b){double d=a-b;if(d<0)d=-d;return d<1e-9;}int main(void){if(!okc(log(2.0),0.69314718055994531))return 1;if(!okc(log2(8.0),3.0))return 2;if(!okc(log10(1000.0),3.0))return 3;if(!okc(tan(1.0),1.5574077246549023))return 4;if(!okc(pow(2.0,10.0),1024.0))return 5;if(!okc(pow(2.0,-3.0),0.125))return 6;if(!okc(pow(5.0,0.0),1.0))return 7;if(!okc(pow(1.0,99.0),1.0))return 8;if(!okc(sinh(1.0),1.1752011936438014))return 9;if(!okc(cosh(1.0),1.5430806348152437))return 10;if(!okc(tanh(0.5),0.46211715726000974))return 11;return 0;}\\n' > {W}/fm2.c && "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -c {W}/fm2.c -o {W}/fm2_0.o && "
		 "{MCC} -B{B} -I{I} -ffold-math -O1 -c {W}/fm2.c -o {W}/fm2_1.o && "
		 "readelf -r {W}/fm2_0.o | grep -Ec 'log|tan|pow|sinh|cosh|tanh' ; "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -run {W}/fm2.c && echo O0OK ; "
		 "{MCC} -B{B} -I{I} -ffold-math -O1 -run {W}/fm2.c && echo O1OK",
		 "0\nO0OK\nO1OK\n"},

		{"foldmath_must_not_fold", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double log(double);double tan(double);double pow(double,double);double a(void){return log(-1.0);}double b(void){return tan(2000000.0);}double c(void){return pow(2.0,0.5);}double d(void){return pow(-2.0,3.0);}\\n' > {W}/fm3.c && "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -c {W}/fm3.c -o {W}/fm3.o && "
		 "readelf -r {W}/fm3.o | grep -c log ; readelf -r {W}/fm3.o | grep -c tan ; readelf -r {W}/fm3.o | grep -c pow",
		 "1\n1\n2\n"},

		{"superopt_search_O4", "cpu=x86_64,os=linux,optimizer",
		 "printf 'int fib(int n){return n<2?n:fib(n-1)+fib(n-2);}int main(void){return fib(10);}\\n' > {W}/so.c && "
		 "XDG_CACHE_HOME={W}/cache {MCC} -B{B} -I{I} -O13 -c {W}/so.c -o {W}/so.o && "
		 "XDG_CACHE_HOME={W}/cache {MCC} -B{B} -I{I} -O13 -c {W}/so.c -o {W}/so_warm.o && "
		 "test $(wc -c < {W}/so_warm.o) -le $(wc -c < {W}/so.o) && echo WARMOK ; "
		 "{MCC} -B{B} -I{I} {W}/so.o -o {W}/so && {W}/so ; echo rc=$? ; "
		 "{MCC} -B{B} -I{I} --no-embed-jit -O0 -c {W}/so.c -o {W}/so2.o && echo FLAGOK",
		 "WARMOK\nrc=55\nFLAGOK\n"},

		{"cvtsi2f_pxor_depbreak", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double f(int x){return (double)x;}int main(void){return "
		 "(int)f(-7)==-7?0:1;}\\n' > {W}/pxb.c && "
		 "{MCC} -B{B} -I{I} -O0 -S {W}/pxb.c -o {W}/pxb0.s && "
		 "{MCC} -B{B} -I{I} -O1 -S {W}/pxb.c -o {W}/pxb1.s && "
		 "grep -c pxor {W}/pxb0.s ; grep -c pxor {W}/pxb1.s ; "
		 "{MCC} -B{B} -I{I} -O1 -run {W}/pxb.c && echo RUNOK",
		 "0\n1\nRUNOK\n"},

		{"u64_to_double_inline", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double u2d(unsigned long x){return (double)x;}int "
		 "main(void){unsigned long v=0xFFFFFFFFFFFFFFFFUL;return "
		 "u2d(v)==18446744073709551616.0?0:1;}\\n' > {W}/ud.c && "
		 "{MCC} -B{B} -I{I} -O0 -S {W}/ud.c -o {W}/ud0.s && "
		 "{MCC} -B{B} -I{I} -O1 -S {W}/ud.c -o {W}/ud1.s && "
		 "grep -c __floatundidf {W}/ud0.s ; grep -c __floatundidf {W}/ud1.s ; "
		 "grep -cw cvtsi2sd {W}/ud1.s ; "
		 "{MCC} -B{B} -I{I} -O1 -run {W}/ud.c && echo RUNOK",
		 "1\n0\n2\nRUNOK\n"},

		{"mul_const_lea_strength", "cpu=x86_64,os=linux,optimizer",
		 "printf 'int m3(int x){return x*3;}int m5(int x){return x*5;}int "
		 "m9(int x){return x*9;}int m7(int x){return x*7;}int main(void){"
		 "return m3(7)==21&&m5(-3)==-15&&m9(4)==36&&m7(3)==21?0:1;}\\n' "
		 "> {W}/ml.c && "
		 "{MCC} -B{B} -I{I} -O0 -S {W}/ml.c -o {W}/ml0.s && "
		 "{MCC} -B{B} -I{I} -O1 -S {W}/ml.c -o {W}/ml1.s && "
		 "grep -cw lea {W}/ml0.s ; grep -cw lea {W}/ml1.s ; "
		 "grep -cw imul {W}/ml1.s ; "
		 "{MCC} -B{B} -I{I} -O1 -run {W}/ml.c && echo RUNOK",
		 "0\n3\n1\nRUNOK\n"},

		{"fneg_inreg_xorps", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double nd(double x){return -x;}float nf(float x){return "
		 "-x;}int main(void){return nd(2.0)==-2.0&&nf(3.0f)==-3.0f?0:1;}\\n' "
		 "> {W}/fn.c && "
		 "{MCC} -B{B} -I{I} -O0 -S {W}/fn.c -o {W}/fn0.s && "
		 "{MCC} -B{B} -I{I} -O1 -S {W}/fn.c -o {W}/fn1.s && "
		 "grep -cE 'xorps|xorpd' {W}/fn0.s ; grep -cE 'xorps|xorpd' {W}/fn1.s ; "
		 "{MCC} -B{B} -I{I} -O1 -run {W}/fn.c && echo RUNOK",
		 "0\n2\nRUNOK\n"},

		{"embed_jit_manifest", "cpu=x86_64,os=linux,optimizer",
		 "printf 'int main(void){return 0;}\\n' > {W}/mf.c && "
		 "XDG_CACHE_HOME={W}/mfc {MCC} -B{B} -I{I} -O13 -v --embed-jit --jit-functions main,helper --jit-max-duration 120 -c {W}/mf.c -o {W}/mf.o 2>&1 | grep 'embed-jit manifest' ; "
		 "XDG_CACHE_HOME={W}/mfc {MCC} -B{B} -I{I} -O13 -v --embed-jit --jit-functions=main,helper --jit-max-duration=120 -c {W}/mf.c -o {W}/mf.o 2>&1 | grep 'embed-jit manifest' ; "
		 "XDG_CACHE_HOME={W}/mfc {MCC} -B{B} -I{I} -O13 -v --no-embed-jit -c {W}/mf.c -o {W}/mf2.o 2>&1 | grep -c 'embed-jit manifest'",
		 "embed-jit manifest: functions=main,helper max-duration=120s\nembed-jit manifest: functions=main,helper max-duration=120s\n0\n"},

		{"bitflag_detect", "cpu=x86_64,os=linux,optimizer",
		 "printf 'int classify(int x){if(x==1)return 10;else if(x==3)return 30;else if(x==5)return 50;else if(x==7)return 70;return 0;}int two(int y){if(y==2)return 1;if(y==4)return 2;return 0;}int main(void){return classify(5)+two(4);}\\n' > {W}/bf.c && "
		 "{MCC} -fdump-bitflag -B{B} -I{I} -O1 -c {W}/bf.c -o {W}/bf.o 2>&1 | grep bitflag ; "
		 "{MCC} -B{B} -I{I} -O1 {W}/bf.c -o {W}/bfx && {W}/bfx ; echo rc=$?",
		 "bitflag-candidate: classify cluster=4\nrc=52\n"},

		{"ast_poison_lowering", "optimizer",
		 "printf 'int dse(int x){int a;a=5;a=7;a=x+1;return a;}\\nint sccp(int x){if(1)return x*2;else return -999;}\\nint bf(int f){int r=0;if(f&1)r+=1;if(f&2)r+=2;if(f&4)r+=4;return r;}\\nint main(void){int ok=dse(10)==11&&sccp(3)==6&&bf(5)==5;return ok?0:1;}\\n' > {W}/pz.c && "
		 "{MCC} -B{B} -O1 {W}/pz.c -o {W}/pz && {W}/pz && echo o1ok && "
		 "{MCC} -B{B} -O2 {W}/pz.c -o {W}/pz2 && {W}/pz2 && echo o2ok && "
		 "{MCC} -fdump-replay -B{B} -O1 -c {W}/pz.c -o {W}/pz.o 2>&1 | grep -oE 'ast-dse|ast-sccp|Poison' | sort -u | tr '\\n' ','",
		 "o1ok\no2ok\nPoison,ast-dse,ast-sccp,"},

		{"bitflag_transform", "cpu=x86_64,os=linux,optimizer",
		 "printf 'int g;int f(int x){if(x==1||x==3||x==5||x==7||x==9)return 1;return 0;}int c(int x){if(x==2)g=4;else if(x==4)g=4;else if(x==6)g=4;else if(x==8)g=4;else if(x==10)g=4;else g=7;return g;}int main(void){int k[10]={-1,0,1,2,3,64,65,7,9,10},s=0;for(int i=0;i<10;i++)s+=f(k[i])+c(k[i]);return s;}\\n' > {W}/bft.c && "
		 "{MCC} -fdump-replay -ftree-switch-conversion -B{B} -I{I} -O1 -c {W}/bft.c -o {W}/bft.o 2>&1 | grep -c 'ast-bitflag' ; "
		 "{MCC} -ftree-switch-conversion -B{B} -I{I} -O1 {W}/bft.c -o {W}/bft && {W}/bft ; echo rc=$? ; "
		 "{MCC} -ftree-switch-conversion -B{B} -I{I} -O3 {W}/bft.c -o {W}/bft3 && {W}/bft3 ; echo rc=$? ; "
		 "{MCC} -B{B} -I{I} -O1 {W}/bft.c -o {W}/bft0 && {W}/bft0 ; echo rc=$?",
		 "2\nrc=68\nrc=68\nrc=68\n"},

		{"bitflag_ifne", "cpu=x86_64,os=linux,optimizer",
		 "printf 'int ni(int x){if(x!=1){if(x!=3){if(x!=5){if(x!=7){if(x!=9){return 1;}}}}}return 0;}int main(void){int k[10]={-1,0,1,2,3,64,65,7,9,10},s=0;for(int i=0;i<10;i++)s+=ni(k[i]);return s;}\\n' > {W}/bfn.c && "
		 "{MCC} -fdump-replay -ftree-switch-conversion -B{B} -I{I} -O1 -c {W}/bfn.c -o {W}/bfn.o 2>&1 | grep -c 'ast-bitflag' ; "
		 "{MCC} -ftree-switch-conversion -B{B} -I{I} -O1 {W}/bfn.c -o {W}/bfn && {W}/bfn ; echo rc=$? ; "
		 "{MCC} -ftree-switch-conversion -B{B} -I{I} -O3 {W}/bfn.c -o {W}/bfn3 && {W}/bfn3 ; echo rc=$? ; "
		 "{MCC} -B{B} -I{I} -O1 {W}/bfn.c -o {W}/bfn0 && {W}/bfn0 ; echo rc=$?",
		 "1\nrc=6\nrc=6\nrc=6\n"},

		{"slice_eligible_set", "cpu=x86_64,os=linux,optimizer",






		 "printf 'int f(int a){int i,s=0;for(i=1;i<40;i++)s+=i/7+a%%7;return s;}int main(void){return f(3)&1;}\\n' > {W}/el.c && "


		 "rm -rf {W}/elc {W}/el.txt && mkdir -p {W}/elc && "
		 "XDG_CACHE_HOME={W}/elc MCC_SLICE_DUMP={W}/el.txt MCC_DEV=1 "
		 "{MCC} -fopt-slice -fno-divmagic -fno-tree-vrp -B{B} -I{I} -O2 -c {W}/el.c -o {W}/el.o && "
		 "awk 'NR==1{ gv=$2; ev=$3; sub(/^g=/,\"\",gv); sub(/^e=/,\"\",ev); "
		 "           if (index($3,\"e=\")!=1) { print \"NO_ELIGIBLE\"; exit } "
		 "           if (gv == ev) print \"SAME\"; else print \"WIDER\"; "
		 "           print \"HAS_ELIGIBLE\" }' {W}/el.txt",
		 "WIDER\nHAS_ELIGIBLE\n"},

		{"pre_diamond", "cpu=x86_64,os=linux,optimizer",



		 "printf 'int f(int a,int b,int c){int x=0,y=0,r;if(c){x=a+b;}else{y=a+b;}r=a+b;return x+y+r;}int main(void){return f(3,4,1)+f(3,4,0);}\\n' > {W}/pre.c && "
		 "{MCC} -fno-tree-pre -B{B} -I{I} -O2 -c {W}/pre.c -o {W}/pre.off.o && "
		 "{MCC} -ftree-pre -B{B} -I{I} -O2 -c {W}/pre.c -o {W}/pre.on.o && "
		 "( cmp -s {W}/pre.off.o {W}/pre.on.o && echo SAME || echo DIFFER ) ; "
		 "{MCC} -ftree-pre -B{B} -I{I} -O2 {W}/pre.c -o {W}/pre2 && {W}/pre2 ; echo rc=$? ; "
		 "{MCC} -ftree-pre -B{B} -I{I} -O3 {W}/pre.c -o {W}/pre3 && {W}/pre3 ; echo rc=$? ; "
		 "{MCC} -B{B} -I{I} -O2 {W}/pre.c -o {W}/pre0 && {W}/pre0 ; echo rc=$?",
		 "DIFFER\nrc=28\nrc=28\nrc=28\n"},

		{"perfn_inproc", "cpu=x86_64,os=linux,optimizer",




		 "printf 'static int chunk(int v,int k){int a=v*k+3;int b=a^(v<<2);int c=b+(k*7);int d=c^(a>>1);int e=d+(b*5);int f=e^(c<<1);return (f+a+b+c+d+e)&0xffff;}static int driver(int seed){int r=seed;int i;for(i=0;i<4;i++){r=chunk(r,3)+chunk(r,5);r^=chunk(r,7)+chunk(r,9);r&=0xffff;}return r;}int main(void){return driver(17)&0x7f;}\\n' > {W}/pfi.c && "
		 "XDG_CACHE_HOME={W}/pfic {MCC} -fno-inline-functions -B{B} -I{I} -O3 -c {W}/pfi.c -o {W}/pfi.off.o && "
		 "XDG_CACHE_HOME={W}/pfic MCC_DEV=1 {MCC} -fno-inline-functions -fopt-perfn-inproc -B{B} -I{I} -O3 -c {W}/pfi.c -o {W}/pfi.on.o && "
		 "( cmp -s {W}/pfi.off.o {W}/pfi.on.o && echo SAME || echo DIFFER ) ; "
		 "XDG_CACHE_HOME={W}/pfic {MCC} -B{B} -I{I} -O3 -c {W}/pfi.c -o {W}/pfi.ni.off.o && "
		 "XDG_CACHE_HOME={W}/pfic MCC_DEV=1 {MCC} -fopt-perfn-inproc -B{B} -I{I} -O3 -c {W}/pfi.c -o {W}/pfi.ni.on.o && "
		 "( cmp -s {W}/pfi.ni.off.o {W}/pfi.ni.on.o && echo SAME || echo DIFFER ) ; "
		 "XDG_CACHE_HOME={W}/pfic MCC_DEV=1 {MCC} -fno-inline-functions -fopt-perfn-inproc -B{B} -I{I} -O3 {W}/pfi.c -o {W}/pfi.on && {W}/pfi.on ; echo rc=$? ; "
		 "XDG_CACHE_HOME={W}/pfic {MCC} -B{B} -I{I} -O0 {W}/pfi.c -o {W}/pfi.o0 && {W}/pfi.o0 ; echo rc=$?",
		 "DIFFER\nSAME\nrc=80\nrc=80\n"},

		{"perfn_search", "cpu=x86_64,os=linux,optimizer",
		 "printf 'static int sq(int x){return x*x;}static int cube(int x){return x*x*x;}int main(void){int s=0;for(int i=0;i<8;i++)s+=sq(i)+cube(i);return s;}\\n' > {W}/pfs.c && "
		 "XDG_CACHE_HOME={W}/pfsc MCC_AST_PERFN=1 {MCC} -B{B} -I{I} -O13 -c {W}/pfs.c -o {W}/pfs.o && "
		 "{MCC} -B{B} -I{I} {W}/pfs.o -o {W}/pfs && {W}/pfs ; echo rc=$?",
		 "rc=156\n"},

		{"per_fn_config", "cpu=x86_64,os=linux,optimizer",
		 "printf 'static int sq(int x){return x*x;}int main(void){int s=0;for(int i=0;i<10;i++)s+=sq(i);return s;}\\n' > {W}/pf.c && "
		 "MCC_AST_FN_CONFIG='main=1;sq=1' {MCC} -freemit-templates -B{B} -I{I} -O3 -c {W}/pf.c -o {W}/pf1.o && "
		 "MCC_AST_FN_CONFIG='main=7;sq=7' {MCC} -freemit-templates -B{B} -I{I} -O3 -c {W}/pf.c -o {W}/pf7.o && "
		 "( cmp -s {W}/pf1.o {W}/pf7.o && echo SAME || echo DIFFER ) ; "
		 "{MCC} -B{B} -I{I} -O3 {W}/pf.c -o {W}/pf && {W}/pf ; echo rc=$?",
		 "DIFFER\nrc=29\n"},

		{"ast_cost_report", "cpu=x86_64,os=linux,optimizer",
		 "printf 'static int inner(int x){int s=0;for(int i=0;i<x;i++)for(int j=0;j<x;j++)s+=i*j;return s;}\\nint main(void){return inner(5);}\\n' > {W}/cost.c && "
		 "{MCC} -fdump-cost -B{B} -I{I} -O1 -c {W}/cost.c -o {W}/cost.o 2>&1 | grep '^ast-cost' | awk '{print $2, $4}' | sort",
		 "inner loopdepth=2\nmain loopdepth=0\n"},

		{"clear_cache_and_jit_flags", "cpu=x86_64,os=linux,optimizer",
		 "printf 'int main(void){return 0;}\\n' > {W}/cc.c && "
		 "XDG_CACHE_HOME={W}/cch {MCC} -B{B} -I{I} -O13 -c {W}/cc.c -o {W}/cc.o && "
		 "ls {W}/cch/mcc | grep -c '[.]ck$' ; "
		 "XDG_CACHE_HOME={W}/cch {MCC} --clear-cache >/dev/null && test ! -d {W}/cch/mcc && echo CLEARED ; "
		 "{MCC} -B{B} -I{I} --jit-max-duration 30 --jit-functions main,foo -c {W}/cc.c -o {W}/cc2.o && echo JITFLAGS",
		 "1\nCLEARED\nJITFLAGS\n"},

		{"foldmath_invtrig", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double atan(double);double asin(double);double acos(double);double atan2(double,double);double cbrt(double);double hypot(double,double);int okc(double a,double b){double d=a-b;if(d<0)d=-d;return d<1e-9;}int main(void){if(!okc(atan(0.5),0.46364760900080609))return 1;if(!okc(asin(0.5),0.52359877559829887))return 2;if(!okc(acos(0.5),1.0471975511965979))return 3;if(!okc(asin(1.0),1.5707963267948966))return 4;if(!okc(acos(-1.0),3.1415926535897931))return 5;if(!okc(atan2(1.0,1.0),0.78539816339744831))return 6;if(!okc(atan2(1.0,0.0),1.5707963267948966))return 7;if(!okc(cbrt(27.0),3.0))return 8;if(!okc(cbrt(-8.0),-2.0))return 9;if(!okc(hypot(3.0,4.0),5.0))return 10;if(!okc(hypot(5.0,12.0),13.0))return 11;return 0;}\\n' > {W}/fmi.c && "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -c {W}/fmi.c -o {W}/fmi_0.o && "
		 "{MCC} -B{B} -I{I} -ffold-math -O1 -c {W}/fmi.c -o {W}/fmi_1.o && "
		 "readelf -r {W}/fmi_0.o | grep -Ec 'atan|asin|acos|cbrt|hypot' ; "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -run {W}/fmi.c && echo O0OK ; "
		 "{MCC} -B{B} -I{I} -ffold-math -O1 -run {W}/fmi.c && echo O1OK",
		 "0\nO0OK\nO1OK\n"},

		{"foldmath_invtrig_must_not", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double asin(double);double acos(double);double hypot(double,double);double a(void){return asin(2.0);}double b(void){return acos(-2.0);}double c(void){double inf=1e308*10.0;return hypot(inf,1.0);}\\n' > {W}/fmi3.c && "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -c {W}/fmi3.c -o {W}/fmi3.o && "
		 "readelf -r {W}/fmi3.o | grep -c asin ; readelf -r {W}/fmi3.o | grep -c acos ; readelf -r {W}/fmi3.o | grep -c hypot",
		 "1\n1\n1\n"},

		{"foldmath_more2", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double exp2(double);double expm1(double);double log1p(double);double asinh(double);double acosh(double);double atanh(double);int okc(double a,double b){double d=a-b;if(d<0)d=-d;return d<1e-9;}int okr(double a,double b){double d=a-b;if(d<0)d=-d;double e=b<0?-b:b;return d<=1e-9*e;}int main(void){if(!okc(exp2(10.0),1024.0))return 1;if(!okc(exp2(0.5),1.4142135623730951))return 2;if(!okc(exp2(-3.0),0.125))return 3;if(!okc(expm1(1.0),1.7182818284590451))return 4;if(!okr(expm1(1e-10),1.00000000005e-10))return 5;if(!okr(log1p(1e-10),9.9999999995e-11))return 6;if(!okc(log1p(0.5),0.40546510810816438))return 7;if(!okc(asinh(2.0),1.4436354751788103))return 8;if(!okc(asinh(-0.5),-0.48121182505960347))return 9;if(!okc(acosh(2.0),1.3169578969248166))return 10;if(!okc(acosh(1.0),0.0))return 11;if(!okc(atanh(0.5),0.54930614433405489))return 12;if(!okc(atanh(-0.9),-1.4722194895832204))return 13;return 0;}\\n' > {W}/fm4.c && "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -c {W}/fm4.c -o {W}/fm4_0.o && "
		 "{MCC} -B{B} -I{I} -ffold-math -O1 -c {W}/fm4.c -o {W}/fm4_1.o && "
		 "readelf -r {W}/fm4_0.o | grep -Ec 'exp2|expm1|log1p|asinh|acosh|atanh' ; "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -run {W}/fm4.c && echo O0OK ; "
		 "{MCC} -B{B} -I{I} -ffold-math -O1 -run {W}/fm4.c && echo O1OK",
		 "0\nO0OK\nO1OK\n"},

		{"foldmath_more2_must_not", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double log1p(double);double acosh(double);double atanh(double);double a(void){return log1p(-2.0);}double b(void){return acosh(0.5);}double c(void){return atanh(2.0);}\\n' > {W}/fm4n.c && "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -c {W}/fm4n.c -o {W}/fm4n.o && "
		 "readelf -r {W}/fm4n.o | grep -c log1p ; readelf -r {W}/fm4n.o | grep -c acosh ; readelf -r {W}/fm4n.o | grep -c atanh",
		 "1\n1\n1\n"},

		{"foldmath_erf", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double erf(double);double erfc(double);int okc(double a,double b){double d=a-b;if(d<0)d=-d;return d<1e-9;}int okr(double a,double b){double d=a-b;if(d<0)d=-d;double e=b<0?-b:b;return d<=2e-15*e;}int main(void){if(!okc(erf(0.0),0.0))return 1;if(!okc(erf(0.5),0.52049987781304652))return 2;if(!okc(erf(1.0),0.84270079294971489))return 3;if(!okc(erf(2.0),0.99532226501895271))return 4;if(!okc(erf(-1.0),-0.84270079294971489))return 5;if(!okr(erf(1e-10),1.1283791670955126e-10))return 6;if(!okc(erfc(0.0),1.0))return 7;if(!okc(erfc(1.0),0.15729920705028513))return 8;if(!okc(erfc(3.0),2.2090496998585438e-05))return 9;if(!okr(erfc(10.0),2.0884875837625449e-45))return 10;return 0;}\\n' > {W}/fe.c && "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -c {W}/fe.c -o {W}/fe_0.o && "
		 "{MCC} -B{B} -I{I} -ffold-math -O1 -c {W}/fe.c -o {W}/fe_1.o && "
		 "readelf -r {W}/fe_0.o | grep -c erf ; "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -run {W}/fe.c && echo O0OK ; "
		 "{MCC} -B{B} -I{I} -ffold-math -O1 -run {W}/fe.c && echo O1OK",
		 "0\nO0OK\nO1OK\n"},

		{"foldmath_erf_must_not", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double erf(double);double erfc(double);double a(double v){return erf(v);}double b(void){return erfc(0.0/0.0);}\\n' > {W}/fen.c && "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -c {W}/fen.c -o {W}/fen.o && "
		 "readelf -r {W}/fen.o | grep -cw erf ; readelf -r {W}/fen.o | grep -cw erfc",
		 "1\n1\n"},

		{"foldmath_gamma", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double lgamma(double);double tgamma(double);int okr(double a,double b){double d=a-b;if(d<0)d=-d;double e=b<0?-b:b;return d<=1e-12*(e>1?e:1);}int main(void){if(!okr(lgamma(1.0),0.0))return 1;if(!okr(lgamma(2.0),0.0))return 2;if(!okr(lgamma(0.5),0.57236494292470008))return 3;if(!okr(lgamma(5.0),3.1780538303479458))return 4;if(!okr(lgamma(10.0),12.801827480081469))return 5;if(!okr(lgamma(0.1),2.252712651734206))return 6;if(!okr(lgamma(100.0),359.1342053695754))return 7;if(!okr(tgamma(1.0),1.0))return 8;if(!okr(tgamma(2.0),1.0))return 9;if(!okr(tgamma(5.0),24.0))return 10;if(!okr(tgamma(0.5),1.7724538509055161))return 11;if(!okr(tgamma(3.5),3.3233509704478426))return 12;if(!okr(tgamma(10.0),362880.0))return 13;return 0;}\\n' > {W}/fg.c && "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -c {W}/fg.c -o {W}/fg_0.o && "
		 "{MCC} -B{B} -I{I} -ffold-math -O1 -c {W}/fg.c -o {W}/fg_1.o && "
		 "readelf -r {W}/fg_0.o | grep -Ec 'lgamma|tgamma' ; "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -run {W}/fg.c && echo O0OK ; "
		 "{MCC} -B{B} -I{I} -ffold-math -O1 -run {W}/fg.c && echo O1OK",
		 "0\nO0OK\nO1OK\n"},

		{"foldmath_gamma_must_not", "cpu=x86_64,os=linux,optimizer",
		 "printf 'double lgamma(double);double tgamma(double);double a(void){return tgamma(0.0);}double b(void){return lgamma(-1.0);}double c(void){return tgamma(-2.5);}double d(void){return lgamma(-0.5);}\\n' > {W}/fgn.c && "
		 "{MCC} -B{B} -I{I} -ffold-math -O0 -c {W}/fgn.c -o {W}/fgn.o && "
		 "readelf -r {W}/fgn.o | grep -cw tgamma ; readelf -r {W}/fgn.o | grep -cw lgamma",
		 "2\n2\n"},

		{"O3_float_return_inline", "cpu=x86_64,os=linux,optimizer",
		 "printf 'static double f(double x){return x*2.0+1.0;} static float g(float x){return x*3.0f+0.5f;} static double h(int a,int b){return (double)a/(double)b+0.25;} static int ii(int x){return x*2+1;} int main(void){ if(f(10.0)!=21.0)return 1; if(g(4.0f)!=12.5f)return 2; if(h(7,2)!=3.75)return 3; if(ii(20)!=41)return 4; return 0; }\\n' > {W}/fi.c && "
		 "{MCC} -B{B} -I{I} -O0 -run {W}/fi.c && echo O0OK && "
		 "{MCC} -B{B} -I{I} -O3 -run {W}/fi.c && echo O3OK",
		 "O0OK\nO3OK\n"},

		{"debug_default_stabs", "cpu=x86_64,os=linux,stabs",
		 "{MCC} -B{B} -I{I} -g -c {D}/lib.c -o {W}/g.o && readelf -S {W}/g.o | grep -oE '\\.stab' | sort -u",
		 ".stab\n"},

		{"debug_gstabs", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -gstabs -c {D}/lib.c -o {W}/st.o && readelf -S {W}/st.o | grep -oE '\\.stab' | sort -u",
		 ".stab\n"},

		{"debug_dwarf5_info", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -gdwarf-5 -c {D}/lib.c -o {W}/g5.o && "
		 "readelf --debug-dump=info {W}/g5.o 2>/dev/null | grep -oE 'DW_TAG_subprogram|exported_fn' | sort -u",
		 "DW_TAG_subprogram\nexported_fn\n"},

		{"debug_dwarf_version_select", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -gdwarf-5 -c {D}/lib.c -o {W}/g5b.o && "
		 "readelf --debug-dump=info {W}/g5b.o 2>/dev/null | awk '/Version/{print $2; exit}'",
		 "5\n"},

		{"debug_dwarf_int256_basetype", "os=darwin",
		 "printf '__int256 v_s;\\nunsigned __int256 v_u;\\n' > {W}/i256.c && "
		 "{MCC} -B{B} -I{I} -gdwarf-5 -c {W}/i256.c -o {W}/i256.o && "
		 "dwarfdump {W}/i256.o | grep -A3 'DW_TAG_base_type' | grep -Ec '\"(unsigned )?__int256\"'",
		 "2\n"},

		{"constructor_init_array", "cpu=x86_64,os=linux",
		 "printf '__attribute__((constructor)) void c1(void){}\\nint main(void){return 0;}\\n' > {W}/ctor.c && "
		 "{MCC} -B{B} -I{I} -c {W}/ctor.c -o {W}/ctor.o && readelf -S {W}/ctor.o | grep -oE '\\.init_array' | head -1",
		 ".init_array\n"},

		{"deps_M_rule", "",
		 "cd {D} && {MCC} -B{B} -I{I} -M dep.c",
		 "dep.o: \\\ndep.c \\\ndep.h\n"},

		{"deps_MD_MF_file", "",
		 "cd {D} && {MCC} -B{B} -I{I} -MD -MF {W}/out.d -c dep.c -o {W}/dep.o && grep -oE 'dep\\.(c|h)' {W}/out.d",
		 "dep.c\ndep.h\n"},

		{"include_next_directive", "",
		 "{MCC} -B{B} -I{I} -I{D}/incnext/d1 -I{D}/incnext/d2 {D}/incnext/incnext_main.c -o {W}/incn && {W}/incn",
		 "1 2\n"},

		{"undef_flag", "",
		 "printf 'X\\n' > {W}/u.c && {MCC} -B{B} -DX=1 -UX -E -P {W}/u.c",
		 "X\n"},

		{"array_bound_completed_through_pointer_redecl", "",
		 "printf 'extern int(*p)[];int(*p)[10];extern int(**q)[];int(**q)[7];"
		 "int main(void){return (sizeof(*p)==40&&sizeof(**q)==28)?0:1;}\\n' > {W}/abp.c && "
		 "{MCC} -B{B} -I{I} -run {W}/abp.c && echo OK",
		 "OK\n"},

		{"line_macro_near_intmax", "",
		 "printf '#line 2147483647\\nlong long a=__LINE__;\\nlong long b=__LINE__;\\nlong long c=__LINE__;\\n"
		 "int main(void){return (a==2147483647LL&&b==2147483648LL&&c==2147483649LL)?0:1;}\\n' > {W}/ln.c && "
		 "{MCC} -B{B} -I{I} -w -run {W}/ln.c && echo OK",
		 "OK\n"},

		{"std_last_wins_trigraphs", "",
		 "printf 'char *s = \"x?" "?=y\";\\n' > {W}/tg.c && "
		 "printf 'c99=%s gnu=%s\\n' "
		 "$({MCC} -B{B} -std=c99 -E -P {W}/tg.c | grep -Fc 'x#y') "
		 "$({MCC} -B{B} -std=c99 -std=gnu99 -E -P {W}/tg.c | grep -Fc 'x#y')",
		 "c99=1 gnu=0\n"},

		{"sync_nand_builtins", "",
		 "{MCC} -B{B} -I{I} {D}/syncnand.c -o {W}/snd && {W}/snd",
		 "OK\n"},

		{"cgoto_into_vla_rejected", "",
		 "{MCC} -B{B} -I{I} -c {D}/cgoto_vla.c -o {W}/cg.o 2>{W}/cg.err; "
		 "grep -c 'variably modified' {W}/cg.err",
		 "1\n"},

		{"u8_char_single_code_unit", "",
		 "{MCC} -B{B} -I{I} -c {D}/u8char_ok.c -o {W}/u8ok.o 2>{W}/u8ok.err; "
		 "{MCC} -B{B} -I{I} -c {D}/u8char_bad.c -o {W}/u8bad.o 2>{W}/u8bad.err; "
		 "printf 'ok=%s bad=%s\\n' $(grep -c 'error' {W}/u8ok.err) $(grep -c 'error' {W}/u8bad.err)",
		 "ok=0 bad=1\n"},

		{"depfile_escapes_hash_dollar", "",
		 "mkdir -p {W}/depd && printf 'int h;\\n' > '{W}/depd/a$b#c.h' && "
		 "printf '#include \"a$b#c.h\"\\nint x;\\n' > {W}/depd/m.c && "
		 "{MCC} -B{B} -I{W}/depd -M -MF {W}/depd/out.mk -c {W}/depd/m.c -o {W}/depd/m.o && "
		 "grep -Fc 'a$$b\\#c.h' {W}/depd/out.mk",
		 "1\n"},

		{"depfile_mg_missing_header", "",
		 "printf '#include \"gen_cfg.h\"\\nint x;\\n' > {W}/mg.c && "
		 "{MCC} -B{B} -I{I} -M -MF {W}/mg.nomg.mk {W}/mg.c >{W}/mg.nomg.out 2>{W}/mg.nomg.err; nomg=$?; "
		 "{MCC} -B{B} -I{I} -M -MG -MF {W}/mg.yes.mk {W}/mg.c >{W}/mg.yes.out 2>{W}/mg.yes.err; yes=$?; "
		 "printf 'nomg_rc=%s mg_rc=%s dep=%s\\n' $nomg $yes $(grep -Fc 'gen_cfg.h' {W}/mg.yes.mk)",
		 "nomg_rc=1 mg_rc=0 dep=1\n"},

		{"preprocess_long_warning_not_truncated", "",
		 "printf '#warning START%01100dEND\\nint a;\\n' 0 > {W}/lw.c && "
		 "{MCC} -B{B} -I{I} -c {W}/lw.c -o {W}/lw.o 2>{W}/lw.err; "
		 "printf 'start=%s end=%s\\n' $(grep -Fc 'START' {W}/lw.err) $(grep -Fc 'END' {W}/lw.err)",
		 "start=1 end=1\n"},

		{"asm_zero_equ_equiv_directives", "",
		 "printf '.data\\n.globl asmval\\nasmval:\\n.equ MYV, 42\\n.long MYV\\n.zero 8\\n.globl asmval2\\nasmval2:\\n.equiv MYV2, 58\\n.long MYV2\\n' > {W}/dz.s && "
		 "printf 'extern int asmval __asm__(\"asmval\"), asmval2 __asm__(\"asmval2\");\\nint main(void){ return asmval + asmval2; }\\n' > {W}/dz.c && "
		 "{MCC} -B{B} {W}/dz.s {W}/dz.c -o {W}/dz.exe 2>{W}/dz.err; link=$?; "
		 "{W}/dz.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=100\n"},

		{"asm_if_else_endif_directives", "",
		 "printf '.data\\n.globl base\\nbase:\\n.if 1\\n.byte 10\\n.else\\n.byte 20\\n.endif\\n.if 0\\n.byte 30\\n.else\\n.byte 40\\n.endif\\n.if 1\\n.if 0\\n.byte 99\\n.else\\n.byte 7\\n.endif\\n.endif\\n' > {W}/if.s && "
		 "printf 'extern char base[] __asm__(\"base\");\\nint main(void){ return base[0]+base[1]+base[2]; }\\n' > {W}/if.c && "
		 "{MCC} -B{B} {W}/if.s {W}/if.c -o {W}/if.exe 2>{W}/if.err; link=$?; "
		 "{W}/if.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=57\n"},

		{"asm_ifc_ifb_directives", "",
		 "printf '.data\\n.globl base\\nbase:\\n.ifc abc, abc\\n.byte 1\\n.else\\n.byte 2\\n.endif\\n.ifc abc, xyz\\n.byte 4\\n.else\\n.byte 8\\n.endif\\n.ifb\\n.byte 16\\n.else\\n.byte 32\\n.endif\\n.ifnb something\\n.byte 64\\n.else\\n.byte 128\\n.endif\\n' > {W}/fc.s && "
		 "printf 'extern char base[] __asm__(\"base\");\\nint main(void){ return base[0]+base[1]+base[2]+base[3]; }\\n' > {W}/fc.c && "
		 "{MCC} -B{B} {W}/fc.s {W}/fc.c -o {W}/fc.exe 2>{W}/fc.err; link=$?; "
		 "{W}/fc.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=89\n"},

		{"asm_elseif_directive", "",
		 "printf '.data\\n.globl base\\nbase:\\n.if 0\\n.byte 1\\n.elseif 1\\n.byte 2\\n.elseif 1\\n.byte 3\\n.else\\n.byte 4\\n.endif\\n.if 0\\n.byte 10\\n.elseif 0\\n.byte 20\\n.else\\n.byte 40\\n.endif\\n.if 1\\n.byte 100\\n.elseif 1\\n.byte 99\\n.endif\\n' > {W}/ei.s && "
		 "printf 'extern char base[] __asm__(\"base\");\\nint main(void){ return base[0]+base[1]+base[2]; }\\n' > {W}/ei.c && "
		 "{MCC} -B{B} {W}/ei.s {W}/ei.c -o {W}/ei.exe 2>{W}/ei.err; link=$?; "
		 "{W}/ei.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=142\n"},

		{"asm_ifdef_ifndef_directives", "",
		 "printf '.data\\n.globl base\\nbase:\\n.set DEF_SYM, 1\\n.ifdef DEF_SYM\\n.byte 11\\n.else\\n.byte 22\\n.endif\\n.ifdef UNDEF_SYM\\n.byte 33\\n.else\\n.byte 44\\n.endif\\n.ifndef UNDEF_SYM2\\n.byte 55\\n.else\\n.byte 66\\n.endif\\n' > {W}/id.s && "
		 "printf 'extern char base[] __asm__(\"base\");\\nint main(void){ return base[0]+base[1]+base[2]; }\\n' > {W}/id.c && "
		 "{MCC} -B{B} {W}/id.s {W}/id.c -o {W}/id.exe 2>{W}/id.err; link=$?; "
		 "{W}/id.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=110\n"},

		{"asm_equiv_redefinition_errors", "",
		 "printf '.equiv foo, 5\\n.equiv foo, 6\\n.long foo\\n' > {W}/eq.s && "
		 "{MCC} -B{B} -c {W}/eq.s -o {W}/eq.o 2>{W}/eq.err; equiv=$?; "
		 "printf '.equ bar, 5\\n.equ bar, 6\\n.long bar\\n' > {W}/eu.s && "
		 "{MCC} -B{B} -c {W}/eu.s -o {W}/eu.o 2>{W}/eu.err; equ=$?; "
		 "printf 'equiv_rc=%s equ_rc=%s\\n' $equiv $equ",
		 "equiv_rc=1 equ_rc=0\n"},

		{"asm_if_compare_variants", "",
		 "printf '.data\\n.globl base\\nbase:\\n.ifeq 0\\n.byte 1\\n.else\\n.byte 2\\n.endif\\n.ifne 5\\n.byte 4\\n.else\\n.byte 8\\n.endif\\n.ifgt 3\\n.byte 16\\n.else\\n.byte 32\\n.endif\\n.iflt 3\\n.byte 64\\n.else\\n.byte 8\\n.endif\\n' > {W}/ic.s && "
		 "printf 'extern char base[] __asm__(\"base\");\\nint main(void){ return base[0]+base[1]+base[2]+base[3]; }\\n' > {W}/ic.c && "
		 "{MCC} -B{B} {W}/ic.s {W}/ic.c -o {W}/ic.exe 2>{W}/ic.err; link=$?; "
		 "{W}/ic.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=29\n"},

		{"asm_p2align_max_skip", "",
		 "printf '.data\\n.globl base\\nbase:\\n.byte 1,2,3,4,5\\n.p2align 4,,15\\n.globl m1\\nm1:\\n.byte 9\\n.p2align 4,,3\\n.globl m2\\nm2:\\n.long 0\\n' > {W}/pa.s && "
		 "printf 'extern char base[] __asm__(\"base\"), m1[] __asm__(\"m1\"), m2[] __asm__(\"m2\");\\nint main(void){ return (int)(m1-base) + (int)(m2-base); }\\n' > {W}/pa.c && "
		 "{MCC} -B{B} {W}/pa.s {W}/pa.c -o {W}/pa.exe 2>{W}/pa.err; link=$?; "
		 "{W}/pa.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=33\n"},

		{"asm_comm_lcomm_directives", "",
		 "printf '.comm sharedbuf, 16, 8\\n.lcomm localbuf, 32, 4\\n' > {W}/cl.s && "
		 "printf 'extern char sharedbuf[16] __asm__(\"sharedbuf\");\\nint main(void){ sharedbuf[0]=42; sharedbuf[15]=58; return sharedbuf[0]+sharedbuf[15]; }\\n' > {W}/cl.c && "
		 "{MCC} -B{B} {W}/cl.s {W}/cl.c -o {W}/cl.exe 2>{W}/cl.err; link=$?; "
		 "{W}/cl.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=100\n"},

		{"asm_cfi_arch_cpu_accepted", "cpu=x86_64",
		 "printf '.text\\n.arch x86-64\\n.cpu generic\\n.globl asmfn\\nasmfn:\\n.cfi_startproc\\n.cfi_def_cfa_offset 8\\n.cfi_offset 6, -16\\nmovl $123, %%eax\\nret\\n.cfi_endproc\\n' > {W}/cf.s && "
		 "printf 'extern int asmfn(void) __asm__(\"asmfn\");\\nint main(void){ return asmfn(); }\\n' > {W}/cf.c && "
		 "{MCC} -B{B} {W}/cf.s {W}/cf.c -o {W}/cf.exe 2>{W}/cf.err; link=$?; "
		 "{W}/cf.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=123\n"},

		{"asm_macro_directive", "",
		 "printf '%s\\n' '.macro pair a, b' '.long \\a' '.long \\b' '.endm' '.data' '.globl blob' 'blob:' 'pair 0x1111, 0x2222' 'pair 3+4, 5*6' > {W}/mc.s && "
		 "printf '#include <string.h>\\nextern unsigned char blob[] __asm__(\"blob\");\\nint main(void){ unsigned int a,b,c,d; memcpy(&a,blob+0,4); memcpy(&b,blob+4,4); memcpy(&c,blob+8,4); memcpy(&d,blob+12,4); return (a==0x1111&&b==0x2222&&c==7&&d==30)?42:1; }\\n' > {W}/mc.c && "
		 "{MCC} -B{B} {W}/mc.s {W}/mc.c -o {W}/mc.exe 2>{W}/mc.err; link=$?; "
		 "{W}/mc.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=42\n"},

		{"asm_irp_directive", "",
		 "printf '%s\\n' '.data' '.globl blob' 'blob:' '.irp v, 0x11, 0x22, 0x33' '.long \\v' '.endr' '.irp w, 5+5, 3*4' '.long \\w' '.endr' > {W}/ip.s && "
		 "printf '#include <string.h>\\nextern unsigned int blob[] __asm__(\"blob\");\\nint main(void){ return (blob[0]==0x11&&blob[1]==0x22&&blob[2]==0x33&&blob[3]==10&&blob[4]==12)?42:1; }\\n' > {W}/ip.c && "
		 "{MCC} -B{B} {W}/ip.s {W}/ip.c -o {W}/ip.exe 2>{W}/ip.err; link=$?; "
		 "{W}/ip.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=42\n"},

		{"asm_data_directives_widths", "",
		 "printf '.data\\n.globl base\\nbase:\\n.hword 0x1234\\n.value 0x5678\\n.2byte 0xABCD\\n.4byte 0xDEADBEEF\\n.8byte 0x1122334455667788\\n.xword 0x99AABBCCDDEEFF11\\n.float 1.5\\n.double 2.5\\n.octa 0x0102030405060708090A0B0C0D0E0F10\\n.dc.b 0x77\\n.dc.w 0x8899\\n.dc.l 0xCAFEBABE\\n.globl endp\\nendp:\\n' > {W}/dd.s && "
		 "printf '#include <string.h>\\nextern unsigned char base[] __asm__(\"base\");\\nextern unsigned char endp[] __asm__(\"endp\");\\nint main(void){ unsigned short h,v,t,w2; unsigned int f,l; unsigned long long e,x; float ff; double dd; memcpy(&h,base+0,2); memcpy(&v,base+2,2); memcpy(&t,base+4,2); memcpy(&f,base+6,4); memcpy(&e,base+10,8); memcpy(&x,base+18,8); memcpy(&ff,base+26,4); memcpy(&dd,base+30,8); memcpy(&w2,base+55,2); memcpy(&l,base+57,4); if((endp-base)!=61) return 100; if(h!=0x1234) return 101; if(v!=0x5678) return 102; if(t!=0xabcd) return 103; if(f!=0xdeadbeefU) return 104; if(e!=0x1122334455667788ULL) return 105; if(x!=0x99aabbccddeeff11ULL) return 106; if(ff!=1.5f) return 107; if(dd!=2.5) return 108; if(base[38]!=0x10||base[45]!=0x09||base[53]!=0x01) return 109; if(base[54]!=0x77) return 110; if(w2!=0x8899) return 111; if(l!=0xcafebabeU) return 112; return 42; }\\n' > {W}/dd.c && "
		 "{MCC} -B{B} {W}/dd.s {W}/dd.c -o {W}/dd.exe 2>{W}/dd.err; link=$?; "
		 "{W}/dd.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=42\n"},

		{"asm_ldst_exclusive_atomic", "cpu=arm64",
		 "printf '%s\\n' '.text' '.globl atomx' 'atomx:' 'sub sp, sp, #16' 'mov x9, sp' "
		 "'mov x1, #100' 'str x1, [x9]' '1:' 'ldaxr x2, [x9]' 'add x2, x2, #15' "
		 "'stlxr w3, x2, [x9]' 'cbnz w3, 1b' 'ldar x4, [x9]' 'mov w1, #10' 'str w1, [x9]' "
		 "'2:' 'ldxr w5, [x9]' 'add w5, w5, #5' 'stxr w6, w5, [x9]' 'cbnz w6, 2b' "
		 "'ldxr w7, [x9]' 'add x4, x4, x7' 'mov w1, #7' 'stlrb w1, [x9]' 'ldarb w8, [x9]' "
		 "'add x4, x4, x8' 'mov w1, #9' '3:' 'ldxrb w5, [x9]' 'stxrb w6, w1, [x9]' "
		 "'cbnz w6, 3b' 'ldxrb w5, [x9]' 'add x4, x4, x5' 'mov w1, #200' 'strh w1, [x9]' "
		 "'4:' 'ldxrh w5, [x9]' 'add w5, w5, #100' 'stxrh w6, w5, [x9]' 'cbnz w6, 4b' "
		 "'ldxrh w5, [x9]' 'add x4, x4, x5' 'mov w1, #0x1000' 'stlrh w1, [x9]' "
		 "'ldarh w8, [x9]' 'add x4, x4, x8' 'mov x0, x4' 'add sp, sp, #16' 'ret' > {W}/ax.s && "
		 "printf 'extern int atomx(void) __asm__(\"atomx\");\\nint main(void){ return atomx()==4542?42:1; }\\n' > {W}/ax.c && "
		 "{MCC} -B{B} {W}/ax.s {W}/ax.c -o {W}/ax.exe 2>{W}/ax.err; link=$?; "
		 "{W}/ax.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=42\n"},

		{"asm_constraint_o_p_X", "cpu=arm64",
		 "printf 'int gv = 99;\\nint main(void){ int x=7, ry, rp, rx; "
		 "__asm__(\"ldr %%w0, %%1\" : \"=r\"(ry) : \"o\"(x)); "
		 "__asm__(\"ldr %%w0, [%%1]\" : \"=r\"(rp) : \"p\"(&gv)); "
		 "__asm__(\"mov %%w0, %%w1\" : \"=r\"(rx) : \"X\"(13)); "
		 "return (ry==7 && rp==99 && rx==13) ? 42 : 1; }\\n' > {W}/cpx.c && "
		 "{MCC} -B{B} {W}/cpx.c -o {W}/cpx.exe 2>{W}/cpx.err; link=$?; "
		 "{W}/cpx.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=42\n"},

		{"pragma_pack_aligned_member_align", "os=Darwin",
		 "printf '#pragma pack(8)\\nstruct A { char c; int x __attribute__((aligned(2))); };\\n"
		 "struct B { char c; int x __attribute__((aligned(16))); };\\n#pragma pack(2)\\n"
		 "struct C { char c; int x __attribute__((aligned(1))); };\\n#pragma pack()\\n"
		 "struct D { char c; int x __attribute__((packed, aligned(2))); };\\n"
		 "int main(void){ if (_Alignof(struct A)!=4 || sizeof(struct A)!=8) return 1; "
		 "if (_Alignof(struct B)!=8 || sizeof(struct B)!=16) return 2; "
		 "if (_Alignof(struct C)!=2 || sizeof(struct C)!=6) return 3; "
		 "if (_Alignof(struct D)!=2 || sizeof(struct D)!=6) return 4; return 42; }\\n' > {W}/pk.c && "
		 "{MCC} -B{B} {W}/pk.c -o {W}/pk.exe 2>{W}/pk.err; link=$?; "
		 "{W}/pk.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=42\n"},

		{"pragma_weak_alias_form", "os=Darwin",
		 "printf 'int impl_a(void){ return 40; }\\n#pragma weak alias_a = impl_a\\n"
		 "extern int alias_a(void);\\nint impl_b(void){ return 100; }\\n"
		 "#pragma weak alias_b = impl_b\\n"
		 "int main(void){ extern int alias_b(void); "
		 "return (alias_a()==40 && alias_b()==100) ? 42 : 1; }\\n' > {W}/pw.c && "
		 "{MCC} -B{B} {W}/pw.c -o {W}/pw.exe 2>{W}/pw.err; link=$?; "
		 "{W}/pw.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=42\n"},

		{"stmt_expr_void_when_last_not_expr", "",
		 "printf 'int f(int x){return x;}\\nint main(void){ int r=({ f(1); f(40); }); "
		 "return r+2==42?42:1; }\\n' > {W}/seg.c && "
		 "{MCC} -B{B} {W}/seg.c -o {W}/seg.exe 2>{W}/seg.err; {W}/seg.exe; good=$?; "
		 "printf 'int g(int x){return x;}\\nint h(void){ int r=({ g(1); {int z=5;} }); "
		 "return r; }\\n' > {W}/seb.c && "
		 "{MCC} -B{B} -c {W}/seb.c -o {W}/seb.o 2>{W}/seb.err; bad=$?; "
		 "printf 'good=%s bad_rejected=%s\\n' $good $bad",
		 "good=42 bad_rejected=1\n"},

		{"wa_assembler_passthrough_accepted", "",
		 "printf 'int main(void){return 42;}\\n' > {W}/wa.c && "
		 "{MCC} -B{B} -Wa,--noexecstack,-g -c {W}/wa.c -o {W}/wa.o 2>{W}/wa.err; crc=$?; "
		 "warns=$(grep -c 'unsupported option' {W}/wa.err); "
		 "{MCC} -B{B} -Wa,--noexecstack {W}/wa.c -o {W}/wa.exe 2>>{W}/wa.err; {W}/wa.exe; run=$?; "
		 "printf 'compile=%s warns=%s run=%s\\n' $crc $warns $run",
		 "compile=0 warns=0 run=42\n"},

		{"macho_bundle_output", "os=Darwin",
		 "printf 'int plugin_answer(void){return 42;}\\n' > {W}/plg.c && "
		 "{MCC} -B{B} -bundle {W}/plg.c -o {W}/plg.bundle 2>{W}/plg.err; brc=$?; "
		 "ftb=$(otool -hv {W}/plg.bundle 2>/dev/null | grep -c BUNDLE); "
		 "idc=$(otool -l {W}/plg.bundle 2>/dev/null | grep -c LC_ID_DYLIB); "
		 "printf '#include <dlfcn.h>\\n#include <stdio.h>\\n"
		 "int main(int c,char**v){ void*h=dlopen(v[1],2); if(!h)return 2; "
		 "int(*f)(void)=(int(*)(void))dlsym(h,\"plugin_answer\"); if(!f)return 3; "
		 "return f()==42?42:1; }\\n' > {W}/host.c && "
		 "{MCC} -B{B} {W}/host.c -o {W}/host 2>>{W}/plg.err; {W}/host {W}/plg.bundle; drc=$?; "
		 "printf 'bundle=%s isbundle=%s idcmd=%s dlrun=%s\\n' $brc $ftb $idc $drc",
		 "bundle=0 isbundle=1 idcmd=0 dlrun=42\n"},

		{"macho_custom_writable_section", "os=Darwin",
		 "printf '__attribute__((section(\"__DATA,mine\"))) int gv=0;\\nint main(void){gv=99;return gv;}\\n' > {W}/csec.c && "
		 "{MCC} -B{B} -I{I} {W}/csec.c -o {W}/csec && {W}/csec; echo aot=$?; "
		 "{MCC} -B{B} -I{I} -run {W}/csec.c; echo run=$?",
		 "aot=99\nrun=99\n"},

		{"attr_naked_function", "cpu=arm64,os=Darwin",
		 "printf '__attribute__((naked)) int addfn(int a,int b){ __asm__(\"add w0, w0, w1\"); __asm__(\"ret\"); }\\nint main(void){return addfn(40,2);}\\n' > {W}/nk.c && "
		 "{MCC} -B{B} -I{I} {W}/nk.c -o {W}/nk && {W}/nk; echo aot=$?; "
		 "{MCC} -B{B} -I{I} -run {W}/nk.c; echo run=$?; "
		 "printf '__attribute__((naked)) int bad(int a){int b=a+1;return b;}\\n' > {W}/nkbad.c && "
		 "{MCC} -B{B} -I{I} -c {W}/nkbad.c -o {W}/nkbad.o 2>&1 | grep -oE 'naked function must contain only asm statements'; echo done",
		 "aot=42\nrun=42\nnaked function must contain only asm statements\ndone\n"},

		{"macho_dylib_no_internal_exports", "os=Darwin",
		 "printf 'int pubfn(void){return 7;}\\n' > {W}/vs.c && "
		 "{MCC} -B{B} -shared {W}/vs.c -o {W}/vs.dylib 2>{W}/vs.err; src=$?; "
		 "exp=$(dyld_info -exports {W}/vs.dylib 2>/dev/null); "
		 "pub=$(printf '%s' \"$exp\" | grep -c _pubfn); "
		 "intern=$(printf '%s' \"$exp\" | grep -cE '_edata|_etext|_end|__mh_execute_header'); "
		 "printf 'shared=%s pub=%s internal=%s\\n' $src $pub $intern",
		 "shared=0 pub=1 internal=0\n"},

		{"macho_wl_dylib_version", "os=Darwin",
		 "printf 'int f(void){return 1;}\\n' > {W}/cv.c && "
		 "{MCC} -B{B} -shared -Wl,-current_version,2.3.4 -Wl,-compatibility_version,5.6.7 "
		 "-Wl,-install_name,@rpath/x.dylib {W}/cv.c -o {W}/cv.dylib 2>{W}/cv.err; rc=$?; "
		 "cur=$(otool -l {W}/cv.dylib 2>/dev/null | grep -c 'current version 2.3.4'); "
		 "cmp=$(otool -l {W}/cv.dylib 2>/dev/null | grep -c 'compatibility version 5.6.7'); "
		 "printf 'link=%s cur=%s compat=%s\\n' $rc $cur $cmp",
		 "link=0 cur=1 compat=1\n"},

		{"run_atexit_order_and_reentrant", "os=Darwin",
		 "printf '#include <stdio.h>\\n#include <stdlib.h>\\n"
		 "__attribute__((destructor)) static void dt(void){puts(\"dtor\");}\\n"
		 "static void late(void){puts(\"late\");}\\nstatic void a1(void){puts(\"a1\");}\\n"
		 "static void a2(void){puts(\"a2\"); atexit(late);}\\n"
		 "int main(void){atexit(a1);atexit(a2);return 0;}\\n' > {W}/rr.c && "
		 "{MCC} -B{B} -run {W}/rr.c > {W}/rr.out 2>{W}/rr.err; "
		 "printf 'out=%s\\n' \"$(tr '\\n' ',' < {W}/rr.out)\"",
		 "out=a2,late,a1,dtor,\n"},

		{"macho_framework_header_search_for_c_e", "os=Darwin",
		 "printf '#include <CoreFoundation/CFBase.h>\\nint main(void){return 0;}\\n' > {W}/fwt.c && "
		 "{MCC} -B{B} -E {W}/fwt.c > /dev/null 2>{W}/fwt.err; ec=$?; "
		 "nf=$(grep -c 'not found' {W}/fwt.err); "
		 "{MCC} -B{B} -c {W}/fwt.c -o {W}/fwt.o 2>>{W}/fwt.err; cc=$?; "
		 "printf 'preprocess=%s notfound=%s compile=%s\\n' $ec $nf $cc",
		 "preprocess=0 notfound=0 compile=0\n"},

		{"dwarf_prototyped_attribute", "os=Darwin",
		 "printf 'int kr(a) int a;{return a;}\\nint proto(int a){return a;}\\n' > {W}/dp.c && "
		 "{MCC} -B{B} -g -c {W}/dp.c -o {W}/dp.o 2>{W}/dp.err; rc=$?; "
		 "krp=$(dwarfdump {W}/dp.o 2>/dev/null | grep -A6 '\"kr\"' | grep -c 'DW_AT_prototyped.*0x00'); "
		 "prp=$(dwarfdump {W}/dp.o 2>/dev/null | grep -A6 '\"proto\"' | grep -c 'DW_AT_prototyped.*0x01'); "
		 "printf 'compile=%s kr_notproto=%s proto_yes=%s\\n' $rc $krp $prp",
		 "compile=0 kr_notproto=1 proto_yes=1\n"},

		{"dwarf_line_no_stale_rows", "os=Darwin",
		 "printf 'int a,b,c;\\nint f(void){\\n#line 40\\n a=5;\\n#line 90\\n b=a+7;\\n"
		 "#line 140\\n c=b*3;\\n return c;\\n}\\n' > {W}/bl.c && "
		 "{MCC} -B{B} -g -c {W}/bl.c -o {W}/bl.o 2>{W}/bl.err; rc=$?; "
		 "dups=$(dwarfdump --debug-line {W}/bl.o 2>/dev/null | grep -E '^0x[0-9a-f]+ ' | "
		 "awk '{print $1}' | sort | uniq -d | wc -l | tr -d ' '); "
		 "printf 'compile=%s dupaddrs=%s\\n' $rc $dups",
		 "compile=0 dupaddrs=0\n"},

		{"print_multiarch_empty", "",
		 "{MCC} -B{B} -print-multiarch > {W}/pm.out 2>{W}/pm.err; rc=$?; "
		 "bytes=$(tr -d '\\r' < {W}/pm.out | wc -c | tr -d ' '); "
		 "printf 'rc=%s bytes=%s\\n' $rc $bytes",
		 "rc=0 bytes=1\n"},

		{"dwarf_const_volatile_qualifiers", "os=Darwin",
		 "printf 'const int *a;\\nvolatile int v;\\nint main(void){return a?v:0;}\\n' > {W}/cv.c && "
		 "{MCC} -B{B} -g -c {W}/cv.c -o {W}/cv.o 2>{W}/cv.err; rc=$?; "
		 "ct=$(dwarfdump {W}/cv.o 2>/dev/null | grep -c 'DW_TAG_const_type'); "
		 "vt=$(dwarfdump {W}/cv.o 2>/dev/null | grep -c 'DW_TAG_volatile_type'); "
		 "ver=$(dwarfdump --verify {W}/cv.o 2>&1 | grep -c 'No errors'); "
		 "printf 'compile=%s const=%s volatile=%s verify=%s\\n' $rc $ct $vt $ver",
		 "compile=0 const=1 volatile=1 verify=1\n"},

		{"dwarf_void_unspecified_type", "os=Darwin",
		 "printf 'void doit(void){}\\nint main(void){doit();return 0;}\\n' > {W}/vd.c && "
		 "{MCC} -B{B} -g -c {W}/vd.c -o {W}/vd.o 2>{W}/vd.err; rc=$?; "
		 "uns=$(dwarfdump {W}/vd.o 2>/dev/null | grep -c 'DW_TAG_unspecified_type'); "
		 "voidbase=$(dwarfdump {W}/vd.o 2>/dev/null | grep -B2 'DW_AT_name.*\"void\"' | grep -c 'DW_TAG_base_type'); "
		 "ver=$(dwarfdump --verify {W}/vd.o 2>&1 | grep -c 'No errors'); "
		 "printf 'compile=%s unspec=%s voidbase=%s verify=%s\\n' $rc $uns $voidbase $ver",
		 "compile=0 unspec=1 voidbase=0 verify=1\n"},

		{"dwarf_atomic_type", "os=Darwin",
		 "printf '_Atomic int ai;\\nint main(void){return ai;}\\n' > {W}/at.c && "
		 "{MCC} -B{B} -g -c {W}/at.c -o {W}/at.o 2>{W}/at.err; rc=$?; "
		 "atom=$(dwarfdump {W}/at.o 2>/dev/null | grep -c 'DW_TAG_atomic_type'); "
		 "ver=$(dwarfdump --verify {W}/at.o 2>&1 | grep -c 'No errors'); "
		 "printf 'compile=%s atomic=%s verify=%s\\n' $rc $atom $ver",
		 "compile=0 atomic=1 verify=1\n"},

		{"dwarf_complex_base_type", "os=Darwin",
		 "printf '_Complex float cf;\\n_Complex double cd;\\nint main(void){return 0;}\\n' > {W}/cx.c && "
		 "{MCC} -B{B} -g -c {W}/cx.c -o {W}/cx.o 2>{W}/cx.err; rc=$?; "
		 "cplx=$(dwarfdump {W}/cx.o 2>/dev/null | grep -c 'DW_ATE_complex_float'); "
		 "ver=$(dwarfdump --verify {W}/cx.o 2>&1 | grep -c 'No errors'); "
		 "printf 'compile=%s complex=%s verify=%s\\n' $rc $cplx $ver",
		 "compile=0 complex=2 verify=1\n"},

		{"attr_dll_ignored_macho", "os=Darwin",
		 "printf 'int __attribute__((dllexport)) f(void){return 0;}\\nint __attribute__((dllimport)) h;\\n' > {W}/dll.c && "
		 "{MCC} -B{B} -c {W}/dll.c -o {W}/dll.o 2>&1 | grep -oE \"'dll(ex|im)port' attribute directive ignored\"; "
		 "{MCC} -B{B} -Wno-attributes -c {W}/dll.c -o {W}/dll.o 2>&1 | grep -oE 'attribute directive ignored'; echo END",
		 "'dllexport' attribute directive ignored\n'dllimport' attribute directive ignored\nEND\n"},

		{"attr_protected_visibility_macho", "os=Darwin",
		 "printf 'int __attribute__((visibility(\"protected\"))) g = 1;\\nint __attribute__((visibility(\"hidden\"))) hh = 2;\\n' > {W}/pv.c && "
		 "{MCC} -B{B} -c {W}/pv.c -o {W}/pv.o 2>&1 | grep -oE 'protected visibility attribute not supported in this configuration; ignored'; "
		 "{MCC} -B{B} -w -c {W}/pv.c -o {W}/pv.o 2>&1 | grep -oE 'visibility attribute not supported'; echo END",
		 "protected visibility attribute not supported in this configuration; ignored\nEND\n"},

		{"attr_warn_unused_result_on_nonfunction", "",
		 "printf 'int __attribute__((warn_unused_result)) v;\\nint (*fp)(void) __attribute__((warn_unused_result));\\nint f(void) __attribute__((warn_unused_result));\\nint main(void){return 0;}\\n' > {W}/wur.c && "
		 "{MCC} -B{B} -c {W}/wur.c -o {W}/wur.o 2>&1 | grep -oE 'only applies to function types'; "
		 "{MCC} -B{B} -Wno-attributes -c {W}/wur.c -o {W}/wur.o 2>&1 | grep -oE 'only applies to function types'; echo END",
		 "only applies to function types\nEND\n"},

		{"preprocess_system_header_flag", "",
		 "mkdir -p {W}/sysh && printf 'int sysfn(void);\\n' > {W}/sysh/sf.h && "
		 "printf 'int userfn(void);\\n' > {W}/uh.h && "
		 "printf '#include <sf.h>\\n#include \"uh.h\"\\nint x;\\n' > {W}/pp.c && "
		 "{MCC} -B{B} -I{W} -isystem {W}/sysh -E {W}/pp.c > {W}/pp.out 2>{W}/pp.err; "
		 "printf 'sys=%s usr=%s cmdline=%s\\n' "
		 "$(grep -F 'sf.h' {W}/pp.out | grep -Ec ' 3$') "
		 "$(grep -F 'uh.h' {W}/pp.out | grep -Ec ' 3$') "
		 "$(grep -F 'command-line' {W}/pp.out | grep -Ec ' 3$')",
		 "sys=1 usr=0 cmdline=0\n"},

		{"builtin_f16_inf_nan", "cpu=x86_64",
		 "printf 'int main(void){ _Float16 i=__builtin_inff16(); _Float16 h=__builtin_huge_valf16(); _Float16 n=__builtin_nanf16(\"\"); return ((i==h)&&(i>(_Float16)65000.0f)&&(n!=n))?7:0; }\\n' > {W}/f16b.c && "
		 "{MCC} -B{B} {W}/f16b.c -o {W}/f16b.exe 2>{W}/f16b.err; {W}/f16b.exe; printf 'r=%s\\n' $?",
		 "r=7\n"},

		{"knr_char_param_promotion", "",
		 "printf 'int f(a) char a;{return a;} int f(char a);\\n' > {W}/kd.c && "
		 "printf 'int h(a) char a;{return a+1;} int main(void){ return h(200)==-55 ? 7 : 0; }\\n' > {W}/kb.c && "
		 "printf 'int g(char a); int g(a) char a;{return a+1;} int use(void){return g(1);}\\n' > {W}/kp.c && "
		 "{MCC} -B{B} -c {W}/kd.c -o {W}/kd.o 2>{W}/kd.e; rej=$?; "
		 "{MCC} -B{B} {W}/kb.c -o {W}/kb.exe 2>{W}/kb.e; {W}/kb.exe; body=$?; "
		 "{MCC} -B{B} -c {W}/kp.c -o {W}/kp.o 2>{W}/kp.e; pf=$?; "
		 "printf 'reject=%s body=%s protofirst=%s\\n' $rej $body $pf",
		 "reject=1 body=7 protofirst=0\n"},

		{"empty_paren_def_vs_prototype", "",
		 "printf 'int f(int,int); int f(){return 42;} int main(void){return 0;}\\n' > {W}/pf.c && "
		 "{MCC} -B{B} -c {W}/pf.c -o {W}/pf.o 2>{W}/pf.e; pf=$?; "
		 "printf 'int g(){return 42;} int g(int,int); int main(void){return 0;}\\n' > {W}/df.c && "
		 "{MCC} -B{B} -c {W}/df.c -o {W}/df.o 2>{W}/df.e; df=$?; "
		 "printf 'int h(){return 7;} int main(void){return h(1,2);}\\n' > {W}/ctl.c && "
		 "{MCC} -B{B} {W}/ctl.c -o {W}/ctl.exe 2>{W}/ctl.e && {W}/ctl.exe; ctl=$?; "
		 "printf 'protofirst=%s deffirst=%s control=%s\\n' $pf $df $ctl",
		 "protofirst=1 deffirst=1 control=7\n"},

		{"c23_empty_paren_std_gated", "",
		 "printf 'int g();\\nint main(void){ return g(1,2); }\\n' > {W}/ep.c && "
		 "{MCC} -B{B} -c {W}/ep.c -o {W}/ep_d.o 2>{W}/ep_d.e; dflt=$?; "
		 "{MCC} -B{B} -std=c23 -c {W}/ep.c -o {W}/ep_c.o 2>{W}/ep_c.e; c23=$?; "
		 "{MCC} -B{B} -std=gnu23 -c {W}/ep.c -o {W}/ep_g.o 2>{W}/ep_g.e; gnu=$?; "
		 "printf 'default=%s c23=%s gnu23=%s\\n' $dflt $c23 $gnu",
		 "default=0 c23=1 gnu23=0\n"},

		{"generic_nonselected_semantic_check", "",
		 "printf 'int main(void){ return _Generic(0, int:0, double: undeclared_xyz); }\\n' > {W}/gu.c && "
		 "{MCC} -B{B} -I{I} -c {W}/gu.c -o {W}/gu.o 2>{W}/gu.e; undecl=$?; "
		 "printf 'int f(void){return 0;} int main(void){ return _Generic(0, int: 7, double: f()+f()+f()); }\\n' > {W}/gv.c && "
		 "{MCC} -B{B} -I{I} {W}/gv.c -o {W}/gv.exe 2>{W}/gv.e && {W}/gv.exe; valid=$?; "
		 "printf 'undecl=%s valid=%s\\n' $undecl $valid",
		 "undecl=1 valid=7\n"},

		{"builtin_dwarf_cfa", "",
		 "printf 'int main(void){ char*c=(char*)__builtin_dwarf_cfa(); char*f=(char*)__builtin_frame_address(0); return (c>=f && c!=0) ? 7 : 0; }\\n' > {W}/cfa.c && "
		 "{MCC} -B{B} {W}/cfa.c -o {W}/cfa.exe && {W}/cfa.exe; echo val=$?; "
		 "printf 'int main(void){ return (int)(long)__builtin_dwarf_cfa(0); }\\n' > {W}/cfaa.c && "
		 "{MCC} -B{B} -c {W}/cfaa.c -o {W}/cfaa.o 2>{W}/cfaa.e; echo arg=$?",
		 "val=7\narg=1\n"},

		{"builtin_dwarf_sp_column", "",
		 "printf 'int arr[__builtin_dwarf_sp_column()>0 ? 1 : -1]; int main(void){ return (int)sizeof(arr); }\\n' > {W}/spc.c && "
		 "{MCC} -B{B} {W}/spc.c -o {W}/spc.exe && {W}/spc.exe; echo rc=$?",
		 "rc=4\n"},

		{"include_next_primary_source_warn", "",
		 "printf '#include_next <stddef.h>\\nint main(void){return 0;}\\n' > {W}/incn.c && "
		 "{MCC} -B{B} -I{I} -c {W}/incn.c -o {W}/incn.o 2>{W}/incn.e; "
		 "grep -oE \"'#include_next' in primary source file\" {W}/incn.e; "
		 "printf '#include_next <stddef.h>\\n' > {W}/inh.h && "
		 "printf '#include \"inh.h\"\\nint main(void){return 0;}\\n' > {W}/inh_host.c && "
		 "{MCC} -B{B} -I{I} -I{W} -c {W}/inh_host.c -o {W}/inh_host.o 2>{W}/inh.e; "
		 "if grep -q 'primary source' {W}/inh.e; then echo HDR_WARNED_BUG; else echo hdr_ok; fi",
		 "'#include_next' in primary source file\nhdr_ok\n"},

		{"preprocess_C_keeps_comments", "",
		 "printf '#define M 1 /* DIRCMT */\\nint aa; /* BLOCKCMT */ int bb; // LINECMT\\nint cc = M;\\n' > {W}/kc.c && "
		 "{MCC} -B{B} -E -C {W}/kc.c > {W}/kcC.out 2>{W}/kc.e1 && "
		 "{MCC} -B{B} -E {W}/kc.c > {W}/kcP.out 2>{W}/kc.e2 && "
		 "printf 'block=%s line=%s dir=%s plain=%s\\n' "
		 "$(grep -c 'BLOCKCMT' {W}/kcC.out) "
		 "$(grep -c 'LINECMT' {W}/kcC.out) "
		 "$(grep -c 'DIRCMT' {W}/kcC.out) "
		 "$(grep -c 'BLOCKCMT' {W}/kcP.out)",
		 "block=1 line=1 dir=0 plain=0\n"},

		{"preprocess_imacros_macros_only", "",
		 "printf '#define IM_VAL 42\\nint im_dup = 100;\\n' > {W}/im.h && "
		 "printf 'extern int im_dup;\\nint main(void){ return IM_VAL + im_dup; }\\n' > {W}/imx.c && "
		 "printf 'int im_dup = 5;\\n' > {W}/imy.c && "
		 "{MCC} -B{B} -imacros {W}/im.h {W}/imx.c {W}/imy.c -o {W}/im.exe 2>{W}/im.err; link=$?; "
		 "{W}/im.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=47\n"},

		{"link_start_group_cross_archive", "os!=Darwin:mcc -ar is ELF/PE-only (T-mac-30232)",
		 "printf 'extern int b_sym(void); int a_sym(void){ return b_sym()+1; }\\n' > {W}/la.c && "
		 "printf 'int a2_sym(void){ return 40; }\\n' > {W}/la2.c && "
		 "printf 'extern int a2_sym(void); int b_sym(void){ return a2_sym()+1; }\\n' > {W}/lb.c && "
		 "printf 'extern int a_sym(void); int main(void){ return a_sym(); }\\n' > {W}/lmain.c && "
		 "{MCC} -B{B} -c {W}/la.c -o {W}/la.o && {MCC} -B{B} -c {W}/la2.c -o {W}/la2.o && "
		 "{MCC} -B{B} -c {W}/lb.c -o {W}/lb.o && {MCC} -B{B} -c {W}/lmain.c -o {W}/lmain.o && "
		 "{MCC} -B{B} -ar rcs {W}/liba.a {W}/la.o {W}/la2.o && {MCC} -B{B} -ar rcs {W}/libb.a {W}/lb.o && "
		 "{MCC} -B{B} {W}/lmain.o -Wl,--start-group {W}/liba.a {W}/libb.a -Wl,--end-group -o {W}/lg.exe 2>{W}/lg.err; link=$?; "
		 "{W}/lg.exe; run=$?; "
		 "printf 'link=%s run=%s\\n' $link $run",
		 "link=0 run=42\n"},

		{"output_type_same_action_no_warn", "",
		 "printf 'int x;\\n' > {W}/ot.c && "
		 "{MCC} -B{B} -I{I} -c -c -o {W}/ot.o {W}/ot.c 2>{W}/ot.same.err; "
		 "{MCC} -B{B} -I{I} -c -S -o {W}/ot.s {W}/ot.c 2>{W}/ot.diff.err; "
		 "printf 'same=%s diff=%s\\n' $(grep -c 'overriding' {W}/ot.same.err) $(grep -c 'overriding' {W}/ot.diff.err)",
		 "same=0 diff=1\n"},

		{"struct_member_no_storage_class", "",
		 "printf 'struct S { register int x; };\\n' > {W}/sm_bad.c && "
		 "{MCC} -B{B} -I{I} -c {W}/sm_bad.c -o {W}/sm_bad.o 2>{W}/sm_bad.err; "
		 "printf 'struct S { int x; };\\n' > {W}/sm_ok.c && "
		 "{MCC} -B{B} -I{I} -c {W}/sm_ok.c -o {W}/sm_ok.o 2>{W}/sm_ok.err; "
		 "printf 'bad=%s ok=%s\\n' $(grep -c 'storage class' {W}/sm_bad.err) $(grep -c 'error' {W}/sm_ok.err)",
		 "bad=1 ok=0\n"},

		{"constexpr_unsigned_overflow_rejected", "",
		 "printf 'constexpr int i = 0xFFFFFFFFFFFFFFFFULL;\\n' > {W}/cx_bad.c && "
		 "{MCC} -B{B} -I{I} -std=c23 -c {W}/cx_bad.c -o {W}/cx_bad.o 2>{W}/cx_bad.err; "
		 "printf 'constexpr unsigned long long u = 0xFFFFFFFFFFFFFFFFULL;\\n' > {W}/cx_ok.c && "
		 "{MCC} -B{B} -I{I} -std=c23 -c {W}/cx_ok.c -o {W}/cx_ok.o 2>{W}/cx_ok.err; "
		 "printf 'bad=%s ok=%s\\n' $(grep -c 'changes value' {W}/cx_bad.err) $(grep -c 'error' {W}/cx_ok.err)",
		 "bad=1 ok=0\n"},

		{"dM_dump_macros", "",
		 "printf '\\n' > {W}/empty.c && {MCC} -B{B} -E -dM {W}/empty.c | grep -cE '^#define __STDC__ '",
		 "1\n"},

		{"strict_ansi_std_gate", "",
		 "printf '\\n' > {W}/sa.c && echo "
		 "$({MCC} -B{B} -std=c89 -E -dM {W}/sa.c | grep -cE '^#define __STRICT_ANSI__ 1$') "
		 "$({MCC} -B{B} -std=gnu89 -E -dM {W}/sa.c | grep -cE '^#define __STRICT_ANSI__') "
		 "$({MCC} -B{B} -E -dM {W}/sa.c | grep -cE '^#define __STRICT_ANSI__')",
		 "1 0 0\n"},

		{"strict_ansi_gnu_keyword_gate", "",
		 "printf 'int x;\\ntypeof(x) y;\\n' > {W}/gk1.c && "
		 "printf 'int typeof = 1;\\nlong typeof_unqual = 2;\\n' > {W}/gk5.c && "
		 "{MCC} -B{B} -std=c89 -c {W}/gk5.c -o {W}/gk5.o 2>&1 >/dev/null && echo TYPEOF_IDENT_C89_OK; "
		 "{MCC} -B{B} -std=c11 -pedantic-errors -c {W}/gk5.c -o {W}/gk5.o 2>&1 >/dev/null && echo TYPEOF_IDENT_C11_OK; "
		 "{MCC} -B{B} -std=gnu11 -fno-asm -c {W}/gk5.c -o {W}/gk5.o 2>&1 >/dev/null && echo TYPEOF_IDENT_NOASM_OK; "
		 "{MCC} -B{B} -std=c23 -c {W}/gk5.c -o {W}/gk5.o >/dev/null 2>&1 || echo TYPEOF_KEYWORD_C23_OK; "
		 "printf 'int x;\\n__typeof__(x) y;\\nint main(void){return 0;}\\n' > {W}/gk2.c && "
		 "{MCC} -B{B} -std=c89 -c {W}/gk2.c -o {W}/gk2.o 2>&1 && echo TYPEOF_RSVD_OK; "
		 "{MCC} -B{B} -std=gnu89 -c {W}/gk1.c -o {W}/gk1.o 2>&1 >/dev/null && echo TYPEOF_GNU_OK; "
		 "printf 'int main(void){ asm(\\042\\042); return 0; }\\n' > {W}/gk3.c && "
		 "{MCC} -B{B} -std=c89 -pedantic-errors -c {W}/gk3.c -o {W}/gk3.o 2>&1 | "
		 "grep -oE \"'asm' is a GNU extension\"; "
		 "printf 'int main(void){ __asm__(\\042\\042); return 0; }\\n' > {W}/gk4.c && "
		 "{MCC} -B{B} -std=c89 -c {W}/gk4.c -o {W}/gk4.o 2>&1 | "
		 "grep -oE \"'__asm__' is a GNU extension\"; echo ASM_RSVD_OK; echo END",
		 "TYPEOF_IDENT_C89_OK\nTYPEOF_IDENT_C11_OK\nTYPEOF_IDENT_NOASM_OK\nTYPEOF_KEYWORD_C23_OK\nTYPEOF_RSVD_OK\nTYPEOF_GNU_OK\n'asm' is a GNU extension\nASM_RSVD_OK\nEND\n"},

		{"strict_ansi_asm_keyword_compiles", "asm",
		 "printf 'int main(void){ asm(\\042\\042); return 0; }\\n' > {W}/gk5.c && "
		 "{MCC} -B{B} -std=c89 -c {W}/gk5.c -o {W}/gk5.o 2>&1 >/dev/null && echo ASM_BARE_NOPEDANTIC_OK; "
		 "{MCC} -B{B} -std=gnu89 -c {W}/gk5.c -o {W}/gk5.o 2>&1 >/dev/null && echo ASM_GNU_OK; echo END",
		 "ASM_BARE_NOPEDANTIC_OK\nASM_GNU_OK\nEND\n"},

		{"gnu_ext_pedantic_gate", "",
		 "printf 'int f(void){ return ({int x=3; x;}); }\\n' > {W}/ge1.c && "
		 "{MCC} -B{B} -std=c89 -pedantic-errors -c {W}/ge1.c -o {W}/ge1.o 2>&1 | "
		 "grep -oE 'ISO C forbids braced-groups within expressions'; "
		 "printf 'int f(int x){ switch(x){ case 1 ... 5: return 1;} return 0;}\\n' > {W}/ge2.c && "
		 "{MCC} -B{B} -std=c89 -pedantic-errors -c {W}/ge2.c -o {W}/ge2.o 2>&1 | "
		 "grep -oE 'case ranges are a GNU extension'; "
		 "printf 'int f(void){ void*p=&&L; goto *p; L: return 0;}\\n' > {W}/ge3.c && "
		 "{MCC} -B{B} -std=c89 -pedantic-errors -c {W}/ge3.c -o {W}/ge3.o 2>&1 | "
		 "grep -oE 'taking the address of a label is a GNU extension'; "
		 "printf 'int f(int x){ return x?:1;}\\n' > {W}/ge4.c && "
		 "{MCC} -B{B} -std=c89 -pedantic-errors -c {W}/ge4.c -o {W}/ge4.o 2>&1 | "
		 "grep -oE 'ISO C forbids omitting the middle term of a .: expression'; "
		 "{MCC} -B{B} -std=gnu89 -c {W}/ge1.c -o {W}/ge1.o 2>&1 >/dev/null && echo GNU_OK; echo END",
		 "ISO C forbids braced-groups within expressions\ncase ranges are a GNU extension\ntaking the address of a label is a GNU extension\nISO C forbids omitting the middle term of a ?: expression\nGNU_OK\nEND\n"},

		{"c99_c11_feature_pedantic_gate", "",
		 "printf '_Bool b;\\nint main(void){return 0;}\\n' > {W}/cf1.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/cf1.c -o {W}/cf1.o 2>&1 | "
		 "grep -oE \"'_Bool' is a C99 feature\"; "
		 "printf '_Complex double z;\\nint main(void){return 0;}\\n' > {W}/cf2.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/cf2.c -o {W}/cf2.o 2>&1 | "
		 "grep -oE \"'_Complex' is a C99 feature\"; "
		 "printf '#define M(...) __VA_ARGS__\\nint x;\\n' > {W}/cf3.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/cf3.c -o {W}/cf3.o 2>&1 | "
		 "grep -oE 'variadic macros are a C99 feature'; "
		 "printf 'int x=_Generic(1,int:2,default:0);\\nint main(void){return x-2;}\\n' > {W}/cf4.c && "
		 "{MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/cf4.c -o {W}/cf4.o 2>&1 | "
		 "grep -oE \"'_Generic' is a C11 feature\"; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/cf4.c -o {W}/cf4.o 2>&1 >/dev/null && echo C11_OK; echo END",
		 "'_Bool' is a C99 feature\n'_Complex' is a C99 feature\nvariadic macros are a C99 feature\n'_Generic' is a C11 feature\nC11_OK\nEND\n"},

		{"dollar_in_identifier_pedantic", "",
		 "printf 'int a$b;\\nint main(void){return 0;}\\n' > {W}/di.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/di.c -o {W}/di.o 2>&1 | "
		 "grep -oE \"'.' in identifier\"; "
		 "printf 'int main(void){int a$b=7;return a$b-7;}\\n' > {W}/di2.c && "
		 "{MCC} -B{B} -I{I} {W}/di2.c -o {W}/di2 2>&1 >/dev/null && {W}/di2 && echo DOLLAR_RUN_OK; echo END",
		 "'$' in identifier\nDOLLAR_RUN_OK\nEND\n"},

		{"overlength_string_pedantic", "",
		 "{ printf 'char s[]=\"'; printf 'a%.0s' $(seq 510); printf '\";\\nint main(void){return 0;}\\n'; } > {W}/ols.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/ols.c -o {W}/ols.o 2>&1 | "
		 "grep -oE 'string literal of length 510 exceeds maximum length 509'; "
		 "{ printf 'char s[]=\"'; printf 'a%.0s' $(seq 509); printf '\";\\nint main(void){return 0;}\\n'; } > {W}/ok.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/ok.c -o {W}/ok.o 2>&1 >/dev/null && echo LEN509_OK; "
		 "{MCC} -B{B} -I{I} -c {W}/ols.c -o {W}/ols.o 2>&1 >/dev/null && echo DEFAULT_OK; echo END",
		 "string literal of length 510 exceeds maximum length 509\nLEN509_OK\nDEFAULT_OK\nEND\n"},

		{"extra_tokens_directive_pedantic", "",
		 "printf '#if 1\\n#endif junk\\nint x;\\nint main(void){return 0;}\\n' > {W}/et.c && "
		 "{MCC} -B{B} -I{I} -pedantic-errors -c {W}/et.c -o {W}/et.o 2>&1 | "
		 "grep -oE 'error: extra tokens after directive'; "
		 "{MCC} -B{B} -I{I} -c {W}/et.c -o {W}/et.o 2>&1 | "
		 "grep -oE 'warning: extra tokens after directive'; "
		 "{MCC} -B{B} -I{I} -c {W}/et.c -o {W}/et.o 2>/dev/null && echo DEFAULT_OK; "
		 "printf '#include <stdio.h>\\nint main(void){return 0;}\\n' > {W}/eth.c && "
		 "{MCC} -B{B} -I{I} -pedantic-errors -c {W}/eth.c -o {W}/eth.o 2>/dev/null && echo HDR_OK; echo END",
		 "error: extra tokens after directive\nwarning: extra tokens after directive\nDEFAULT_OK\nHDR_OK\nEND\n"},

		{"ordered_ptr_zero_warning", "",
		 "printf 'int a(int *p){ return p > 0; }\\nint b(int *p){ return p == 0; }\\nint main(void){return 0;}\\n' > {W}/pz.c && "
		 "{MCC} -B{B} -I{I} -Wextra -c {W}/pz.c -o {W}/pz.o 2>&1 | "
		 "grep -oE 'ordered comparison of pointer with integer zero'; "
		 "{MCC} -B{B} -I{I} -c {W}/pz.c -o {W}/pz.o 2>&1 | "
		 "grep -c 'integer zero'; echo END",
		 "ordered comparison of pointer with integer zero\n0\nEND\n"},

	{"return_local_addr_warning", "",
		 "printf 'int *f(void){ int x; return &x; }\\nint *g(int *b){ return &b[0]; }\\nint main(void){return 0;}\\n' > {W}/rl.c && "
		 "{MCC} -B{B} -I{I} -c {W}/rl.c -o {W}/rl.o 2>&1 | "
		 "grep -oE 'function returns address of local variable'; "
		 "{MCC} -B{B} -I{I} -Wno-return-local-addr -c {W}/rl.c -o {W}/rl.o 2>&1 | "
		 "grep -c 'address of local'; echo END",
		 "function returns address of local variable\n0\nEND\n"},

	{"undefined_internal_warning", "",
		 "printf 'static int helper(int);\\nint use(int y){ return helper(y); }\\nstatic int never_used(void);\\nint main(void){return use(0);}\\n' > {W}/ui.c && "
		 "{MCC} -B{B} -I{I} -c {W}/ui.c -o {W}/ui.o 2>&1 | "
		 "grep -oE \"'[a-z_]+' used but never defined\"; "
		 "{MCC} -B{B} -I{I} -Wno-undefined-internal -c {W}/ui.c -o {W}/ui.o 2>&1 | "
		 "grep -oE \"'[a-z_]+' used but never defined\"; echo END",
		 "'helper' used but never defined\nEND\n"},

	{"shift_count_warnings", "",
		 "printf 'int a=1<<40;\\nint b=1<<-1;\\nlong long c=1LL<<40;\\nunsigned e=1u>>33;\\nint main(void){return 0;}\\n' > {W}/sh.c && "
		 "{MCC} -B{B} -I{I} -c {W}/sh.c -o {W}/sh.o 2>&1 | "
		 "grep -oE 'left shift count >= width of type|left shift count is negative|right shift count >= width of type'; "
		 "{MCC} -B{B} -I{I} -Wno-shift-count-overflow -c {W}/sh.c -o {W}/sh.o 2>&1 | "
		 "grep -oE 'left shift count is negative'; echo END",
		 "left shift count >= width of type\nleft shift count is negative\nright shift count >= width of type\nleft shift count is negative\nEND\n"},

		{"wmain_return_type", "",
		 "printf 'void main(void){}\\n' > {W}/wm.c && "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/wm.c -o {W}/wm.o 2>&1 | grep -oE \"return type of .main. is not .int.\"; "
		 "{MCC} -B{B} -I{I} -Wall -Wno-main -c {W}/wm.c -o {W}/wm.o 2>&1 | grep -oE 'return type'; "
		 "{MCC} -B{B} -I{I} -c {W}/wm.c -o {W}/wm.o 2>&1 | grep -oE 'return type'; "
		 "printf 'int main(void){return 0;}\\n' > {W}/wmi.c && {MCC} -B{B} -I{I} -Wall -c {W}/wmi.c -o {W}/wmi.o 2>&1 | grep -oE 'return type'; echo END",
		 "return type of 'main' is not 'int'\nEND\n"},

	{"div_by_zero_warnings", "",
		 "printf 'int f(void){ return 5/0; }\\nint g(void){ return 5%%0; }\\nint main(void){return 0;}\\n' > {W}/dz.c && "
		 "{MCC} -B{B} -I{I} -c {W}/dz.c -o {W}/dz.o 2>&1 | grep -oE 'division by zero'; "
		 "test -f {W}/dz.o && echo COMPILED; "
		 "{MCC} -B{B} -I{I} -Wno-div-by-zero -c {W}/dz.c -o {W}/dz2.o 2>&1 | grep -oE 'division by zero'; echo NOWARN_DONE; "
		 "printf 'int q=sizeof(1/0);\\nint main(void){return 0;}\\n' > {W}/dz3.c && "
		 "{MCC} -B{B} -I{I} -c {W}/dz3.c -o {W}/dz3.o 2>&1 | grep -oE 'division by zero'; echo SIZEOF_DONE; "
		 "printf 'int a[5/0];\\nint main(void){return 0;}\\n' > {W}/dz4.c && "
		 "{MCC} -B{B} -I{I} -c {W}/dz4.c -o {W}/dz4.o 2>&1 | grep -oE 'division by zero in constant'; echo CONST_DONE",
		 "division by zero\ndivision by zero\nCOMPILED\nNOWARN_DONE\nSIZEOF_DONE\ndivision by zero in constant\nCONST_DONE\n"},

	{"hex_escape_no_digits_diag", "",
		 "printf 'char c[] = \"\\\\x\";\\n' > {W}/hx.c && "
		 "{MCC} -B{B} -I{I} -c {W}/hx.c -o {W}/hx.o 2>&1 | grep -oE 'used with no following hex digits'; "
		 "printf 'char d[] = \"\\\\u12\";\\n' > {W}/hu.c && "
		 "{MCC} -B{B} -I{I} -c {W}/hu.c -o {W}/hu.o 2>&1 | grep -oE 'universal-character-name expected'; "
		 "printf 'char e[] = \"\\\\x41\";\\nint main(void){return e[0];}\\n' > {W}/hv.c && "
		 "{MCC} -B{B} -I{I} -c {W}/hv.c -o {W}/hv.o && echo VALID_OK",
		 "used with no following hex digits\nuniversal-character-name expected\nVALID_OK\n"},

	{"old_style_definition_warning", "",
		 "printf 'int f(a,b) int a,b; { return a+b; }\\nint main(void){ return f(1,2); }\\n' > {W}/kr.c && "
		 "{MCC} -B{B} -I{I} -c {W}/kr.c -o {W}/kr.o 2>&1 | grep -oE 'old-style function definition'; "
		 "{MCC} -B{B} -I{I} -Wno-old-style-definition -c {W}/kr.c -o {W}/kr2.o 2>&1 | grep -oE 'old-style function definition'; echo NOWARN_DONE; "
		 "printf 'int g(){ return 0; }\\nint main(void){ return g(); }\\n' > {W}/ep.c && "
		 "{MCC} -B{B} -I{I} -c {W}/ep.c -o {W}/ep.o 2>&1 | grep -oE 'old-style function definition'; echo EMPTY_DONE",
		 "old-style function definition\nNOWARN_DONE\nEMPTY_DONE\n"},

	{"scalar_designated_init_diag", "",
		 "printf 'int x = {.foo=1};\\n' > {W}/sd1.c && "
		 "{MCC} -B{B} -I{I} -c {W}/sd1.c -o {W}/sd1.o 2>&1 | grep -oE 'field name not in record or union initializer'; "
		 "printf 'int y = {[0]=1};\\n' > {W}/sd2.c && "
		 "{MCC} -B{B} -I{I} -c {W}/sd2.c -o {W}/sd2.o 2>&1 | grep -oE 'array index in non-array initializer'; "
		 "printf 'struct S{int a;} s={.a=5};\\nint z={1};\\nint main(void){return s.a+z;}\\n' > {W}/sd3.c && "
		 "{MCC} -B{B} -I{I} -c {W}/sd3.c -o {W}/sd3.o && echo VALID_OK",
		 "field name not in record or union initializer\narray index in non-array initializer\nVALID_OK\n"},

	{"c11_keyword_feature_pedantic", "",
		 "printf '_Alignas(16) int x;\\nint main(void){return 0;}\\n' > {W}/k1.c && "
		 "{MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/k1.c -o {W}/k1.o 2>&1 | "
		 "grep -oE \"'_Alignas' is a C11 feature\"; "
		 "printf 'int x=_Alignof(int);\\nint main(void){return 0;}\\n' > {W}/k2.c && "
		 "{MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/k2.c -o {W}/k2.o 2>&1 | "
		 "grep -oE \"'_Alignof' is a C11 feature\"; "
		 "printf '_Noreturn void f(void){for(;;);}\\nint main(void){return 0;}\\n' > {W}/k3.c && "
		 "{MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/k3.c -o {W}/k3.o 2>&1 | "
		 "grep -oE \"'_Noreturn' is a C11 feature\"; "
		 "printf 'int x=__alignof__(int);\\nint main(void){return 0;}\\n' > {W}/k4.c && "
		 "{MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/k4.c -o {W}/k4.o 2>/dev/null && echo GNU_ALIGNOF_OK; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/k1.c -o {W}/k1.o 2>/dev/null && echo C11_OK; echo END",
		 "'_Alignas' is a C11 feature\n'_Alignof' is a C11 feature\n'_Noreturn' is a C11 feature\nGNU_ALIGNOF_OK\nC11_OK\nEND\n"},

		{"static_assert_no_message_c23", "",
		 "printf '_Static_assert(1);\\nint main(void){return 0;}\\n' > {W}/sa.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/sa.c -o {W}/sa.o 2>&1 | "
		 "grep -oE \"'_Static_assert' with no message is a C23 feature\"; "
		 "printf '_Static_assert(1,\"m\");\\nint main(void){return 0;}\\n' > {W}/sa2.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/sa2.c -o {W}/sa2.o 2>/dev/null && echo WITHMSG_OK; "
		 "{MCC} -B{B} -I{I} -std=c23 -c {W}/sa.c -o {W}/sa.o 2>/dev/null && echo C23_OK; echo END",
		 "'_Static_assert' with no message is a C23 feature\nWITHMSG_OK\nC23_OK\nEND\n"},

		{"duplicate_qualifier_c89", "",
		 "printf 'const const int x=1;\\nint main(void){return x-1;}\\n' > {W}/dq.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/dq.c -o {W}/dq.o 2>&1 | "
		 "grep -oE \"duplicate 'const' declaration specifier\"; "
		 "printf 'volatile volatile int y;\\nint main(void){return 0;}\\n' > {W}/dv.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/dv.c -o {W}/dv.o 2>&1 | "
		 "grep -oE \"duplicate 'volatile' declaration specifier\"; "
		 "{MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/dq.c -o {W}/dq.o 2>/dev/null && echo C99_ALLOWED; "
		 "{MCC} -B{B} -I{I} -c {W}/dq.c -o {W}/dq.o 2>/dev/null && echo DEFAULT_OK; echo END",
		 "duplicate 'const' declaration specifier\nduplicate 'volatile' declaration specifier\nC99_ALLOWED\nDEFAULT_OK\nEND\n"},

		{"duplicate_qualifier_wall_c99", "",
		 "printf 'const const int x=1;\\nvolatile volatile int y;\\nint main(void){return x-1;}\\n' > {W}/dqw.c && "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/dqw.c -o {W}/dqw.o 2>&1 | grep -oE \"duplicate '(const|volatile)' declaration specifier\"; "
		 "{MCC} -B{B} -I{I} -c {W}/dqw.c -o {W}/dqw.o 2>&1 | grep -oE 'duplicate'; echo END",
		 "duplicate 'const' declaration specifier\nduplicate 'volatile' declaration specifier\nEND\n"},

		{"tentative_array_pedantic", "",
		 "printf 'int a[];\\nint main(void){a[0]=5;return a[0]-5;}\\n' > {W}/ta.c && "
		 "{MCC} -B{B} -I{I} -std=c99 -pedantic -c {W}/ta.c -o {W}/ta.o 2>&1 | "
		 "grep -oE 'tentative array definition assumed to have one element'; "
		 "{MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/ta.c -o {W}/ta.o 2>/dev/null && echo NO_ESCALATE_OK; "
		 "{MCC} -B{B} -I{I} -c {W}/ta.c -o {W}/ta.o 2>/dev/null && echo DEFAULT_OK; echo END",
		 "tentative array definition assumed to have one element\nNO_ESCALATE_OK\nDEFAULT_OK\nEND\n"},

		{"tentative_array_warn_at_decl_line", "",
		 "printf 'int x;\\nint y;\\nint arr[];\\nint main(void){return arr[0];}\\n' > {W}/tal.c && "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/tal.c -o {W}/tal.o 2>&1 | grep -oE 'tal.c:[0-9]+: warning: array .arr. assumed to have one element'; echo END",
		 "tal.c:3: warning: array 'arr' assumed to have one element\nEND\n"},

		{"builtin_source_location", "",
		 "printf 'int main(void){ int l=__builtin_LINE(); const char *f=__builtin_FILE();"
		 " const char *fn=__builtin_FUNCTION(); long e=__builtin_expect_with_probability(l,0,0.9);"
		 " return (l==1 && f[0]!=0 && fn[0]!=0 && e==l)?0:1; }\\n' > {W}/loc.c && "
		 "{MCC} -B{B} -I{I} {W}/loc.c -o {W}/loc 2>/dev/null && {W}/loc && echo LOC_OK; echo END",
		 "LOC_OK\nEND\n"},

		{"builtin_expect_is_code_neutral", "optimizer",
		 "printf 'int g;\\nint f(void){ if (__builtin_expect(!!(g==0),0)) return 1; return 2; }\\n' > {W}/be.c && "
		 "{MCC} -B{B} -I{I} -w -O1 -c {W}/be.c -o {W}/beA.o && "
		 "printf 'int g;\\nint f(void){ if (__builtin_expect_with_probability(!!(g==0),0,0.9)) return 1; return 2; }\\n' > {W}/be.c && "
		 "{MCC} -B{B} -I{I} -w -O1 -c {W}/be.c -o {W}/beC.o && "
		 "printf 'int g;\\nint f(void){ if (!!(g==0)) return 1; return 2; }\\n' > {W}/be.c && "
		 "{MCC} -B{B} -I{I} -w -O1 -c {W}/be.c -o {W}/beB.o && "
		 /* Assert NO materialization bloat (the invariant this test guards --
		  * __builtin_expect{,_with_probability} must not emit the wrapped
		  * comparison as extra code) via OBJECT SIZE, not byte-identity. Byte
		  * identity also flips on legitimate, backend-dependent block-layout
		  * reordering from the branch hint (present on some targets, absent on
		  * win-x64 where mcc matches win-gcc's identical -O1 output) -- size is
		  * layout-invariant, so this stays portable while still catching a
		  * materialized comparison (which would enlarge .text). */
		 "a=$(wc -c < {W}/beA.o); b=$(wc -c < {W}/beB.o); c=$(wc -c < {W}/beC.o); "
		 "{ [ \"$a\" -eq \"$b\" ] && echo expect=nobloat || echo expect=BLOAT; }; "
		 "{ [ \"$c\" -eq \"$b\" ] && echo prob=nobloat || echo prob=BLOAT; }; echo END",
		 "expect=nobloat\nprob=nobloat\nEND\n"},

		{"reverse_sso_initializer", "",
		 "printf 'extern int printf(const char*,...);\\nstruct S { int x; short i:12; char c1:1,c2:1,c3:1,c4:1; } __attribute__((scalar_storage_order(\"big-endian\")));\\nstruct S g = { 0x12345678, 341, 1,1,1,1 };\\nint main(void){ struct S s = { 0x12345678, 341, 1,1,1,1 }; unsigned char *p=(unsigned char*)&g,*q=(unsigned char*)&s; printf(\"%%02x%%02x %%d | %%02x%%02x %%d\\\\n\", p[0],p[1],p[4], q[0],q[1],q[4]); return 0; }\\n' > {W}/rs.c && "
		 "{MCC} -B{B} -nostdinc {W}/rs.c -o {W}/rs && {W}/rs",
		 "1234 21 | 1234 21\n"},

		{"always_inline_out_of_line_emit", "",
		 "printf 'inline __attribute__((always_inline)) int add(int a,int b){ return a+b; }\\nint main(void){ return add(2,3)==5 ? 0 : 1; }\\n' > {W}/ai.c && "
		 "printf 'inline int pf(int a){ return a+1; }\\nint main(void){ return pf(4); }\\n' > {W}/pi.c && "
		 "{ {MCC} -B{B} -nostdinc {W}/ai.c -o {W}/ai && {W}/ai && echo ai=OK || echo ai=FAIL; "
		 "{MCC} -B{B} -nostdinc {W}/pi.c -o {W}/pi 2>/dev/null && echo pi=LINK || echo pi=noemit; }",
		 "ai=OK\npi=noemit\n"},

		{"local_label_address", "",
		 "printf 'int f(int x){ __label__ a,b; static void*jt[2]; jt[0]=&&a; jt[1]=&&b; goto *jt[x]; a: return 1; b: return 2; }\\nint main(void){ return (f(0)==1 && f(1)==2) ? 0 : 1; }\\n' > {W}/ll.c && "
		 "{ {MCC} -B{B} -nostdinc {W}/ll.c -o {W}/ll && {W}/ll && echo OK || echo FAIL; }",
		 "OK\n"},

		{"builtin_expect_side_effect", "",
		 "printf 'int x,y;\\nint foo(int z){ if(__builtin_expect(x ? y!=0 : 0, z++)) return 7; return z; }\\nint main(){ x=1; y=0; return foo(10)==11 ? 0 : 1; }\\n' > {W}/bx.c && "
		 "{ {MCC} -B{B} -nostdinc {W}/bx.c -o {W}/bx && {W}/bx && echo OK || echo FAIL; }",
		 "OK\n"},

		{"builtin_inline_mem_and_retaddr", "",
		 "printf 'int main(void){ char a[8]={0}, b[8]={1,2,3,4,5,6,7,8}, c[8], d[4];"
		 " __builtin_memcpy_inline(a,b,8); __builtin_memmove_inline(c,a,8); __builtin_memset_inline(d,9,4);"
		 " void *r=__builtin_frob_return_addr(__builtin_extract_return_addr(__builtin_return_address(0)));"
		 " return (a[0]==1 && a[7]==8 && c[3]==4 && d[0]==9 && d[3]==9 && r!=0)?0:1; }\\n' > {W}/mi.c && "
		 "{MCC} -B{B} -I{I} {W}/mi.c -o {W}/mi 2>/dev/null && {W}/mi && echo MI_OK; echo END",
		 "MI_OK\nEND\n"},

		{"builtin_integer_abs", "",
		 "printf 'int main(void){ volatile int i=-7; volatile long l=-1234567L; volatile long long ll=-9876543210LL;"
		 " static const int fs=__builtin_abs(-42);"
		 " return (__builtin_abs(i)==7 && __builtin_abs(5)==5 && __builtin_labs(l)==1234567L"
		 " && __builtin_llabs(ll)==9876543210LL && fs==42)?0:1; }\\n' > {W}/ab.c && "
		 "{MCC} -B{B} -I{I} {W}/ab.c -o {W}/ab 2>/dev/null && {W}/ab && echo ABS_OK; echo END",
		 "ABS_OK\nEND\n"},

		{"builtin_isnormal_fpclassify", "",
		 "printf '#define C(x) __builtin_fpclassify(0,1,4,3,2,(x))\\n"
		 "int main(void){ volatile double n=1.5,z=0.0,sub=5e-320,big=1e308; double inf=big*10, nan=z; nan=nan/z;\\n"
		 " volatile float f=2.0f;\\n"
		 " int ok=__builtin_isnormal(n) && !__builtin_isnormal(z) && !__builtin_isnormal(sub)"
		 " && !__builtin_isnormal(inf) && !__builtin_isnormal(nan) && __builtin_isnormal(f);\\n"
		 " int c=C(n)*10000+C(z)*1000+C(sub)*100+C(inf)*10+C(nan);\\n"
		 " return (ok && c==42310)?0:1; }\\n' > {W}/fc.c && "
		 "{MCC} -B{B} -I{I} {W}/fc.c -o {W}/fc 2>/dev/null && {W}/fc && echo FC_OK; echo END",
		 "FC_OK\nEND\n"},

		{"builtin_object_size", "",
		 "printf 'int main(void){ char b[8];"
		 " unsigned long s0=__builtin_object_size(b,0), s2=__builtin_object_size(b,2);"
		 " unsigned long d0=__builtin_dynamic_object_size(b,0);"
		 " int v=__builtin_speculation_safe_value(42), v2=__builtin_speculation_safe_value(7,0);"
		 " return (s0==(unsigned long)-1 && s2==0 && d0==(unsigned long)-1 && v==42 && v2==7)?0:1; }\\n' > {W}/os.c && "
		 "{MCC} -B{B} -I{I} {W}/os.c -o {W}/os 2>/dev/null && {W}/os && echo OS_OK; "
		 "printf '#include <string.h>\\nint main(void){ char d[16]; memcpy(d,\"hi\",3); return d[0]!=104; }\\n' > {W}/ft.c && "
		 "{MCC} -B{B} -I{I} -D_FORTIFY_SOURCE=2 -O2 {W}/ft.c -o {W}/ft 2>/dev/null && {W}/ft && echo FORTIFY_OK; echo END",
		 "OS_OK\nFORTIFY_OK\nEND\n"},

		{"builtin_trap", "",
		 "printf 'int main(int c,char**v){ (void)v; if(c>100) return 1; __builtin_trap(); return 0; }\\n' > {W}/tr.c && "
		 "{MCC} -B{B} -I{I} {W}/tr.c -o {W}/tr 2>&1 >/dev/null && "
		 "{ {W}/tr 2>/dev/null; [ $? -gt 128 ] && echo TRAPPED; }; echo END",
		 "TRAPPED\nEND\n"},

		{"builtin_prefetch_assume_aligned", "",
		 "printf 'int main(void){ int a[4]={1,2,3,4}; int i=0; __builtin_prefetch(&a[i++]); __builtin_prefetch(&a[0],1,3);"
		 " int *p=(int*)__builtin_assume_aligned(a,16); int *q=(int*)__builtin_assume_aligned(a,16,0);"
		 " return (i==1 && p[2]==3 && q[0]==1)?0:1; }\\n' > {W}/bi.c && "
		 "{MCC} -B{B} -I{I} {W}/bi.c -o {W}/bi 2>&1 >/dev/null && {W}/bi && echo BUILTIN_OK; echo END",
		 "BUILTIN_OK\nEND\n"},

		{"stdc_utf_encoding_macros", "",
		 "printf '\\n' > {W}/utf.c && {MCC} -B{B} -E -dM {W}/utf.c | grep -cE '^#define __STDC_UTF_(16|32)__ 1$'",
		 "2\n"},

		{"nostdinc_drops_system", "os!=WIN32",
		 "printf '#include <stdio.h>\\n' > {W}/ns.c && {MCC} -B{B} -nostdinc -E {W}/ns.c 2>&1 | grep -coE 'not found|No such'",
		 "1\n"},

		{"empty_aggregate_global_distinct_addr", "",
		 "printf 'struct E{};struct E a,b;union U{};union U c,d;int main(void){return ((&a!=&b)+(&c!=&d)==2 && sizeof(struct E)==0 && sizeof(union U)==0)?0:1;}\\n' > {W}/eag.c && {MCC} -B{B} {W}/eag.c -o {W}/eag && {W}/eag && echo EAG_OK",
		 "EAG_OK\n"},

		{"asm_empty_clobber_section", "",
		 "printf 'int f(void){int x=0;asm volatile(\"\":\"=m\"(x)::);asm volatile(\"\":::);return x;}\\n' > {W}/aec.c && {MCC} -B{B} -c {W}/aec.c -o {W}/aec.o && echo AEC_OK",
		 "AEC_OK\n"},

		{"u32char_no_surrogate_fuse", "",
		 "printf 'int main(void){return (U'\\''\\\\xD800\\\\xDC00'\\''==0xDC00 && U'\\''\\\\U0001F600'\\''==0x1F600)?0:1;}\\n' > {W}/u32c.c && {MCC} -B{B} -run {W}/u32c.c && echo U32C_OK",
		 "U32C_OK\n"},

		{"frounding_math_disables_sqrt_fold", "",
		 "printf 'double f(void){return __builtin_sqrt(4.0);}\\n' > {W}/frm.c && "
		 "{MCC} -B{B} -S -O1 {W}/frm.c -o - 2>/dev/null | grep -q sqrt && echo BAD_DEFAULT_NOT_FOLDED; "
		 "{MCC} -B{B} -S -O1 -frounding-math {W}/frm.c -o - 2>/dev/null | grep -q sqrt && echo FRM_OK",
		 "FRM_OK\n"},

		{"bcheck_bcopy_index_bounds", "os!=WIN32",
		 "printf '#include <strings.h>\\n#include <string.h>\\nint main(void){char a[4],b[64];memset(b,1,64);bcopy(b,a,64);return 0;}\\n' > {W}/bcb.c && "
		 "{MCC} -B{B} -b -run {W}/bcb.c 2>&1 | grep -q 'BCHECK: invalid pointer' && echo BCB_OK",
		 "BCB_OK\n"},

		{"dumpmachine", "os!=WIN32",
		 "{MCC} -dumpmachine | grep -qE '^(x86_64|i386|i686|aarch64|arm64|arm|riscv64)-' && echo TRIPLE_OK",
		 "TRIPLE_OK\n"},

		{"dumpversion_format", "",
		 "{MCC} -dumpversion | grep -qE '^[0-9]{1,3}\\.[0-9]+\\.[0-9]+$' && echo VER_OK",
		 "VER_OK\n"},

		{"inline_main_diag", "",
		 "printf 'inline int main(void){return 0;}\\n' > {W}/im.c && "
		 "{MCC} -B{B} -c {W}/im.c -o {W}/im.o 2>&1 | "
		 "grep -c 'not allowed to be declared inline'",
		 "1\n"},

		{"static_in_inline_pedantic", "",
		 "printf 'static void s(void){}\\ninline int f(void){s();return 0;}\\n"
		 "int g(void){return f();}\\n' > {W}/si.c && "
		 "{MCC} -B{B} -pedantic -c {W}/si.c -o {W}/si.o 2>&1 | grep -c 'internal linkage'",
		 "1\n"},

		{"forward_alias", "",
		 "{MCC} -B{B} -run {D}/fwdalias.c",
		 "back\nfwd\n"},

		{"apple_arm64_long_double_is_double", "cpu=arm64,os=Darwin",
		 "{MCC} -B{B} -run {D}/appleldouble.c",
		 "1\n"},

		{"macho_fat64_dylib", "os=Darwin",
		 "printf 'int ff(int x){return x+100;}\\n' > {W}/ffl.c && "
		 "clang -arch arm64 -dynamiclib {W}/ffl.c -o {W}/fa.dylib && "
		 "clang -arch x86_64 -dynamiclib {W}/ffl.c -o {W}/fx.dylib && "
		 "lipo -create -fat64 {W}/fa.dylib {W}/fx.dylib -o {W}/ffat.dylib && "
		 "printf 'extern int ff(int); int main(void){return ff(-100);}\\n' > {W}/ffm.c && "
		 "{MCC} -B{B} {W}/ffm.c {W}/ffat.dylib -o {W}/ffm && {W}/ffm; echo $?",
		 "0\n"},

		{"const_modify_is_error", "",
		 "printf 'int main(void){const int x=3; x=4; return x;}\\n' > {W}/cm.c && "
		 "{MCC} -B{B} -c {W}/cm.c -o {W}/cm.o 2>&1 | grep -c 'error: assignment of read-only'",
		 "1\n"},

		{"nonscalar_same_type_cast_pedantic", "",
		 "printf 'struct S{int a;}; int main(void){struct S s={1}; struct S t=(struct S)s; return t.a;}\\n' > {W}/nc.c && "
		 "{MCC} -B{B} -c {W}/nc.c -o {W}/nc0.o 2>&1 | grep -c forbids; "
		 "{MCC} -B{B} -pedantic -c {W}/nc.c -o {W}/nc.o 2>&1 | grep -c 'forbids casting nonscalar'",
		 "0\n1\n"},

		{"print_search_dirs", "",
		 "{MCC} -B{B} -print-search-dirs | grep -oE '^(install|include|libraries):'",
		 "install:\ninclude:\nlibraries:\n"},

		{"ar_create_list", "",
		 "{MCC} -B{B} -I{I} -c {D}/lib.c -o {W}/al.o && {MCC} -B{B} -I{I} -c {D}/sec.c -o {W}/as.o && "
		 "{MCC} -ar rcs {W}/libcli.a {W}/al.o {W}/as.o && {MCC} -ar t {W}/libcli.a",
		 "al.o\nas.o\n"},

		{"response_file", "",
		 "printf -- '-c {D}/lib.c -o {W}/resp.o\\n' > {W}/a.rsp && {MCC} -B{B} -I{I} @{W}/a.rsp && "
		 "nm {W}/resp.o | grep -oE 'exported_fn'",
		 "exported_fn\n"},

		{"symbol_type_func_object", "os!=WIN32",
		 "{MCC} -B{B} -I{I} -c {D}/lib.c -o {W}/ts.o && "
		 "if [ \"$MCC_TEST_OS\" = Darwin ]; then "
		 "nm {W}/ts.o | grep -E '_exported_fn|_global_var' | "
		 "awk '{print ($(NF-1)==\"T\"||$(NF-1)==\"t\")?\"FUNC\":\"OBJECT\"}' | sort -u; "
		 "else readelf -s {W}/ts.o | grep -E 'exported_fn|global_var' | awk '{print $4}' | sort -u; fi",
		 "FUNC\nOBJECT\n"},

		{"assemble_dot_s_file", "cpu=x86_64,os=linux,asm",
		 "{MCC} -B{B} -I{I} {D}/asmadd.s {D}/asmmain.c -o {W}/ae && {W}/ae",
		 "42\n"},

		{"asm_binary_literal_0b", "cpu=x86_64,asm",
		 "{MCC} -B{B} -I{I} {D}/binlit.s {D}/binlitmain.c -o {W}/ble && {W}/ble",
		 "OK\n"},

		{"weak_override_multi_tu", "os!=WIN32",
		 "{MCC} -B{B} -I{I} -c {D}/wstrong.c -o {W}/wstrong.o && "
		 "{MCC} -B{B} -I{I} {D}/wmain.c {W}/wstrong.o -o {W}/we && {W}/we",
		 "1\n"},

		{"shared_dynamic_tags", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -shared -Wl,-soname,libt.so.1 {D}/lib.c -o {W}/lt.so && "
		 "readelf -d {W}/lt.so | grep -oE 'SONAME|GNU_HASH|BIND_NOW' | sort -u && "
		 "readelf -l {W}/lt.so | grep -oE 'GNU_RELRO' | head -1",
		 "BIND_NOW\nGNU_HASH\nSONAME\nGNU_RELRO\n"},

		{"rpath_new_dtags_runpath", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -Wl,-rpath,/opt/x -Wl,--enable-new-dtags -shared {D}/lib.c -o {W}/rp.so && "
		 "readelf -d {W}/rp.so | grep -oE 'RUNPATH'",
		 "RUNPATH\n"},

		{"dwarf_line_table", "cpu=x86_64,os=linux",
		 "{MCC} -B{B} -I{I} -gdwarf-5 -c {D}/lib.c -o {W}/gl.o && "
		 "readelf --debug-dump=decodedline {W}/gl.o 2>/dev/null | grep -oE 'lib\\.c' | head -1",
		 "lib.c\n"},

		{"tls_segment_and_run", "os=linux",
		 "{MCC} -B{B} -I{I} {D}/tlsvar.c -o {W}/te && {W}/te && "
		 "readelf -l {W}/te | grep -oE 'TLS' | head -1",
		 "7\nTLS\n"},

		{"fcommon_vs_default", "cpu=x86_64,os=linux",
		 "printf 'int gg;\\n' > {W}/cm.c && "
		 "{MCC} -B{B} -I{I} -c {W}/cm.c -o {W}/cm.o && nm {W}/cm.o | awk '/ gg$/{print $2}' && "
		 "{MCC} -B{B} -I{I} -fcommon -c {W}/cm.c -o {W}/cmc.o && nm {W}/cmc.o | awk '/ gg$/{print $2}'",
		 "B\nC\n"},

		{"werror_promotes_to_error", "",
		 "printf 'int main(void){ undeclared_fn(); return 0; }\\n' > {W}/werr.c && "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/werr.c -o {W}/werr.o 2>&1 | grep -oE 'error: implicit declaration' | head -1",
		 "error: implicit declaration\n"},

		{"wwrite_strings_warns", "",
		 "printf 'char *p = \"x\"; void f(void){ *p = 0; }\\n' > {W}/ws.c && "
		 "{MCC} -B{B} -I{I} -Wwrite-strings -c {W}/ws.c -o {W}/ws.o 2>&1 | grep -coE 'discards qualifiers|read-only'",
		 "1\n"},

		{"multichar_warning", "",
		 "{MCC} -B{B} -I{I} -c {D}/multichar.c -o {W}/mc.o 2>&1 | grep -oE 'multi-character'",
		 "multi-character\n"},

		{"u16char_ucn_not_encodable", "",
		 "printf '%s\\n' \"int c = u'\\\\U0001F600';\" > {W}/u16u.c && "
		 "{MCC} -B{B} -I{I} -std=c23 -c {W}/u16u.c -o {W}/u16u.o 2>&1 | grep -oE 'character not encodable in a single code unit'",
		 "character not encodable in a single code unit\n"},

		{"u16char_ucn_string_silent", "",
		 "printf '%s\\n' \"unsigned short s[] = u\\\"\\\\U0001F600\\\";\" > {W}/u16s.c && "
		 "{MCC} -B{B} -I{I} -std=c23 -c {W}/u16s.c -o {W}/u16s.o 2>&1 | grep -cE 'encodable|out of range'",
		 "0\n"},

		{"integer_suffix_error", "",
		 "{MCC} -B{B} -I{I} -c {D}/suffix_bad.c -o {W}/sb.o 2>&1 | grep -oE \"three 'l's\"",
		 "three 'l's\n"},

		{"include_flag", "",
		 "printf '#define INCV 7\\n' > {W}/inc.h && printf 'int x = INCV;\\n' > {W}/iu.c && "
		 "{MCC} -B{B} -I{I} -include {W}/inc.h -E -P {W}/iu.c | grep -oE 'int x = 7'",
		 "int x = 7\n"},

		{"isystem_include", "",
		 "mkdir -p {W}/sysinc && printf '#define SYSVAL 11\\n' > {W}/sysinc/syshdr.h && "
		 "printf '#include <syshdr.h>\\nint v = SYSVAL;\\n' > {W}/ui.c && "
		 "{MCC} -B{B} -I{I} -isystem {W}/sysinc -E -P {W}/ui.c | grep -oE 'int v = 11'",
		 "int v = 11\n"},

		{"pragma_comment_lib", "",
		 "printf '#pragma comment(lib,\"m\")\\nint main(void){ return 0; }\\n' > {W}/pc.c && "
		 "{MCC} -B{B} -I{I} {W}/pc.c -o {W}/pce && echo OK",
		 "OK\n"},

		{"x_force_language", "",
		 "printf '#include <stdio.h>\\nint main(void){ puts(\"xc\"); return 0; }\\n' > {W}/notc.txt && "
		 "{MCC} -B{B} -I{I} -x c {W}/notc.txt -o {W}/xce && {W}/xce",
		 "xc\n"},

		{"atomic_type_constraints", "",
		 "printf '_Atomic(int[3]) a;\\n' > {W}/ata.c && "
		 "printf '_Atomic(int(void)) f;\\n' > {W}/atf.c && "
		 "printf 'typedef int A[3]; _Atomic A a;\\n' > {W}/atq.c && "
		 "printf 'typedef int F(void); _Atomic F f;\\n' > {W}/atqf.c && "
		 "printf 'typedef int* P; _Atomic P p; _Atomic int v[3]; int main(void){return 0;}\\n' > {W}/atok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/ata.c -o {W}/ata.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/atf.c -o {W}/atf.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/atq.c -o {W}/atq.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/atqf.c -o {W}/atqf.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/atok.c -o {W}/atok 2>&1 && echo VALID_OK; } | "
		 "grep -oE '_Atomic cannot be applied to an? (array|function) type|VALID_OK' | sort -u",
		 "VALID_OK\n_Atomic cannot be applied to a function type\n_Atomic cannot be applied to an array type\n"},

		{"storage_specifier_constraints", "",
		 "printf 'inline int x;\\n' > {W}/sc1.c && "
		 "printf '_Thread_local void f(void);\\n' > {W}/sc2.c && "
		 "printf 'void g(void){ _Thread_local int y; (void)y; }\\n' > {W}/sc3.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/sc1.c -o {W}/sc1.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/sc2.c -o {W}/sc2.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/sc3.c -o {W}/sc3.o 2>&1; } | "
		 "grep -oE \"'(inline|_Thread_local)'.*\" | sort -u",
		 "'_Thread_local' applied to a function\n'_Thread_local' at block scope requires 'static' or 'extern'\n'inline' used outside of a function declaration\n"},

		{"bitfield_operand_constraints", "",
		 "printf 'struct S{int b:3;}s; int *p(void){return &s.b;}\\n' > {W}/bf1.c && "
		 "printf 'struct S{int b:3;}s; int n(void){return (int)sizeof(s.b);}\\n' > {W}/bf2.c && "
		 "printf 'struct S{int b:3;}s; int a(void){return (int)_Alignof(s.b);}\\n' > {W}/bf3.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/bf1.c -o {W}/bf1.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/bf2.c -o {W}/bf2.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/bf3.c -o {W}/bf3.o 2>&1; } | "
		 "grep -oE '(cannot take address of|(sizeof|_Alignof). cannot be applied to a) bit-field' | sort -u",
		 "_Alignof' cannot be applied to a bit-field\ncannot take address of bit-field\nsizeof' cannot be applied to a bit-field\n"},

		{"integer_constant_expr_type", "",
		 "printf 'int f(int x){switch(x){case 1.5: return 1; default: return 0;}}\\n' > {W}/ic1.c && "
		 "printf 'enum E{A=1.5};\\n' > {W}/ic2.c && "
		 "printf 'int v[(int)1.5];int main(void){return (int)sizeof v;}\\n' > {W}/ic3.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/ic1.c -o {W}/ic1.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/ic2.c -o {W}/ic2.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/ic3.c -o {W}/ic3 2>&1 && echo CAST_OK; } | "
		 "grep -oE 'integer constant expression must have integer type|CAST_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CAST_OK\n2 integer constant expression must have integer type\n"},

		{"integer_constant_overflow", "",
		 "printf 'unsigned long long a=99999999999999999999;\\n"
		 "unsigned long long b=0xFFFFFFFFFFFFFFFF0;\\nint main(void){return (a!=0)&&(b!=0)?0:1;}\\n' > {W}/ov.c && "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/ov.c -o {W}/ov 2>&1 | grep -c 'integer constant overflow'; "
		 "{W}/ov && echo OVF_RUN_OK",
		 "2\nOVF_RUN_OK\n"},

		{"float_pointer_cast_constraint", "",
		 "printf 'void *p(double d){return (void*)d;}\\n' > {W}/fp1.c && "
		 "printf 'double g(void*q){return (double)q;}\\n' > {W}/fp2.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/fp1.c -o {W}/fp1.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/fp2.c -o {W}/fp2.o 2>&1; } | "
		 "grep -oE 'cannot cast between a floating type and a pointer' | sort | uniq -c | sed 's/^ *//'",
		 "2 cannot cast between a floating type and a pointer\n"},

		{"generic_duplicate_assoc", "",
		 "printf 'int f(void){return _Generic(1,long:1,long:2,int:3);}\\n' > {W}/gd.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/gd.c -o {W}/gd.o 2>&1 | "
		 "grep -oE '_Generic specifies two compatible types'",
		 "_Generic specifies two compatible types\n"},

		{"generic_assoc_type_completeness", "",
		 "printf 'void h(int n){(void)_Generic(1,int[n]:1,default:2);(void)n;}\\n' > {W}/gv.c && "
		 "printf 'int x=_Generic(1,int(void):1,int:2,default:3);\\n' > {W}/gf.c && "
		 "printf 'int f(void){return _Generic(1,int:0,double:1,default:2);}\\n' > {W}/gok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/gv.c -o {W}/gv.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/gf.c -o {W}/gf.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/gok.c -o {W}/gok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'variably modified type|association with a function type|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 association with a function type\n1 variably modified type\n"},

		{"file_scope_storage_class", "",
		 "printf 'auto int x;\\n' > {W}/fs1.c && "
		 "printf 'register int y;\\n' > {W}/fs2.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/fs1.c -o {W}/fs1.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/fs2.c -o {W}/fs2.o 2>&1; } | "
		 "grep -oE \"file-scope declaration of '.' specifies '(auto|register)'\" | sort",
		 "file-scope declaration of 'x' specifies 'auto'\nfile-scope declaration of 'y' specifies 'register'\n"},

		{"restrict_requires_pointer", "",
		 "printf 'int restrict x;\\n' > {W}/rr1.c && "
		 "printf 'typedef int* IP; restrict IP q; int *restrict p;\\nint main(void){return !!p+!!q;}\\n' > {W}/rr2.c && "

		 "printf 'void (*restrict fp)(void);\\n' > {W}/rr3.c && "
		 "printf 'int (*restrict pa)[3]; int *restrict *pp;\\nint main(void){return !!pa+!!pp;}\\n' > {W}/rr4.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/rr1.c -o {W}/rr1.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/rr3.c -o {W}/rr3.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/rr2.c -o {W}/rr2 2>&1 && echo PTR_OK; "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/rr4.c -o {W}/rr4 2>&1 && echo OBJPTR_OK; } | "
		 "grep -oE \"'restrict' requires a pointer type|pointer to function type may not be 'restrict'-qualified|PTR_OK|OBJPTR_OK\"",
		 "'restrict' requires a pointer type\npointer to function type may not be 'restrict'-qualified\nPTR_OK\nOBJPTR_OK\n"},

		{"alignas_constraints", "",
		 "printf 'typedef _Alignas(16) int T;\\n' > {W}/aa1.c && "
		 "printf '_Alignas(16) void f(void);\\n' > {W}/aa2.c && "
		 "printf '_Alignas(1) double d;\\n' > {W}/aa3.c && "
		 "printf 'struct S{_Alignas(16) int b:3;};\\n' > {W}/aa4.c && "
		 "printf 'void f(_Alignas(16) int x);\\n' > {W}/aa6.c && "
		 "printf '_Alignas(64) int a; int main(void){return (int)_Alignof(a);}\\n' > {W}/aa5.c && "
		 "{ for f in aa1 aa2 aa3 aa4 aa6; do {MCC} -B{B} -I{I} -std=c11 -c {W}/$f.c -o {W}/$f.o 2>&1; done; "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/aa5.c -o {W}/aa5 2>&1 && echo OVERALIGN_OK; } | "
		 "grep -oE \"'_Alignas' specified for a (typedef|function|bit-field|function parameter)|requested alignment is less than the minimum alignment of the type|OVERALIGN_OK\" | sort",
		 "'_Alignas' specified for a bit-field\n'_Alignas' specified for a function\n'_Alignas' specified for a function parameter\n'_Alignas' specified for a typedef\nOVERALIGN_OK\nrequested alignment is less than the minimum alignment of the type\n"},

		{"pragma_stdc_recognized", "",
		 "printf '#pragma STDC FP_CONTRACT ON\\n#pragma STDC FENV_ACCESS OFF\\n"
		 "#pragma STDC CX_LIMITED_RANGE DEFAULT\\nint main(void){return 0;}\\n' > {W}/ps.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -Wall -Werror -c {W}/ps.c -o {W}/ps.o && echo OK; "
		 "printf '#pragma frobnicate q\\nint main(void){return 0;}\\n' > {W}/pf.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -Wall -c {W}/pf.c -o {W}/pf.o 2>&1 | grep -oE 'frobnicate ignored'",
		 "OK\nfrobnicate ignored\n"},

		{"knr_implicit_int_param", "",
		 "printf 'int f(x){ return x; }\\nint h(a,b) int a; char b; { return a+b; }\\n"
		 "int main(void){return f(1)+h(2,3);}\\n' > {W}/kr.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/kr.c -o {W}/kr.o 2>&1 | "
		 "grep -c \"type of 'x' defaults to 'int'\"",
		 "1\n"},

		{"star_array_in_funcdef", "",
		 "printf 'void f(int a[*]){(void)a;}\\n' > {W}/sd.c && "
		 "printf 'void p(int a[*]); void g(void(*fp)(int a[*])){(void)fp;}\\n"
		 "int n=3; void h(int a[n],int b[static 3],int c[]){(void)a;(void)b;(void)c;}\\n"
		 "int main(void){return 0;}\\n' > {W}/sp.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/sd.c -o {W}/sd.o 2>&1 | "
		 "grep -oE \"'\\[\\*\\]' not allowed in a function definition\"; "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/sp.c -o {W}/sp && echo PROTO_OK",
		 "'[*]' not allowed in a function definition\nPROTO_OK\n"},

		{"array_param_static_outermost", "",
		 "printf 'void f(int (*a)[static 3]){(void)a;}\\n' > {W}/b1.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/b1.c -o {W}/b1.o 2>&1 | "
		 "grep -oE 'non-outermost array declarator'; "
		 "printf 'void g(int a[3][static 3]){(void)a;}\\n' > {W}/b2.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/b2.c -o {W}/b2.o 2>&1 | "
		 "grep -oE 'non-outermost array declarator'; "
		 "printf 'int n=3;\\nvoid ok(int a[static 3],int b[const static 3],"
		 "int c[static 3][4],int d[static n]){(void)a;(void)b;(void)c;(void)d;}\\n"
		 "int main(void){return 0;}\\n' > {W}/b3.c && "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/b3.c -o {W}/b3 && echo OUTERMOST_OK",
		 "non-outermost array declarator\nnon-outermost array declarator\nOUTERMOST_OK\n"},

		{"jump_into_vla_scope", "",
		 "printf 'int n=3; int f(void){goto L; { int a[n]; L: return sizeof a; } }\\n' > {W}/j1.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/j1.c -o {W}/j1.o 2>&1 | "
		 "grep -oE 'variably modified declaration'; "
		 "printf 'int n=3; int g(int c){switch(c){ int a[n]; case 1: return sizeof a; default: return 0;} }\\n' > {W}/j2.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/j2.c -o {W}/j2.o 2>&1 | "
		 "grep -oE 'variably modified declaration'; "
		 "printf 'int n=3; int h(void){ { int a[n]; L: (void)a; } goto L; return 0; }\\n' > {W}/j3.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/j3.c -o {W}/j3.o 2>&1 | "
		 "grep -oE 'variably modified declaration'; "
		 "printf 'int n=3; int p(void){ goto L; int (*q)[n]; L: q=0; return !!q; }\\n' > {W}/j5.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/j5.c -o {W}/j5.o 2>&1 | "
		 "grep -oE 'variably modified declaration'; "
		 "printf 'int n=3;\\nint ok(int c){ int a[n]; L: if(a[0]) goto L;"
		 " switch(c){ case 1: { int b[n]; return sizeof b; } default: return sizeof a; } }\\n"
		 "int main(void){return 0;}\\n' > {W}/j4.c && "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/j4.c -o {W}/j4 && echo VALID_OK",
		 "variably modified declaration\nvariably modified declaration\nvariably modified declaration\nvariably modified declaration\nVALID_OK\n"},

		{"jump_into_vla_scope_ext", "",
		 "printf 'int n=3,m=4; int f(void){ goto L; int (*p)[n][m]; L: p=0; return !!p; }\\n' > {W}/e1.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/e1.c -o {W}/e1.o 2>&1 | "
		 "grep -oE 'variably modified declaration'; "
		 "printf 'int n=3; int f(void){ goto L; typedef int VM[n]; VM a; L: return sizeof a; }\\n' > {W}/e2.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/e2.c -o {W}/e2.o 2>&1 | "
		 "grep -oE 'variably modified declaration'; "
		 "printf 'int n=3; int f(void){ goto L; int a[n][n]; L: return sizeof a; }\\n' > {W}/e4.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/e4.c -o {W}/e4.o 2>&1 | "
		 "grep -oE 'variably modified declaration'; "
		 "printf 'int n=3; int g(int c){ switch(c){ case 0: { int a[n]; case 1: return sizeof a; } default: return 0; } }\\n' > {W}/e3.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/e3.c -o {W}/e3.o 2>&1 | "
		 "grep -oE 'variably modified declaration'; "
		 "printf 'int n=3; int g(int c){ switch(c){ int a[n]; case 1: return sizeof a; case 2: return 0; } return 0; }\\n' > {W}/e5.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/e5.c -o {W}/e5.o 2>&1 | "
		 "grep -oE 'variably modified declaration'; "
		 "printf 'int n=3; int f(void){ goto L; int (*p)[n]; int (*q)[n]; L: p=q=0; return p==q; }\\n' > {W}/e6.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/e6.c -o {W}/e6.o 2>&1 | "
		 "grep -oE 'variably modified declaration'; "
		 "printf 'int n=3;\\nint ok(void){ int a[n]; L: if(a[0]) goto L; return sizeof a; }\\n"
		 "int main(void){return 0;}\\n' > {W}/v1.c && "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/v1.c -o {W}/v1 && echo VALID_OK; "
		 "printf 'int n=3;\\nint ok(int c){ if(c) goto L; c=1; L: { int a[n]; return sizeof a + c; } }\\n"
		 "int main(void){return 0;}\\n' > {W}/v2.c && "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/v2.c -o {W}/v2 && echo VALID_OK; "
		 "printf 'int n=3;\\nint ok(int c){ switch(c){ case 1: return 1; default: return 0; } }\\n"
		 "int main(void){return 0;}\\n' > {W}/v3.c && "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/v3.c -o {W}/v3 && echo VALID_OK",
		 "variably modified declaration\nvariably modified declaration\nvariably modified declaration\n"
		 "variably modified declaration\nvariably modified declaration\nvariably modified declaration\n"
		 "VALID_OK\nVALID_OK\nVALID_OK\n"},

		{"atomic_large_generic", "",
		 "printf '#include <stdatomic.h>\\ntypedef struct{long a,b,c;}Big;\\n"
		 "_Atomic Big g;\\nvoid f(Big v,Big*e,Big d){ atomic_store(&g,v);"
		 " Big r=atomic_load(&g);(void)r; Big o=atomic_exchange(&g,v);(void)o;"
		 " (void)atomic_compare_exchange_strong(&g,e,d); }\\n' > {W}/lg.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/lg.c -o {W}/lg.o && "
		 "nm {W}/lg.o | grep -oE '__atomic_(load|store|exchange|compare_exchange)$' | sort -u; "
		 "printf '#include <stdatomic.h>\\nstruct P{int x,y;}; _Atomic struct P s;\\n"
		 "void h(struct P v){ atomic_store(&s,v); }\\n' > {W}/sm.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/sm.c -o {W}/sm.o && "
		 "nm {W}/sm.o | grep -cE '__atomic_store$'",
		 "__atomic_compare_exchange\n__atomic_exchange\n__atomic_load\n__atomic_store\n0\n"},

		{"atomic_rmw_unsupported", "elf",
		 "printf '_Atomic long double ld; void f(void){ ld*=2; }\\n' > {W}/ar1.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/ar1.c -o {W}/ar1.o 2>&1 | "
		 "grep -oE 'compound assignment to an ._Atomic. object is not supported'; "
		 "printf '#include <stdatomic.h>\\nint main(void){ atomic_int g=7; g*=3; g%%=5;"
		 " g<<=4; _Atomic double d=2; d*=2.5; d+=1; return ((int)g==16 && d==6)?0:1; }\\n' > {W}/ar3.c && "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/ar3.c -o {W}/ar3 && {W}/ar3 && echo RMW_OK",
		 "compound assignment to an '_Atomic' object is not supported\nRMW_OK\n"},

		{"member_declares_nothing", "",
		 "printf 'struct S{int;}; struct T{enum E{A=5};};\\n"
		 "int main(void){ return (sizeof(struct S)>=0 && A==5)?0:1; }\\n' > {W}/dn.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dn.c -o {W}/dn.o 2>&1 | "
		 "grep -c 'declaration does not declare anything'; "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/dn.c -o {W}/dn >/dev/null 2>&1 && {W}/dn && echo DN_OK; "
		 "printf 'struct U{int @;};\\n' > {W}/dn2.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dn2.c -o {W}/dn2.o 2>&1 | "
		 "grep -oE 'identifier expected'; "

		 "printf 'struct W{struct T{int a;};};\\n' > {W}/dn3.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -fms-extensions -c {W}/dn3.c -o {W}/dn3.o 2>&1 | wc -l | tr -d ' '; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dn3.c -o {W}/dn3.o 2>&1 | "
		 "grep -c 'declaration does not declare anything'; "
		 "printf 'struct A{char a;}; struct B{struct A; char b;};\\n"
		 "char t[sizeof(struct B)==1?1:-1];\\n' > {W}/dn4.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -w -c {W}/dn4.c -o {W}/dn4.o && echo NOFIELD_OK",
		 "2\nDN_OK\nidentifier expected\n0\n1\nNOFIELD_OK\n"},

		{"cast_to_nonscalar", "",
		 "printf 'struct A{int x;}; struct B{int y;}; int f(struct A a){ return ((struct B)a).y; }\\n' > {W}/c1.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/c1.c -o {W}/c1.o 2>&1 | "
		 "grep -oE 'conversion to non-scalar type requested'; "
		 "printf 'typedef int AT[3]; int f(int*p){ (void)(AT)p; return 0; }\\n' > {W}/c2.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/c2.c -o {W}/c2.o 2>&1 | "
		 "grep -oE 'conversion to non-scalar type requested'; "
		 "printf '#include <stdarg.h>\\nstruct A{int x;};\\n"
		 "int sum(int n,...){va_list a; va_start(a,n); int t=0;"
		 " for(int i=0;i<n;i++) t+=va_arg(a,int); va_end(a); return t;}\\n"
		 "int ns(struct A a){ return ((struct A)a).x; }\\n"
		 "int main(void){ struct A a={7}; return (sum(3,10,20,30)==60 && ns(a)==7)?0:1; }\\n' > {W}/c3.c && "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/c3.c -o {W}/c3 && {W}/c3 && echo CAST_OK",
		 "conversion to non-scalar type requested\nconversion to non-scalar type requested\nCAST_OK\n"},

		{"x86_64_sysv_vararg_2sse_boundary", "cpu=x86_64,os=linux",
		 "printf '#include <stdarg.h>\\nstruct dd{double a,b;};\\n"
		 "static void f(int n,...){va_list ap; va_start(ap,n);"
		 " for(int i=0;i<n;i++)(void)va_arg(ap,double);"
		 " struct dd s=va_arg(ap,struct dd); double last=va_arg(ap,double); va_end(ap);"
		 " if(s.a!=88.0||s.b!=176.0||last!=99.5)__builtin_abort();}\\n"
		 "int main(void){struct dd s={88.0,176.0};"
		 " f(7,1.0,2.0,3.0,4.0,5.0,6.0,7.0,s,99.5); return 0;}\\n' > {W}/va2sse.c && "
		 "{MCC} -B{B} -I{I} -run {W}/va2sse.c && echo OK",
		 "OK\n"},

		{"arm64_unnamed_varargs_c2y", "cpu=arm64",
		 "printf '#include <stdarg.h>\\nstruct dd{double a,b;};\\n"
		 "void f(...){va_list ap; va_start(ap);"
		 " int i=va_arg(ap,int); long long l=va_arg(ap,long long); double d=va_arg(ap,double);"
		 " struct dd s=va_arg(ap,struct dd); int j=va_arg(ap,int); va_end(ap);"
		 " if(i!=42||l!=1000000000007LL||d!=2.5||s.a!=7.5||s.b!=8.5||j!=-9)__builtin_abort();}\\n"
		 "int main(void){struct dd s={7.5,8.5};"
		 " f(42,1000000000007LL,2.5,s,-9); return 0;}\\n' > {W}/uvarg.c && "
		 "{MCC} -B{B} -I{I} -run {W}/uvarg.c && echo OK",
		 "OK\n"},

		{"atomic_aggregate_load_generic", "",
		 "printf '#include <stdatomic.h>\\nstruct P{int x,y;};\\n_Atomic struct P p;\\n"
		 "void f(struct P v){ struct P r=atomic_load(&p);(void)r;"
		 " struct P o=atomic_exchange(&p,v);(void)o; atomic_store(&p,v); }\\n' > {W}/aa.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/aa.c -o {W}/aa.o && "
		 "nm {W}/aa.o | grep -oE '__atomic_(load|exchange|store_8)$' | sort -u; "
		 "printf '#include <stdatomic.h>\\nint main(void){ atomic_int x=1; atomic_store(&x,5);"
		 " return (int)atomic_load(&x)==5?0:1; }\\n' > {W}/as.c && "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/as.c -o {W}/as && {W}/as && echo SCALAR_OK",
		 "__atomic_exchange\n__atomic_load\n__atomic_store_8\nSCALAR_OK\n"},

		{"atomic_param_type_distinct", "",
		 "printf 'void f(_Atomic int); void f(int);\\nint main(void){return 0;}\\n' > {W}/apr.c ; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/apr.c -o {W}/apr.o 2>&1 | grep -oE 'incompatible types for redefinition' ; "
		 "printf '_Atomic int a=3;\\nint main(void){int b=a; return b==3?0:1;}\\n' > {W}/apok.c && "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/apok.c -o {W}/apok && {W}/apok && echo ATOMIC_ASSIGN_OK",
		 "incompatible types for redefinition\nATOMIC_ASSIGN_OK\n"},

		{"override_init_warn", "",
		 "printf 'int a[3]={[0]=1,[0]=2};\\nstruct S{int x,y;};\\nstruct S s={.x=1,.x=2};\\n"
		 "int b[3]={[0]=1,[1]=2};\\nint main(void){return 0;}\\n' > {W}/oi.c && "
		 "printf 'W='; {MCC} -B{B} -I{I} -Woverride-init -c {W}/oi.c -o {W}/oi.o 2>&1 | grep -c 'initialized field overwritten'; "
		 "printf 'A='; {MCC} -B{B} -I{I} -Wall -c {W}/oi.c -o {W}/oi2.o 2>&1 | grep -c 'overwritten'; "
		 "printf 'D='; {MCC} -B{B} -I{I} -c {W}/oi.c -o {W}/oi3.o 2>&1 | grep -c 'overwritten'",
		 "W=2\nA=0\nD=0\n"},

		{"missing_field_initializers_warn", "",
		 "printf 'struct S{int x,y;};struct O{struct S i;int z;};struct F{int n;int fa[];};\\n"
		 "struct S a={1};\\nstruct S b={0};\\nstruct S c={1,2};\\n"
		 "struct S d={.x=1};\\nstruct O e={{1,2}};\\nstruct F g={5};\\nint main(void){return 0;}\\n' > {W}/mfi.c && "
		 "printf 'X='; {MCC} -B{B} -I{I} -Wmissing-field-initializers -c {W}/mfi.c -o {W}/mfi.o 2>&1 | grep -c 'missing initializer for field'; "
		 "printf 'E='; {MCC} -B{B} -I{I} -Wextra -c {W}/mfi.c -o {W}/mfi2.o 2>&1 | grep -c 'missing initializer'; "
		 "printf 'D='; {MCC} -B{B} -I{I} -c {W}/mfi.c -o {W}/mfi3.o 2>&1 | grep -c 'missing initializer'",
		 "X=2\nE=2\nD=0\n"},

		{"empty_body_warn", "",
		 "printf 'void s(void);\\nint a(int x){if(x); return x;}\\n"
		 "int b(int x){if(x)s();else ; return x;}\\nint c(int x){do ; while(x); return x;}\\n"
		 "int d(int x){while(x); return x;}\\nint main(void){return 0;}\\n' > {W}/eb.c && "
		 "printf 'E='; {MCC} -B{B} -I{I} -Wextra -c {W}/eb.c -o {W}/eb.o 2>&1 | grep -c 'empty body'; "
		 "printf 'A='; {MCC} -B{B} -I{I} -Wall -c {W}/eb.c -o {W}/eb2.o 2>&1 | grep -c 'empty body'; "
		 "printf 'D='; {MCC} -B{B} -I{I} -c {W}/eb.c -o {W}/eb3.o 2>&1 | grep -c 'empty body'",
		 "E=3\nA=0\nD=0\n"},

		{"generic_float_const_dispatch", "",
		 "printf 'extern int printf(const char*,...);\\n"
		 "#define W(x) _Generic((x), float: 1, double: 2, long double: 3, default: 0)\\n"
		 "int main(void){ printf(\"%%d%%d%%d\\\\n\", W(4.0f), W(4.0), W(4.0L)); return 0; }\\n' > {W}/gfc.c && "
		 "{MCC} -B{B} -I{I} {W}/gfc.c -o {W}/gfc && {W}/gfc",
		 "123\n"},

		{"bool_operation_warn", "",
		 "printf 'int t1(_Bool b){return ~b;}\\nint t2(_Bool b){b++;return b;}\\n"
		 "int t3(_Bool b){b--;return b;}\\nint t4(int x){return ~x;}\\nint main(void){return 0;}\\n' > {W}/bo.c && "
		 "printf 'A='; {MCC} -B{B} -I{I} -Wall -c {W}/bo.c -o {W}/bo.o 2>&1 | grep -c 'boolean expression'; "
		 "printf 'D='; {MCC} -B{B} -I{I} -c {W}/bo.c -o {W}/bo2.o 2>&1 | grep -c 'boolean'",
		 "A=3\nD=0\n"},

		{"sizeof_array_argument_warn", "",
		 "printf 'int p1(int a[10]){return (int)sizeof(a);}\\n"
		 "int p2(char a[5]){return (int)sizeof a;}\\n"
		 "int n1(int *a){return (int)sizeof(a);}\\n"
		 "int n2(int a[10]){return (int)sizeof(*a);}\\n"
		 "int n3(int a[10]){return (int)sizeof(a+1);}\\n"
		 "int garr[4]; int n4(void){return (int)sizeof(garr);}\\n"
		 "int main(void){return 0;}\\n' > {W}/sza.c && "
		 "printf 'D='; {MCC} -B{B} -I{I} -c {W}/sza.c -o {W}/sza.o 2>&1 | grep -c 'array function parameter'; "
		 "printf 'N='; {MCC} -B{B} -I{I} -Wno-sizeof-array-argument -c {W}/sza.c -o {W}/sza2.o 2>&1 | grep -c 'array function parameter'",
		 "D=2\nN=0\n"},

		{"pedantic_diagnostics", "",
		 "printf 'int f(void){ int n=3; struct S{int a;char b[n];int c;} s;"
		 " s.a=1; s.c=2;"
		 " return (int)sizeof(s)==12 && (int)((char*)&s.c-(char*)&s)==8"
		 " && s.a==1 && s.c==2; }\\n"
		 "int main(void){ return f()?0:1; }\\n' > {W}/pd1.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/pd1.c -o {W}/pd1.o 2>&1 | wc -l | tr -d ' '; "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/pd1.c -o {W}/pd1 && {W}/pd1 && echo VLA_STRUCT_OK; "
		 "printf 'enum E{X=0x100000000}; int main(void){return X*0;}\\n' > {W}/pd2.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/pd2.c -o {W}/pd2.o 2>&1 | "
		 "grep -oE 'range of .int.'; "
		 "printf 'int a[(1,2)]; int main(void){return sizeof a*0;}\\n' > {W}/pd3.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/pd3.c -o {W}/pd3.o 2>&1 | "
		 "grep -oE 'comma operator in a constant expression'; "
		 "printf 'int main(void){ for(static int i=0;i<1;i++); return 0; }\\n' > {W}/pd4.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/pd4.c -o {W}/pd4.o 2>&1 | "
		 "grep -oE \"in a .for. loop initializer\"; "
		 "printf '_Noreturn int x;\\n' > {W}/pd5.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/pd5.c -o {W}/pd5.o 2>&1 | "
		 "grep -oE \"._Noreturn. used outside of a function\"; "
		 "printf 'struct F{int n;int d[];}; struct G{struct F f;int x;};\\n' > {W}/pd6.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/pd6.c -o {W}/pd6.o 2>&1 | "
		 "grep -oE 'flexible array member'; "
		 "printf 'enum E *p; int main(void){return p!=0;}\\n' > {W}/pd7.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/pd7.c -o {W}/pd7.o 2>&1 | "
		 "grep -oE \"forward references to .enum. types\"; "
		 "printf 'void fn(void); int m(void){return (int)sizeof(fn);}\\n' > {W}/pd8.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/pd8.c -o {W}/pd8.o 2>&1 | "
		 "grep -oE \".sizeof. applied to a function type\"; "
		 "printf '_Static_assert(1,\"ok\"); int main(void){return 0;}\\n' > {W}/pd9.c && "
		 "{MCC} -B{B} -I{I} -std=c99 -pedantic -c {W}/pd9.c -o {W}/pd9.o 2>&1 | "
		 "grep -oE \"does not support ._Static_assert. before C11\"; "
		 "printf 'struct S{int d[];}; int main(void){return (int)sizeof(struct S);}\\n' > {W}/pd10.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/pd10.c -o {W}/pd10.o 2>&1 | "
		 "grep -oE 'flexible array member in a struct with no named members'; echo END",
		 "0\nVLA_STRUCT_OK\nrange of 'int'\ncomma operator in a constant expression\nin a 'for' loop initializer\n'_Noreturn' used outside of a function\nflexible array member\nforward references to 'enum' types\n'sizeof' applied to a function type\ndoes not support '_Static_assert' before C11\nflexible array member in a struct with no named members\nEND\n"},

		{"noreturn_nonfunc_default_error", "",
		 "printf '_Noreturn int x;\\n' > {W}/nrnf.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/nrnf.c -o {W}/nrnf.o 2>&1 | "
		 "grep -oE \"error: ._Noreturn. used outside of a function\"; echo DONE",
		 "error: '_Noreturn' used outside of a function\nDONE\n"},

		{"pragma_message_concat", "",
		 "printf '#pragma message(\"a \" \"b\" \" c\")\\nint main(void){return 0;}\\n' > {W}/pmc.c && "
		 "{MCC} -B{B} -I{I} -c {W}/pmc.c -o {W}/pmc.o 2>&1 | "
		 "grep -oE 'message: a b c'; echo DONE",
		 "message: a b c\nDONE\n"},

		{"stdbit_header", "",
		 "{MCC} -B{B} -I{I} -std=c23 {D}/stdbit_check.c -o {W}/stdbit && "
		 "{W}/stdbit && echo DONE",
		 "STDBIT_OK\nDONE\n"},

		{"asm_unknown_directive", "",
		 "printf '.text\\n.foobardir 1, 2\\n' > {W}/ud.s && "
		 "{MCC} -B{B} -I{I} -c {W}/ud.s -o {W}/ud.o 2>&1 | grep -o 'unknown directive' ; echo END",
		 "unknown directive\nEND\n"},

		{"unlocked_stdio_builtins", "",
		 "{MCC} -B{B} -I{I} {D}/unlocked_check.c -o {W}/unlocked && {W}/unlocked",
		 "A1\nB2\nC3\nUNLOCKED_OK\n"},

		{"nodiscard_c23_warns", "",
		 "printf '[[nodiscard]] int f(void);\\nvoid g(void){ f(); }\\n' > {W}/nd.c && "
		 "printf '__attribute__((warn_unused_result)) int w(void);\\nvoid v(void){ w(); }\\n' > {W}/wr.c && "
		 "{MCC} -B{B} -I{I} -std=c23 -Wall -c {W}/nd.c -o {W}/nd.o 2>&1 | grep -o \"attribute 'nodiscard'\" ; "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/wr.c -o {W}/wr.o 2>&1 | grep -o \"attribute 'warn_unused_result'\" ; echo END",
		 "attribute 'nodiscard'\nattribute 'warn_unused_result'\nEND\n"},

		{"flt128_predef", "",
		 "{MCC} -B{B} -I{I} {D}/flt128_predef.c -o {W}/flt128 && {W}/flt128",
		 "FLT128_OK\n"},

		{"float_char_predef", "",
		 "{MCC} -B{B} -I{I} {D}/float_char_predef.c -o {W}/fchar && {W}/fchar",
		 "FLOAT_CHAR_OK\n"},

		{"width_predef", "",
		 "{MCC} -B{B} -I{I} {D}/width_predef.c -o {W}/width && {W}/width",
		 "WIDTH_OK\n"},

		{"address_compare_warn", "",
		 "printf 'int a[4];\\nint x;\\nvoid gg(void);\\nint f(void){ return a == 0; }\\nint h(void){ return gg != 0; }\\nint k(int*p){ return p == 0; }\\nint m(void){ if (a) return 1; return 0; }\\nint n(void){ return !a; }\\nint q(void){ return &x == 0; }\\nint s(void){ return \"z\" == 0; }\\n' > {W}/aw.c && "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/aw.c -o {W}/aw.o 2>&1 | grep -c 'warning: the comparison' ; "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/aw.c -o {W}/aw.o 2>&1 | grep -c 'warning: the address of' ; echo END",
		 "3\n2\nEND\n"},

		{"enum_compare_warn", "",
		 "printf 'enum A{X=1};\\nenum B{Y=1};\\nint f(enum A a, enum B b){ return a==b; }\\nint g(enum A a){ return a==Y; }\\nint h(enum A a, enum A a2){ return a==a2; }\\nint k(enum A a, int i){ return a==i; }\\n' > {W}/ec.c && "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/ec.c -o {W}/ec.o 2>&1 | grep -c \"comparison between 'enum A' and 'enum B'\" ; echo END",
		 "2\nEND\n"},

		{"char_subscripts_warn", "",
		 "printf 'int f(int*p, char c){ return p[c]; }\\nint g(int*p, unsigned char u){ return p[u]; }\\nint h(int*p, signed char s){ return p[s]; }\\nint k(int*p, int i){ return p[i]; }\\n' > {W}/cs.c && "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/cs.c -o {W}/cs.o 2>&1 | grep -c \"subscript has type 'char'\" ; echo END",
		 "1\nEND\n"},

		{"bool_compare_warn", "",
		 "printf '_Bool b;\\nint f(void){ return b == 2; }\\nint g(void){ return b < 0; }\\nint h(void){ return b == 0; }\\nint j(int x){ return x == 2; }\\n' > {W}/bc.c && "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/bc.c -o {W}/bc.o 2>&1 | grep -c 'with boolean expression is always' ; echo END",
		 "2\nEND\n"},

		{"driver_ignore_opt_flags", "",
		 "printf 'int main(void){return 0;}\\n' > {W}/d.c && "
		 "{MCC} -B{B} -I{I} -funroll-loops -fno-plt -fno-semantic-interposition -fvisibility-inlines-hidden -fno-delete-null-pointer-checks -fno-ident -c {W}/d.c -o {W}/d.o 2>&1 | grep -c unsupported ; "
		 "{MCC} -B{B} -I{I} -fstack-clash-protection -c {W}/d.c -o {W}/d2.o 2>&1 | grep -c unsupported ; echo END",
		 "0\n1\nEND\n"},

		{"no_inline_predef", "",
		 "printf 'int x;\\n' > {W}/ni.c && "
		 "{MCC} -B{B} -I{I} -dM -E {W}/ni.c 2>&1 | grep -c '__NO_INLINE__' ; "
		 "{MCC} -B{B} -I{I} -O2 -dM -E {W}/ni.c 2>&1 | grep -c '__NO_INLINE__' ; echo END",
		 "1\n0\nEND\n"},

		{"fast_math_predef_and_fp_opts", "",
		 "printf 'int main(void){return 0;}\\n' > {W}/fm.c && "
		 "{MCC} -B{B} -I{I} -ffast-math -dM -E {W}/fm.c 2>&1 | grep -o '__FAST_MATH__ 1' ; "
		 "{MCC} -B{B} -I{I} -dM -E {W}/fm.c 2>&1 | grep -c '__FAST_MATH__' ; "
		 "{MCC} -B{B} -I{I} -ffp-contract=fast -funsafe-math-optimizations -c {W}/fm.c -o {W}/fm.o 2>&1 | grep -c unsupported ; echo END",
		 "__FAST_MATH__ 1\n0\n0\nEND\n"},

		{"xlinker_accepted", "",
		 "printf 'int main(void){return 0;}\\n' > {W}/xl.c && "
		 "{MCC} -B{B} -I{I} -c {W}/xl.c -o {W}/xl.o -Xlinker -Bsymbolic && echo XLINKER_OK",
		 "XLINKER_OK\n"},

		{"x_assembler_no_cpp", "",
		 "printf '#if 0\\nzzz\\n#endif\\n.text\\n.globl af\\naf:\\n\\tret\\n' > {W}/xa.txt && "
		 "{MCC} -B{B} -I{I} -c -x assembler-with-cpp {W}/xa.txt -o {W}/xa1.o && echo WITHCPP_OK; "
		 "{MCC} -B{B} -I{I} -c -x assembler {W}/xa.txt -o {W}/xa2.o 2>/dev/null && echo NOCPP_BAD || echo NOCPP_FAILED",
		 "WITHCPP_OK\nNOCPP_FAILED\n"},

		{"pragma_operator_real_location", "",
		 "{MCC} -B{B} -I{I} -c {D}/prag_macro.c -o {W}/prag.o 2>&1 | "
		 "grep -oE 'prag_macro[.]c:[0-9]+: note: #pragma message: hi'",
		 "prag_macro.c:2: note: #pragma message: hi\n"},

		{"sizeof_incomplete_specific_msg", "",
		 "printf 'struct S; int n = sizeof(struct S);\\n' > {W}/inc.c && "
		 "{MCC} -B{B} -I{I} -c {W}/inc.c -o {W}/inc.o 2>&1 | "
		 "grep -oE \".sizeof. applied to an incomplete type .struct S.\"; echo DONE",
		 "'sizeof' applied to an incomplete type 'struct S'\nDONE\n"},

		{"c9911_diag_gaps", "",
		 "printf 'static int x; int x;\\nint main(void){return x;}\\n' > {W}/cg1.c && "
		 "{MCC} -B{B} -I{I} -c {W}/cg1.c -o {W}/cg1.o 2>&1 | "
		 "grep -oE 'non-static declaration of .x. follows static'; "
		 "printf 'static int x; extern int x;\\nint main(void){return x;}\\n' > {W}/cg1b.c && "
		 "{MCC} -B{B} -I{I} -c {W}/cg1b.c -o {W}/cg1b.o 2>&1 && echo EXTERN_OK; "
		 "printf 'static int arr[];\\nint main(void){return arr[0];}\\n' > {W}/cg2.c && "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/cg2.c -o {W}/cg2.o 2>&1 | "
		 "grep -oE 'assumed to have one element'; "
		 "printf 'int y={{5}};\\n' > {W}/cg3.c && "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/cg3.c -o {W}/cg3.o 2>&1 | "
		 "grep -oE 'too many braces around scalar'; "
		 "printf 'static void f(void);\\nint main(void){f();return 0;}\\n' > {W}/cg4.c && "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/cg4.c -o {W}/cg4.o 2>&1 | "
		 "grep -oE '.f. used but never defined'; "
		 "printf 'int a[3]={[1]=2};\\n' > {W}/cg5.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/cg5.c -o {W}/cg5.o 2>&1 | "
		 "grep -oE 'designated initializers are a C99 feature'; echo END",
		 "non-static declaration of 'x' follows static\nEXTERN_OK\nassumed to have one element\ntoo many braces around scalar\n'f' used but never defined\ndesignated initializers are a C99 feature\nEND\n"},

		{"c9911_diag_gaps2", "",
		 "printf 'int f(void){ int x=0; goto L; L: }\\n' > {W}/dg1.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/dg1.c -o {W}/dg1.o 2>&1 | "
		 "grep -oE 'label at end of compound statement is a C23 feature'; "
		 "printf 'int p(); int q(void); int main(void){return 0;}\\n' > {W}/dg2.c && "
		 "{MCC} -B{B} -I{I} -Wstrict-prototypes -c {W}/dg2.c -o {W}/dg2.o 2>&1 | "
		 "grep -oE \"isn.t a prototype\"; "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/dg2.c -o {W}/dg2.o 2>&1 && echo PROTO_QUIET; "
		 "printf '#include <stdio.h>\\nint main(void){double d; scanf(\"%%d\",&d); return 0;}\\n' > {W}/dg3.c && "
		 "{MCC} -B{B} -I{I} -Wall -c {W}/dg3.c -o {W}/dg3.o 2>&1 | "
		 "grep -oE 'expects a pointer to int argument'; "
		 "printf '#define BAD(x) # 5\\nint main(void){return 0;}\\n' > {W}/dg4.c && "
		 "{MCC} -B{B} -I{I} -c {W}/dg4.c -o {W}/dg4.o 2>&1 | "
		 "grep -oE 'is not followed by a macro parameter'; echo END",
		 "label at end of compound statement is a C23 feature\nisn't a prototype\nPROTO_QUIET\nexpects a pointer to int argument\nis not followed by a macro parameter\nEND\n"},

		{"c9911_diag_gaps3", "",
		 "printf 'int f(int x){switch(x){case 1:return 1;case 1:return 2;}return 0;}\\n' > {W}/dh1.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dh1.c -o {W}/dh1.o 2>&1 | "
		 "grep -oE 'duplicate case value'; "
		 "printf 'int f(int x){case 1:return x;}\\n' > {W}/dh2.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dh2.c -o {W}/dh2.o 2>&1 | "
		 "grep -oE 'switch expected'; "
		 "printf 'int f(int x){switch(x){default:return 1;default:return 2;}return 0;}\\n' > {W}/dh3.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dh3.c -o {W}/dh3.o 2>&1 | "
		 "grep -oE 'too many .default.'; "
		 "printf 'int f(void){break;return 0;}\\n' > {W}/dh4.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dh4.c -o {W}/dh4.o 2>&1 | "
		 "grep -oE 'cannot break'; "
		 "printf 'int main(void){L: L: return 0;}\\n' > {W}/dh5.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dh5.c -o {W}/dh5.o 2>&1 | "
		 "grep -oE 'duplicate label .L.'; "
		 "printf 'int main(void){goto nowhere; return 0;}\\n' > {W}/dh6.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dh6.c -o {W}/dh6.o 2>&1 | "
		 "grep -oE 'label .nowhere. used but not defined'; "
		 "printf 'typedef int T; typedef long T;\\nint main(void){return 0;}\\n' > {W}/dh7.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dh7.c -o {W}/dh7.o 2>&1 | "
		 "grep -oE 'incompatible redefinition of .T.'; "
		 "printf 'void v;\\nint main(void){return 0;}\\n' > {W}/dh8.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dh8.c -o {W}/dh8.o 2>&1 | "
		 "grep -oE 'declaration of void object'; "
		 "printf 'int a[3](void);\\nint main(void){return 0;}\\n' > {W}/dh9.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dh9.c -o {W}/dh9.o 2>&1 | "
		 "grep -oE 'declaration of an array of functions'; "
		 "printf 'struct S{int m; int m;};\\nint main(void){return 0;}\\n' > {W}/dh10.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dh10.c -o {W}/dh10.o 2>&1 | "
		 "grep -oE 'duplicate member .m.'; "
		 "printf 'int main(void){const int x=1; x=2; return x;}\\n' > {W}/dh11.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dh11.c -o {W}/dh11.o 2>&1 | "
		 "grep -oE 'assignment of read-only location'; "
		 "printf 'struct S{int b:3;};\\nint main(void){struct S s; int*p=&s.b; return 0;}\\n' > {W}/dh12.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dh12.c -o {W}/dh12.o 2>&1 | "
		 "grep -oE 'cannot take address of bit-field'; "
		 "printf 'struct S{int b:40;};\\nint main(void){return 0;}\\n' > {W}/dh13.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dh13.c -o {W}/dh13.o 2>&1 | "
		 "grep -oE 'width of .b. exceeds its type'; "
		 "printf '_Static_assert(sizeof(int)==999,\\042int is not 999 bytes\\042);\\nint main(void){return 0;}\\n' > {W}/dh14.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -fno-diagnostics-show-caret -c {W}/dh14.c -o {W}/dh14.o 2>&1 | "
		 "grep -oE 'int is not 999 bytes'; "
		 "printf 'long long x;\\nint main(void){return (int)x;}\\n' > {W}/dh15.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/dh15.c -o {W}/dh15.o 2>&1 | "
		 "grep -oE 'ISO C90 does not support .long long.'; "
		 "printf 'char c;\\nint *p = &c;\\nint main(void){return 0;}\\n' > {W}/dh16.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dh16.c -o {W}/dh16.o 2>&1 | "
		 "grep -oE 'initialization from incompatible pointer type'; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/dh16.c -o {W}/dh16.o 2>&1 | "
		 "grep -oE 'error: initialization from incompatible pointer type'; echo END",
		 "duplicate case value\nswitch expected\ntoo many 'default'\ncannot break\nduplicate label 'L'\nlabel 'nowhere' used but not defined\nincompatible redefinition of 'T'\ndeclaration of void object\ndeclaration of an array of functions\nduplicate member 'm'\nassignment of read-only location\ncannot take address of bit-field\nwidth of 'b' exceeds its type\nint is not 999 bytes\nISO C90 does not support 'long long'\ninitialization from incompatible pointer type\nerror: initialization from incompatible pointer type\nEND\n"},

		{"c9911_diag_gaps4", "",
		 "printf 'struct S{int n; int a[];};\\nint main(void){return 0;}\\n' > {W}/di1.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/di1.c -o {W}/di1.o 2>&1 | "
		 "grep -oE 'flexible array members are a C99 feature'; "
		 "printf 'enum E{A,B,};\\nint main(void){return B;}\\n' > {W}/di2.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/di2.c -o {W}/di2.o 2>&1 | "
		 "grep -oE 'trailing comma in enumerator list is a C99 feature'; "
		 "printf 'int main(void){for(int i=0;i<3;i++); return 0;}\\n' > {W}/di3.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/di3.c -o {W}/di3.o 2>&1 | "
		 "grep -oE \"'for' loop initial declarations are a C99 feature\"; "
		 "{MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/di3.c -o {W}/di3.o 2>&1 && echo C99_OK; "
		 "{MCC} -B{B} -I{I} -std=c89 -c {W}/di2.c -o {W}/di2.o 2>&1 && echo NONPED_OK; echo END",
		 "flexible array members are a C99 feature\ntrailing comma in enumerator list is a C99 feature\n'for' loop initial declarations are a C99 feature\nC99_OK\nNONPED_OK\nEND\n"},

		{"c9911_diag_gaps5", "",
		 "printf 'int f(int n){int a[n]; return sizeof a;}\\n' > {W}/dj1.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/dj1.c -o {W}/dj1.o 2>&1 | "
		 "grep -oE 'variable length arrays are a C99 feature'; "
		 "printf 'int *f(void){return (int[]){1,2};}\\n' > {W}/dj2.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/dj2.c -o {W}/dj2.o 2>&1 | "
		 "grep -oE 'compound literals are a C99 feature'; "
		 "printf 'double d=0x1.5p3;\\n' > {W}/dj3.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/dj3.c -o {W}/dj3.o 2>&1 | "
		 "grep -oE 'hexadecimal floating constants are a C99 feature'; "
		 "printf 'long x(void){return 1LL;}\\n' > {W}/dj4.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/dj4.c -o {W}/dj4.o 2>&1 | "
		 "grep -oE \"ISO C90 does not support .long long.\"; "
		 "printf 'int c(int*a,char*b){return a==b;}\\n' > {W}/dj5.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/dj5.c -o {W}/dj5.o 2>&1 | "
		 "grep -oE 'pointer type mismatch in comparison'; "
		 "printf 'void *t(int c,int*a,char*b){return c?a:b;}\\n' > {W}/dj6.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/dj6.c -o {W}/dj6.o 2>&1 | "
		 "grep -oE 'pointer type mismatch in conditional expression'; "
		 "{MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/dj1.c -o {W}/dj1.o 2>&1 && echo C99_OK; echo END",
		 "variable length arrays are a C99 feature\ncompound literals are a C99 feature\nhexadecimal floating constants are a C99 feature\nISO C90 does not support 'long long'\npointer type mismatch in comparison\npointer type mismatch in conditional expression\nC99_OK\nEND\n"},

		{"c9911_diag_gaps6", "",
		 "printf 'int main(void){int a=0; a++; int b=1; return a+b;}\\n' > {W}/dk1.c && "
		 "{MCC} -B{B} -I{I} -std=c89 -pedantic-errors -c {W}/dk1.c -o {W}/dk1.o 2>&1 | "
		 "grep -oE 'mixed declarations and code are a C99 feature'; "
		 "printf 'int a[3]={};\\n' > {W}/dk2.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/dk2.c -o {W}/dk2.o 2>&1 | "
		 "grep -oE 'empty initializer braces are a C23 feature'; "
		 "{MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/dk1.c -o {W}/dk1.o 2>&1 && echo C99_OK; "
		 "printf 'int a[3]={1,2,3};\\n' > {W}/dk3.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/dk3.c -o {W}/dk3.o 2>&1 && echo NONEMPTY_OK; echo END",
		 "mixed declarations and code are a C99 feature\nempty initializer braces are a C23 feature\nC99_OK\nNONEMPTY_OK\nEND\n"},

		{"raw_utf8_identifier", "",
		 "printf 'int caf\\303\\251 = 1;\\nint main(void){return caf\\303\\251-1;}\\n' > {W}/ui1.c && "
		 "{MCC} -B{B} -I{I} {W}/ui1.c -o {W}/ui1 && {W}/ui1; echo \"valid=$?\"; "
		 "printf 'int a\\357\\277\\277 = 1;\\nint main(void){return 0;}\\n' > {W}/ui2.c && "
		 "{MCC} -B{B} -I{I} -c {W}/ui2.c -o {W}/ui2.o 2>&1 | grep -oE 'not valid in an identifier'; "
		 "printf 'int \\314\\200x = 1;\\nint main(void){return 0;}\\n' > {W}/ui3.c && "
		 "{MCC} -B{B} -I{I} -c {W}/ui3.c -o {W}/ui3.o 2>&1 | grep -oE 'not valid as the first character'; echo END",
		 "valid=0\nnot valid in an identifier\nnot valid as the first character\nEND\n"},

		{"builtin_nan_inf_const", "os!=WIN32",
		 "printf '#include <stdio.h>\\n#include <fenv.h>\\n"
		 "static double sn=__builtin_nan(\"\"); static double si=__builtin_inf();\\n"
		 "int main(void){ feclearexcept(FE_ALL_EXCEPT);\\n"
		 "volatile double n=__builtin_nan(\"\"), i=__builtin_inf();\\n"
		 "printf(\"%%f %%f %%f %%f sb=%%d inv=%%d\\\\n\", n, i, sn, si,\\n"
		 "__builtin_signbit(n), fetestexcept(FE_INVALID)!=0); return 0; }\\n' > {W}/bni.c && "
		 "{MCC} -B{B} -I{I} {W}/bni.c -o {W}/bni -lm && {W}/bni",
		 "nan inf nan inf sb=0 inv=0\n"},

		{"builtin_signbit_no_trap", "os!=WIN32",

		 "printf '#include <stdio.h>\\n#include <fenv.h>\\n"
		 "static int c1=__builtin_signbit(-0.0), c2=__builtin_signbitf(-2.5f),\\n"
		 "c3=__builtin_signbitl(-3.5L), c4=__builtin_signbit(-__builtin_nan(\"\"));\\n"
		 "int main(void){ volatile double dn=__builtin_nan(\"\"), dz=-0.0;\\n"
		 "volatile float f=-4.5f; volatile long double l=-6.5L, lz=0.0L;\\n"
		 "feclearexcept(FE_ALL_EXCEPT);\\n"
		 "printf(\"c=%%d%%d%%d%%d r=%%d%%d%%d%%d%%d%%d exc=%%d\\\\n\", c1,c2,c3,c4,\\n"
		 "__builtin_signbit(dn), __builtin_signbit(-dn), __builtin_signbit(dz),\\n"
		 "__builtin_signbitf(f), __builtin_signbitl(l), __builtin_signbitl(lz),\\n"
		 "fetestexcept(FE_ALL_EXCEPT)!=0); return 0; }\\n' > {W}/bsb.c && "
		 "{MCC} -B{B} -I{I} {W}/bsb.c -o {W}/bsb -lm && {W}/bsb",
		 "c=1111 r=011110 exc=0\n"},

		{"atomic_inlang_aggregate", "elf",
		 "printf '#include <stdatomic.h>\\ntypedef struct{long a,b,c;}Big;\\n"
		 "_Atomic Big g; _Atomic long double ld;\\n"
		 "void f(Big v){ g = v; }\\nvoid h(long double x){ ld = x; }\\n"
		 "void r(Big *p){ *p = g; }\\n' > {W}/ia.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/ia.c -o {W}/ia.o && "
		 "nm {W}/ia.o | grep -oE 'U __atomic_(store|load)' | sort -u",
		 "U __atomic_load\nU __atomic_store\n"},

		{"imaginary_not_supported", "",
		 "printf '_Imaginary float x;\\n' > {W}/im.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/im.c -o {W}/im.o 2>&1 | "
		 "grep -oE 'imaginary types are not supported'; "
		 "printf '#include <complex.h>\\ndouble _Complex z=1.0;\\nint main(void){return 0;}\\n' > {W}/cx.c && "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/cx.c -o {W}/cx && echo COMPLEX_OK",
		 "imaginary types are not supported\nCOMPLEX_OK\n"},

		{"noreturn_returns", "",
		 "printf '_Noreturn void f(int x){ if(x) return; for(;;); }\\n"
		 "void ok(void){ return; }\\nint main(void){return 0;}\\n' > {W}/nr.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/nr.c -o {W}/nr.o 2>&1 | "
		 "grep -c \"function declared 'noreturn' has a 'return' statement\"",
		 "1\n"},

		{"va_args_non_variadic", "",
		 "printf '#define F(a) __VA_ARGS__\\n#define V(a, ...) __VA_ARGS__\\n"
		 "int main(void){return 0;}\\n' > {W}/va.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/va.c -o {W}/va.o 2>&1 | "
		 "grep -c 'can only appear in the expansion'",
		 "1\n"},

		{"unknown_directive_error", "",
		 "printf '#frobnicate xyz\\nint main(void){return 0;}\\n' > {W}/ud.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/ud.c -o {W}/ud.o 2>&1 | "
		 "grep -oE 'invalid preprocessing directive #frobnicate'; "
		 "printf '#ident \"v\"\\n#sccs \"w\"\\nint main(void){return 0;}\\n' > {W}/ui.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/ui.c -o {W}/ui.o && echo IDENT_OK",
		 "invalid preprocessing directive #frobnicate\nIDENT_OK\n"},

		{"pragma_message_note", "",
		 "printf '#pragma message(\"hi there\")\\n#pragma message \"bare form\"\\n"
		 "int main(void){return 0;}\\n' > {W}/pm.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/pm.c -o {W}/pm.o 2>&1 | "
		 "grep -oE 'note: #pragma message: (hi there|bare form)'",
		 "note: #pragma message: hi there\nnote: #pragma message: bare form\n"},

		{"function_return_type_constraint", "",
		 "printf 'typedef int AT[3]; AT f(void);\\n' > {W}/rt1.c && "
		 "printf 'typedef int FT(void); FT g(void);\\n' > {W}/rt2.c && "
		 "printf 'typedef int AT[3]; AT *ok1(void); typedef int FT(void); FT *ok2(void);\\n"
		 "int main(void){return 0;}\\n' > {W}/rt3.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/rt1.c -o {W}/rt1.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/rt2.c -o {W}/rt2.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/rt3.c -o {W}/rt3 2>&1 && echo PTR_OK; } | "
		 "grep -oE 'function cannot return an? (array|function) type|PTR_OK' | sort",
		 "PTR_OK\nfunction cannot return a function type\nfunction cannot return an array type\n"},

		{"func_outside_function", "",
		 "printf 'const char *p = __func__;\\n' > {W}/fn.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/fn.c -o {W}/fn.o 2>&1 | "
		 "grep -oE \"'__func__' is not defined outside of function scope\"",
		 "'__func__' is not defined outside of function scope\n"},

		{"lexical_alignof_constraints", "",
		 "printf 'long long a=1Ll;\\n' > {W}/ix1.c && "
		 "printf 'long long a=1lL;\\n' > {W}/ix2.c && "
		 "printf 'struct S; int n=_Alignof(struct S);\\n' > {W}/ix3.c && "
		 "printf 'long long a=1LL,b=1ll; unsigned long long c=1ull,d=1ULL;\\n"
		 "struct T{int x;}; int n=(int)_Alignof(struct T)+(int)_Alignof(int);\\n"
		 "int main(void){return 0;}\\n' > {W}/ix4.c && "
		 "{ for f in ix1 ix2 ix3; do {MCC} -B{B} -I{I} -std=c11 -c {W}/$f.c -o {W}/$f.o 2>&1; done; "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/ix4.c -o {W}/ix4 2>&1 && echo VALID_OK; } | "
		 "grep -oE 'incorrect integer suffix|_Alignof. applied to an incomplete type|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 VALID_OK\n1 _Alignof' applied to an incomplete type\n2 incorrect integer suffix\n"},

		{"ucn_identifier_range", "",
		 "printf 'int \\\\u0041 = 5;\\n' > {W}/un1.c && "
		 "printf 'int \\\\U0000d800x;\\n' > {W}/un2.c && "
		 "printf 'int \\\\u00e9 = 5;\\nint main(void){return \\\\u00e9-5;}\\n' > {W}/un3.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/un1.c -o {W}/un1.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/un2.c -o {W}/un2.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/un3.c -o {W}/un3 2>&1 && echo UCN_OK; } | "
		 "grep -oE 'universal character .u(0041|d800) is not valid in an identifier|UCN_OK' | sort",
		 "UCN_OK\nuniversal character \\u0041 is not valid in an identifier\nuniversal character \\ud800 is not valid in an identifier\n"},

		{"implicit_decl_in_knr_body", "",
		 "printf 'int main(){ return foo(); }\\n' > {W}/id.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -Wall -c {W}/id.c -o {W}/id.o 2>&1 | "
		 "grep -oE \"implicit declaration of function 'foo'\"",
		 "implicit declaration of function 'foo'\n"},

		{"implicit_decl_default_error", "",
		 "printf 'int main(void){ return baz(); }\\n' > {W}/ide.c && "
		 "{MCC} -B{B} -I{I} -c {W}/ide.c -o {W}/ide.o 2>&1 | grep -oE 'error: implicit declaration' | head -1",
		 "error: implicit declaration\n"},

		{"implicit_decl_downgradable", "",
		 "printf 'int main(void){ return baz(); }\\n' > {W}/idd.c && "
		 "{MCC} -B{B} -I{I} -Wno-error=implicit-function-declaration -c {W}/idd.c -o {W}/idd.o 2>&1 | grep -oE 'warning: implicit declaration' | head -1",
		 "warning: implicit declaration\n"},

		{"bool_bitfield_width", "",
		 "printf 'struct S{_Bool b:2;};\\n' > {W}/bb.c && "
		 "printf 'struct T{_Bool b:1; int d:8;}; int main(void){return 0;}\\n' > {W}/bg.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/bb.c -o {W}/bb.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/bg.c -o {W}/bg 2>&1 && echo VALID_OK; } | "
		 "grep -oE \"width of '.' exceeds its type|VALID_OK\"",
		 "width of 'b' exceeds its type\nVALID_OK\n"},

		{"register_address_constraint", "",
		 "printf 'int *f(void){register int x=0; return &x;}\\n' > {W}/rga.c && "
		 "printf 'int main(void){register int x=5; int y=x; int *p=&y; return *p+x-10;}\\n' > {W}/rgb.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/rga.c -o {W}/rga.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/rgb.c -o {W}/rgb 2>&1 && echo VALID_OK; } | "
		 "grep -oE \"address of register variable '.' requested|VALID_OK\"",
		 "address of register variable 'x' requested\nVALID_OK\n"},

		{"implicit_int_diag", "",
		 "printf 'const x = 3;\\nstatic y = 7;\\nfoo(void){return 0;}\\n' > {W}/ii.c && "
		 "printf 'long a; unsigned b; const int c; int g(void){return 0;}\\nint main(void){return g();}\\n' > {W}/iv.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -Wno-error=implicit-int -c {W}/ii.c -o {W}/ii.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Wall {W}/iv.c -o {W}/iv 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE \"type defaults to 'int' in declaration|return type defaults to 'int'|CLEAN_OK\" | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 return type defaults to 'int'\n2 type defaults to 'int' in declaration\n"},

		{"default_diag_severity", "",
		 "printf 'char *p; int *q; void f(void){p=q;}\\n' > {W}/a.c && "
		 "printf 'int *p = 3;\\n' > {W}/b.c && "
		 "printf 'foo(void){return 0;}\\n' > {W}/c.c && "
		 "printf '__attribute__((frobnicate)) int x;\\nint main(void){return 0;}\\n' > {W}/d.c && "
		 "printf 'struct S{int a;};\\nstruct S s={1,2};\\nint main(void){return s.a-1;}\\n' > {W}/e.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/a.c -o {W}/o.o 2>&1; "
		 "{MCC} -B{B} -I{I} -c {W}/b.c -o {W}/o.o 2>&1; "
		 "{MCC} -B{B} -I{I} -c {W}/c.c -o {W}/o.o 2>&1; "
		 "{MCC} -B{B} -I{I} -c {W}/d.c -o {W}/o.o 2>&1 && echo ATTR_OK; "
		 "{MCC} -B{B} -I{I} -c {W}/e.c -o {W}/o.o 2>&1 && echo EXCESS_OK; "
		 "{MCC} -B{B} -I{I} -fpermissive -c {W}/a.c -o {W}/o.o 2>&1 && echo PERMISSIVE_OK; "
		 "{MCC} -B{B} -I{I} -std=c89 -c {W}/b.c -o {W}/o.o 2>&1 && echo C89_OK; } | "
		 "grep -oE \"error: assignment from incompatible pointer type|"
		 "error: assignment makes pointer from integer without a cast|"
		 "error: return type defaults to 'int'|"
		 "warning: 'frobnicate' attribute ignored|"
		 "warning: excess elements in initializer|"
		 "warning: assignment from incompatible pointer type|"
		 "warning: assignment makes pointer from integer without a cast|"
		 "ATTR_OK|EXCESS_OK|PERMISSIVE_OK|C89_OK\" | sort | uniq -c | sed 's/^ *//'",
		 "1 ATTR_OK\n1 C89_OK\n1 EXCESS_OK\n1 PERMISSIVE_OK\n"
		 "1 error: assignment from incompatible pointer type\n"
		 "1 error: assignment makes pointer from integer without a cast\n"
		 "1 error: return type defaults to 'int'\n"
		 "1 warning: 'frobnicate' attribute ignored\n"
		 "1 warning: assignment from incompatible pointer type\n"
		 "1 warning: assignment makes pointer from integer without a cast\n"
		 "1 warning: excess elements in initializer\n"},

		{"wsequence_point_diag", "",
		 "printf 'void g(void){int i=0;i=i++;}\\n' > {W}/spw.c && "
		 "printf 'int f(int a,int b){return a+b;}\\n"
		 "void h(void){int i=0,j=0,a=0;i=i+1;i++,j++;f(i++,j++);a=i?i++:j++;}\\n' > {W}/spo.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/spw.c -o {W}/spw.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Wno-sequence-point -c {W}/spw.c -o {W}/spw.o 2>&1; "
		 "{MCC} -B{B} -I{I} -c {W}/spo.c -o {W}/spo.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE \"operation on 'i' may be undefined|CLEAN_OK\" | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 operation on 'i' may be undefined\n"},

		{"wsequence_point_subobject", "",
		 "printf 'struct S{int a,b;};\\n"
		 "void g(void){struct S s;int a[4];s.a=s.a++;a[2]=a[2]++;(void)s;(void)a;}\\n' > {W}/spsw.c && "
		 "printf 'struct S{int a,b;};\\n"
		 "void h(void){struct S s;int a[4];s.a=s.b;a[0]=a[1];(void)s;(void)a;}\\n' > {W}/spso.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/spsw.c -o {W}/spsw.o 2>&1; "
		 "{MCC} -B{B} -I{I} -c {W}/spso.c -o {W}/spso.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE \"operation on '[sa]' may be undefined|CLEAN_OK\" | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 operation on 'a' may be undefined\n1 operation on 's' may be undefined\n"},

		{"wsequence_point_interarg", "",
		 "printf 'void g(int,int);\\nvoid f(int i){ g(i++, i++); }\\n' > {W}/spi.c && "
		 "printf 'void g(int,int);\\nvoid h(int i,int j){ g(i++, j++); }\\n' > {W}/spj.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/spi.c -o {W}/spi.o 2>&1; "
		 "{MCC} -B{B} -I{I} -c {W}/spj.c -o {W}/spj.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE \"operation on 'i' may be undefined|CLEAN_OK\" | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 operation on 'i' may be undefined\n"},

		{"jump_constraints", "",
		 "printf 'void f(void){break;}\\n' > {W}/j1.c && "
		 "printf 'void f(void){continue;}\\n' > {W}/j2.c && "
		 "printf 'void f(void){case 1:;}\\n' > {W}/j3.c && "
		 "printf 'void f(int x){switch(x){case 1:;case 1:;}}\\n' > {W}/j4.c && "
		 "printf 'int f(int x){int s=0;for(int i=0;i<x;i++){if(i==2)continue;if(i==5)break;s+=i;}"
		 "switch(x){case 1:s++;break;default:s--;}return s;}\\n' > {W}/jok.c && "
		 "{ for n in j1 j2 j3 j4; do {MCC} -B{B} -I{I} -c {W}/$n.c -o {W}/$n.o 2>&1; done; "
		 "{MCC} -B{B} -I{I} -c {W}/jok.c -o {W}/jok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE \"cannot break|cannot continue|duplicate case value|switch expected|CLEAN_OK\" | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 cannot break\n1 cannot continue\n1 duplicate case value\n1 switch expected\n"},

		{"common_symbol_merge", "os!=WIN32",
		 "printf 'int shared_g;\\nvoid set_it(void){ shared_g = 5; }\\n' > {W}/cm1.c && "
		 "printf '#include <stdio.h>\\nint shared_g; void set_it(void);\\n"
		 "int main(void){ set_it(); printf(\"%%d\\\\n\", shared_g); return 0; }\\n' > {W}/cm2.c && "
		 "{MCC} -B{B} -I{I} -fcommon {W}/cm1.c {W}/cm2.c -o {W}/cme && {W}/cme",
		 "5\n"},

		{"unary_minus_pointer", "",
		 "printf 'int f(int*p){return (-p)==0;}\\n' > {W}/umn.c && "
		 "printf 'int f(int x){return -x;}\\nint main(void){return 0;}\\n' > {W}/umok.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/umn.c -o {W}/umn.o 2>&1; "
		 "{MCC} -B{B} -I{I} {W}/umok.c -o {W}/umok 2>&1 && echo VALID_OK; } | "
		 "grep -oE 'pointer not accepted for unary minus|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 VALID_OK\n1 pointer not accepted for unary minus\n"},

		{"escape_out_of_range", "",
		 "printf 'char *s=\"\\\\777\";\\n' > {W}/eo.c && "
		 "printf 'char *s=\"\\\\xfff\";\\n' > {W}/ex.c && "
		 "printf 'char *s=\"\\\\77\\\\xff\";\\nint main(void){return 0;}\\n' > {W}/eok.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/eo.c -o {W}/eo.o 2>&1; "
		 "{MCC} -B{B} -I{I} -c {W}/ex.c -o {W}/ex.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror {W}/eok.c -o {W}/eok 2>&1 && echo VALID_OK; } | "
		 "grep -oE 'octal escape sequence out of range|hex escape sequence out of range|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 VALID_OK\n1 hex escape sequence out of range\n1 octal escape sequence out of range\n"},

		{"decl_storage_type_constraints", "",
		 "printf 'void f(const void);\\n' > {W}/dq.c && "
		 "printf '_Thread_local typedef int T;\\n' > {W}/dt.c && "
		 "printf 'void f(int n){ extern int a[n]; }\\n' > {W}/dv.c && "
		 "printf 'void f(void); _Thread_local static int x;\\n"
		 "void g(int n){int a[n]; a[0]=0; (void)a;}\\nint main(void){return 0;}\\n' > {W}/dok.c && "
		 "{ for n in dq dt dv; do {MCC} -B{B} -I{I} -std=c11 -c {W}/$n.c -o {W}/$n.o 2>&1; done; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dok.c -o {W}/dok.o 2>&1 && echo VALID_OK; } | "
		 "grep -oE \"only parameter may not be qualified|'_Thread_local' used with 'typedef'|must have no linkage|VALID_OK\" | sort | uniq -c | sed 's/^ *//'",
		 "1 '_Thread_local' used with 'typedef'\n1 VALID_OK\n1 must have no linkage\n1 only parameter may not be qualified\n"},

		{"builtin_macro_redefine", "",
		 "printf '#define __LINE__ 5\\n#undef __FILE__\\nint x;\\n' > {W}/bm.c && "
		 "printf '#define FOO 1\\n#undef FOO\\nint y;\\n' > {W}/bmok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/bm.c -o {W}/bm.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/bmok.c -o {W}/bmok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE '__LINE__ redefined|undefining __FILE__|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 __LINE__ redefined\n1 undefining __FILE__\n"},

		{"lvalue_cast_comma_constraints", "",
		 "printf 'int f(int a){ (int)a = 9; return a; }\\n' > {W}/l1.c && "
		 "printf 'int f(int a){ return *&(int)a; }\\n' > {W}/l2.c && "
		 "printf 'int f(int a,int b){ (a,b) = 7; return b; }\\n' > {W}/l3.c && "
		 "printf 'int f(int a,int b){ return *&(a,b); }\\n' > {W}/l4.c && "
		 "printf 'int f(int a,int b){ int c=(int)a+1; c=(a,b); return c; }\\nint main(void){return 0;}\\n' > {W}/lok.c && "
		 "{ for n in l1 l2 l3 l4; do {MCC} -B{B} -I{I} -c {W}/$n.c -o {W}/$n.o 2>&1; done; "
		 "{MCC} -B{B} -I{I} {W}/lok.c -o {W}/lok 2>&1 && echo VALID_OK; } | "
		 "grep -oE 'lvalue expected|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 VALID_OK\n4 lvalue expected\n"},

		{"paste_comment_introducer", "",
		 "printf '#define C(a,b) a ## b\\nC(/,/)\\n' > {W}/pc1.c && "
		 "printf '#define C(a,b) a ## b\\nC(/,*)\\n' > {W}/pc2.c && "
		 "printf '#define C(a,b) a ## b\\nint C(foo,bar)=5;\\nint main(void){return foobar-5;}\\n' > {W}/pcok.c && "
		 "{ {TIMEOUT}{MCC} -B{B} -I{I} -std=c11 -E -P {W}/pc1.c 2>&1; "
		 "{TIMEOUT}{MCC} -B{B} -I{I} -std=c11 -E -P {W}/pc2.c 2>&1; "
		 "{MCC} -B{B} -I{I} {W}/pcok.c -o {W}/pcok 2>&1 && echo VALID_OK; } | "
		 "grep -oE 'invalid preprocessing token|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 VALID_OK\n2 invalid preprocessing token\n"},

		{"stringize_trailing_backslash", "",
		 "printf '#define S(x) #x\\nconst char *p = S(a\\\\);\\nint main(void){return p[0];}\\n' > {W}/sb.c && "
		 "printf '#define S(x) #x\\nconst char *a=S(hi);const char *b=S(a\\\\\\\\);"
		 "int main(void){return a[0]+b[0];}\\n' > {W}/sbok.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/sb.c -o {W}/sb.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/sbok.c -o {W}/sbok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'ignoring final|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 ignoring final\n"},

		{"complex_creal_function", "os!=WIN32",
		 "printf '#include <complex.h>\\n#include <stdio.h>\\n"
		 "int main(void){ double _Complex z=3.0+4.0*I; double(*p)(double _Complex)=creal;\\n"
		 "if((int)(creal)(z)==3 && (int)cimag(z)==4 && (int)p(z)==3) puts(\"OK\"); return 0; }\\n' > {W}/cf.c && "
		 "{MCC} -B{B} -I{I} {W}/cf.c -lm -o {W}/cf && {W}/cf",
		 "OK\n"},

		{"uchar_header", "os!=WIN32",
		 "printf '#include <uchar.h>\\nint main(void){char16_t a=0; char32_t b=0; mbstate_t s;\\n"
		 "(void)a;(void)b;(void)s; return (sizeof(char16_t)==2 && sizeof(char32_t)==4)?0:1;}\\n' > {W}/uh.c && "
		 "{MCC} -B{B} -I{I} {W}/uh.c -o {W}/uh && {W}/uh && echo HOSTED_OK && "
		 "{MCC} -B{B} -nostdinc -I{I} {W}/uh.c -o {W}/uhf && {W}/uhf && echo FREE_OK",
		 "HOSTED_OK\nFREE_OK\n"},

		{"trigraphs_strict_std", "",
		 "printf 'int a?"
		 "?(2?"
		 "?);\\n' > {W}/tg.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -E -P {W}/tg.c 2>&1; "
		 "{MCC} -B{B} -I{I} -std=gnu11 -E -P {W}/tg.c 2>&1; } | "
		 "grep -oE 'a\\?\\?\\(|a\\[2\\]' | sort | uniq -c | sed 's/^ *//'",
		 "1 a?"
		 "?(\n1 a[2]\n"},

		{"va_args_empty_pedantic", "",
		 "printf '#define V(f,...) f\\nint a = V(1);\\n' > {W}/ve.c && "
		 "printf '#define V(f,...) f\\nint b = V(1,2);\\nint main(void){return 0;}\\n' > {W}/vok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/ve.c -o {W}/ve.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/vok.c -o {W}/vok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'no argument for the|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 no argument for the\n"},

		{"pp_if_integer_overflow", "",
		 "printf '#if 9223372036854775807 + 1 < 0\\nint a;\\n#endif\\nint main(void){return 0;}\\n' > {W}/po.c && "
		 "printf '#if 9223372036854775807 * 2 < 0\\nint b;\\n#endif\\nint main(void){return 0;}\\n' > {W}/pm.c && "
		 "printf '#if 2147483647 + 1 > 0 && 18446744073709551615U + 1U == 0U\\nint c;\\n#endif\\n"
		 "int main(void){return 0;}\\n' > {W}/pok.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/po.c -o {W}/po.o 2>&1; "
		 "{MCC} -B{B} -I{I} -c {W}/pm.c -o {W}/pm.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/pok.c -o {W}/pok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'overflow in preprocessor|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n2 overflow in preprocessor\n"},

		{"line_number_out_of_range", "",
		 "printf '#line 2147483648\\nint x;\\n#line 0\\nint y;\\nint main(void){return 0;}\\n' > {W}/lr.c && "
		 "printf '#line 100\\nint z;\\nint main(void){return 0;}\\n' > {W}/lrok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/lr.c -o {W}/lr.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/lrok.c -o {W}/lrok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'line number out of range|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n2 line number out of range\n"},

		{"string_init_element_mismatch",
		 "os!=WIN32:int[]=L\"\" is clean only where wchar_t==int; PE wchar_t is 16-bit so it warns",
		 "printf 'int a[4]=\"abc\";\\n' > {W}/sm.c && "
		 "printf 'int wmain(void){char a[]=L\"abc\";return a[0];}\\n' > {W}/sm2.c && "
		 "printf 'char a[]=\"abc\"; int b[]=L\"abc\"; char c[4]={\"ab\"};"
		 " char m[][3]={\"ab\",\"cd\"}; int main(void){return a[0]+b[0]+c[0]+m[0][0];}\\n' > {W}/smok.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/sm.c -o {W}/sm.o 2>&1; "
		 "{MCC} -B{B} -I{I} -c {W}/sm2.c -o {W}/sm2.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/smok.c -o {W}/smok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'from a string literal of a different|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n2 from a string literal of a different\n"},

		{"float_derived_array_size", "",
		 "printf 'int a[(int)(1.0+2.0)]; int b[(1.0<2.0)?4:2];\\n' > {W}/fd.c && "
		 "printf 'int c[(int)3.0]; int d[(int)1.5+(int)2.5]; int e[3+4];\\n"
		 "int g(void){int z[(int)(1.0+2.0)]; return sizeof z;}\\n"
		 "int main(void){return 0;}\\n' > {W}/fdok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/fd.c -o {W}/fd.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/fdok.c -o {W}/fdok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'not an integer constant expression|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n2 not an integer constant expression\n"},

		{"label_then_declaration", "",
		 "printf 'typedef int T;\\nint f(int c){switch(c){case 1: int z=0; return z; default: return 1;}}\\n"
		 "int g(void){ L: int y=0; return y; }\\nint h(void){ M: T t=0; return t; }\\n' > {W}/lab.c && "
		 "printf 'int f(void){ L: return 0; }\\n' > {W}/labok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/lab.c -o {W}/lab.o 2>&1 | grep -c 'declaration is not a statement'; "
		 "{MCC} -B{B} -I{I} -std=c23 -c {W}/lab.c -o {W}/lab23.o 2>&1 | grep -c 'declaration is not a statement'; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/labok.c -o {W}/labok.o 2>&1 && echo CLEAN_OK; }",
		 "3\n0\nCLEAN_OK\n"},

		{"register_param_address", "",
		 "printf 'int f(register int n){ int *p=&n; return *p; }\\n' > {W}/rp.c && "
		 "printf 'int g(int n){ int *p=&n; return *p; }\\n' > {W}/rpok.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/rp.c -o {W}/rp.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/rpok.c -o {W}/rpok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'address of register variable|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 address of register variable\n"},

		{"atomic_flag_type", "",
		 "printf '#include <stdatomic.h>\\nvoid f(void){int x=0; atomic_flag_clear(x);}\\n' > {W}/aft.c && "
		 "printf '#include <stdatomic.h>\\nvoid g(void){atomic_flag a=ATOMIC_FLAG_INIT; atomic_flag_clear(&a);}\\n' > {W}/afok.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/aft.c -o {W}/aft.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/afok.c -o {W}/afok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'pointer from integer|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 pointer from integer\n"},

		{"linkage_static_after_extern", "",
		 "printf 'extern int x; static int x;\\n' > {W}/les.c && "
		 "printf 'static int y; extern int y;\\n' > {W}/lse.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/les.c -o {W}/les.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/lse.c -o {W}/lse.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'follows non-static|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 follows non-static\n"},

		{"inline_extern_static_object", "",
		 "printf 'inline int counter(void){ static int n; return ++n; }\\n"
		 "int(*p)(void)=counter;\\n' > {W}/ie.c && "
		 "printf 'inline int a(void){static const int c=5;return c;}\\n"
		 "static inline int b(void){static int m;return ++m;}\\n"
		 "extern inline int d(void){static int k;return ++k;}\\n"
		 "int main(void){return a();}\\n' > {W}/ieok.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/ie.c -o {W}/ie.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/ieok.c -o {W}/ieok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'static but declared in inline|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 static but declared in inline\n"},

		{"rvalue_struct_member", "",
		 "printf 'struct S{int x;}; struct S g(void); void f(void){ g().x = 3; }\\n' > {W}/r1.c && "
		 "printf 'struct S{int x;}; struct S g(void); int*f(void){ return &g().x; }\\n' > {W}/r2.c && "
		 "printf 'struct S{int x,y;}; struct S g(void){struct S s={1,2};return s;}\\n"
		 "struct S*gp(void){static struct S s;return &s;}\\n"
		 "int f(void){ int a=g().x; struct S c=g(); gp()->x=7; return a+c.x+gp()->x; }\\n"
		 "int main(void){return 0;}\\n' > {W}/rok.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/r1.c -o {W}/r1.o 2>&1; "
		 "{MCC} -B{B} -I{I} -c {W}/r2.c -o {W}/r2.o 2>&1; "
		 "{MCC} -B{B} -I{I} {W}/rok.c -o {W}/rok 2>&1 && echo VALID_OK; } | "
		 "grep -oE 'is not assignable|address of a function-call|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 VALID_OK\n1 address of a function-call\n1 is not assignable\n"},

		{"storage_class_exclusivity", "",
		 "printf 'static auto int a;\\n' > {W}/sc1.c && "
		 "printf 'register static int b;\\n' > {W}/sc2.c && "
		 "printf 'auto auto int c;\\n' > {W}/sc3.c && "
		 "printf 'void f(register int n){(void)n; auto int x=5; (void)x;}\\n"
		 "static int s; extern int e; typedef int T;\\nint main(void){return 0;}\\n' > {W}/scok.c && "
		 "{ for n in sc1 sc2 sc3; do {MCC} -B{B} -I{I} -c {W}/$n.c -o {W}/$n.o 2>&1; done; "
		 "{MCC} -B{B} -I{I} {W}/scok.c -o {W}/scok 2>&1 && echo VALID_OK; } | "
		 "grep -oE 'multiple storage classes|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 VALID_OK\n3 multiple storage classes\n"},

		{"array_static_param", "",
		 "printf 'void f(int a[static]);\\n' > {W}/ap.c && "
		 "printf 'void g(int a[static 3]); void h(int a[const 2]); void i(int a[]);\\n"
		 "int main(void){return 0;}\\n' > {W}/apok.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/ap.c -o {W}/ap.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/apok.c -o {W}/apok.o 2>&1 && echo VALID_OK; } | "
		 "grep -oE 'without an array size|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 VALID_OK\n1 without an array size\n"},

		{"ident_backslash_no_hang", "",
		 "printf 'a\\\\ \\nb\\n' > {W}/ib.c && "
		 "{TIMEOUT}{MCC} -B{B} -I{I} -E -P {W}/ib.c >/dev/null 2>&1 && echo TERMINATED",
		 "TERMINATED\n"},

		{"freestanding_hosted_macro", "",
		 "printf '__STDC_HOSTED__\\n' > {W}/fh.c && "
		 "{ {MCC} -B{B} -I{I} -ffreestanding -E -P {W}/fh.c; "
		 "{MCC} -B{B} -I{I} -ffreestanding -fhosted -E -P {W}/fh.c; "
		 "{MCC} -B{B} -I{I} -E -P {W}/fh.c; } | tr -d ' '",
		 "0\n1\n1\n"},

		{"empty_struct_pedantic", "",
		 "printf 'struct S{}; int main(void){return 0;}\\n' > {W}/es.c && "
		 "printf 'union U{}; int main(void){return 0;}\\n' > {W}/eu.c && "
		 "printf 'struct S{int:4;}; int main(void){return 0;}\\n' > {W}/ubf.c && "
		 "printf 'union U{int:4;}; int main(void){return 0;}\\n' > {W}/ubfu.c && "
		 "printf 'struct B{int x;}; struct A{struct{int y;};}; struct C{int x;int:4;};"
		 " int main(void){return 0;}\\n' > {W}/eok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/es.c -o {W}/es.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/eu.c -o {W}/eu.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/ubf.c -o {W}/ubf.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/ubfu.c -o {W}/ubfu.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/eok.c -o {W}/eok.o 2>&1 && echo VALID_OK; } | "
		 "grep -oE 'no named members|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 VALID_OK\n4 no named members\n"},

		{"empty_declaration_pedantic", "",
		 "printf 'int x;;\\n' > {W}/ed.c && "
		 "printf 'int f(void){ ; ; return 0; }\\n' > {W}/edok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/ed.c -o {W}/ed.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/edok.c -o {W}/edok.o 2>&1 && echo VALID_OK; } | "
		 "grep -oE 'empty declaration|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 VALID_OK\n1 empty declaration\n"},

		{"void_fn_pointer_arith", "",
		 "printf 'void f(void*p){p++;} long g(void*a,void*b){return a-b;}\\n"
		 "void h(int(*fp)(void)){fp++;}\\n' > {W}/pa.c && "
		 "printf 'int f(int*p){return *(p+1);} long g(int*a,int*b){return a-b;}\\n"
		 "char*h(char*p){return p+3;}\\n' > {W}/paok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/pa.c -o {W}/pa.o 2>&1 | grep -c 'forbids arithmetic'; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/paok.c -o {W}/paok.o 2>&1 && echo VALID_OK; }",
		 "3\nVALID_OK\n"},

		{"fn_pointer_void_conversion", "",
		 "printf 'void*f(int(*fp)(void)){void*v=fp;return v;}\\n' > {W}/c1.c && "
		 "printf 'int(*g(void*v))(void){int(*fp)(void)=v;return fp;}\\n' > {W}/c2.c && "
		 "printf 'void*f(int*p){void*v=p;return v;} void*g(int(*fp)(void)){return (void*)fp;}\\n' > {W}/cok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/c1.c -o {W}/c1.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/c2.c -o {W}/c2.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/cok.c -o {W}/cok.o 2>&1 && echo VALID_OK; } | "
		 "grep -oE 'function pointer and|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 VALID_OK\n2 function pointer and\n"},

		{"typedef_redefinition_c99", "",
		 "printf 'typedef int T; typedef int T;\\n' > {W}/td.c && "
		 "printf 'typedef int U; typedef long U;\\n' > {W}/tdbad.c && "
		 "{ {MCC} -B{B} -I{I} -std=c99 -pedantic -c {W}/td.c -o {W}/td.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/td.c -o {W}/td2.o 2>&1 && echo C11_OK; "
		 "{MCC} -B{B} -I{I} -c {W}/tdbad.c -o {W}/tdbad.o 2>&1; } | "
		 "grep -oE 'redefinition of typedef is a C11 feature|incompatible redefinition|C11_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 C11_OK\n1 incompatible redefinition\n1 redefinition of typedef is a C11 feature\n"},

		{"anon_member_c99_and_sysheader", "",
		 "mkdir -p {W}/sys && printf 'struct H{ struct { int x; }; };\\n' > {W}/sys/h.h && "
		 "printf '#include <h.h>\\nstruct U{ struct { int y; }; }; int main(void){return 0;}\\n' > {W}/m.c && "
		 "{ {MCC} -B{B} -I{I} -isystem {W}/sys -std=c99 -pedantic -c {W}/m.c -o {W}/m.o 2>&1 | grep -c 'C11 feature'; "
		 "{MCC} -B{B} -I{I} -isystem {W}/sys -std=c11 -pedantic-errors -c {W}/m.c -o {W}/m11.o 2>&1 && echo C11_OK; }",
		 "1\nC11_OK\n"},

		{"pedantic_errors_predef_clean", "",
		 "printf 'int main(void){return 0;}\\n' > {W}/pe.c && "
		 "printf 'struct S{struct{int x;};}; int main(void){return 0;}\\n' > {W}/peb.c && "
		 "{ {MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/pe.c -o {W}/pe.o 2>&1 && echo CLEAN_OK; "
		 "{MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/peb.c -o {W}/peb.o 2>&1; } | "
		 "grep -oE 'C11 feature|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 C11 feature\n1 CLEAN_OK\n"},

		{"nested_pointer_qualifier_launder", "",
		 "printf 'void f(int **p){ const int **q = p; (void)q; }\\n' > {W}/nl.c && "
		 "printf 'void f(int *ip, const int *cp){ const int *a = ip; int *b = cp; (void)a;(void)b; }\\n' > {W}/nlmix.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/nl.c -o {W}/nl.o 2>&1 | grep -oE 'incompatible pointer'; "
		 "{MCC} -B{B} -I{I} -c {W}/nlmix.c -o {W}/nlmix.o 2>&1 | grep -oE 'discards|incompatible'; } | "
		 "sort | uniq -c | sed 's/^ *//'",
		 "1 discards\n1 incompatible pointer\n"},

		{"pp_macro_name_constraints", "",
		 "printf '#define M(a,a) a\\n' > {W}/mdp.c && "
		 "printf '#undef defined\\n' > {W}/mud.c && "
		 "printf '#define __VA_ARGS__ 1\\n' > {W}/mva.c && "
		 "printf '#undef __STDC__\\nint main(void){return 0;}\\n' > {W}/mus.c && "
		 "printf '#define V(a,...) a\\n#define W2(x,y) x y\\nint main(void){return 0;}\\n' > {W}/mok.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/mdp.c -o {W}/mdp.o 2>&1; "
		 "{MCC} -B{B} -I{I} -c {W}/mud.c -o {W}/mud.o 2>&1; "
		 "{MCC} -B{B} -I{I} -c {W}/mva.c -o {W}/mva.o 2>&1; "
		 "{MCC} -B{B} -I{I} -c {W}/mus.c -o {W}/mus.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/mok.c -o {W}/mok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'duplicate macro parameter|invalid macro name|undefining __STDC__|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 duplicate macro parameter\n2 invalid macro name\n1 undefining __STDC__\n"},

		{"sizeof_alignof_void", "",
		 "printf 'int a=_Alignof(void);\\n' > {W}/sav.c && "
		 "printf 'int f(int x){return _Alignof(x);}\\n' > {W}/sae.c && "
		 "printf 'unsigned long g(void){return sizeof(void);}\\n' > {W}/ssv.c && "
		 "printf 'int h(int x){return __alignof__(x)+__alignof__(void);}\\nint main(void){return 0;}\\n' > {W}/sgnu.c && "
		 "{ {MCC} -B{B} -I{I} -pedantic -c {W}/sav.c -o {W}/sav.o 2>&1; "
		 "{MCC} -B{B} -I{I} -pedantic-errors -c {W}/sae.c -o {W}/sae.o 2>&1; "
		 "{MCC} -B{B} -I{I} -pedantic-errors -c {W}/ssv.c -o {W}/ssv.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/sgnu.c -o {W}/sgnu.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE '_Alignof. applied to a void|sizeof. applied to a void|applied to an expression|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 _Alignof' applied to a void\n1 applied to an expression\n1 sizeof' applied to a void\n"},

		{"function_def_typedef", "",
		 "printf 'typedef int f(void){return 42;}\\n' > {W}/ft.c && "
		 "printf 'typedef int myf(void); int g(void){return 1;}\\nint main(void){return 0;}\\n' > {W}/ftok.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/ft.c -o {W}/ft.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/ftok.c -o {W}/ftok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'function definition declared|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 function definition declared\n"},

		{"init_brace_constraints", "",
		 "printf 'struct V{int len;int data[];}; struct V v={1,{2,3}};\\n' > {W}/fam.c && "
		 "printf 'int x={{1}};\\n' > {W}/sb.c && "
		 "printf 'struct V{int len;int data[];}; struct V v={1};\\nstruct S{int a;}; struct S s={{5}};\\nint y={7}; int a3[]={1,2,3};\\nint main(void){return 0;}\\n' > {W}/iok.c && "
		 "{ {MCC} -B{B} -I{I} -pedantic-errors -c {W}/fam.c -o {W}/fam.o 2>&1; "
		 "{MCC} -B{B} -I{I} -pedantic-errors -c {W}/sb.c -o {W}/sb.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -pedantic -c {W}/iok.c -o {W}/iok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'flexible array member|too many braces around scalar|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 flexible array member\n1 too many braces around scalar\n"},

		{"void_pointer_deref", "",
		 "printf 'void f(void*p){*p;}\\n' > {W}/vd.c && "
		 "printf 'void g(int*p){*p=1; (void)*p;}\\nint main(void){return 0;}\\n' > {W}/vdok.c && "
		 "{ {MCC} -B{B} -I{I} -pedantic-errors -c {W}/vd.c -o {W}/vd.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -pedantic -c {W}/vdok.c -o {W}/vdok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'dereferencing a .void|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 dereferencing a 'void\n"},

		{"const_integer_overflow", "",
		 "printf 'int x = 100000 * 100000;\\n' > {W}/co.c && "
		 "printf 'int a = 2000000000 + 100000000;\\nunsigned b = 4000000000u + 1000000000u;\\nlong long c = 2000000000LL * 2000000000LL;\\nint main(void){return 0;}\\n' > {W}/cok.c && "
		 "{ {MCC} -B{B} -I{I} -pedantic-errors -c {W}/co.c -o {W}/co.o 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -pedantic -c {W}/cok.c -o {W}/cok.o 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'integer overflow in constant expression|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 integer overflow in constant expression\n"},

		{"line_macro_arg", "",
		 "printf '#define ID(x) x\\nint a=ID(\\n__LINE__\\n);\\nint main(void){return a;}\\n' > {W}/lma.c && "
		 "{MCC} -B{B} -I{I} {W}/lma.c -o {W}/lma && {W}/lma; echo L=$?",
		 "L=3\n"},

		{"u8_string_concat", "",
		 "printf 'const void*p=L\"a\" u8\"b\";\\n' > {W}/u8c.c && "
		 "printf 'char a[]=u8\"hi\"; const char*b=u8\"x\" \"y\"; const char*c=\"p\" u8\"q\";\\n"
		 "int main(void){return sizeof(a)+(a[0]!=0x68)+(b[0]!=0x78)+(c[1]!=0x71);}\\n' > {W}/u8ok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/u8c.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Werror {W}/u8ok.c -o {W}/u8ok 2>&1 && {W}/u8ok; echo RUN=$?; } | "
		 "grep -oE 'concatenation of string literals|RUN=3'",
		 "concatenation of string literals\nRUN=3\n"},

		{"pointer_to_vla_param", "",
		 "printf '#include <stddef.h>\\nvoid f(int m,int(*a)[m]){a[1][0]=42;}\\n"
		 "int main(void){int b[3][4]={0}; f(4,b); size_t s=sizeof(b[0]); return (b[1][0]==42 && s==16)?7:0;}\\n' > {W}/pvp.c && "
		 "{MCC} -B{B} -I{I} {W}/pvp.c -o {W}/pvp && {W}/pvp; echo R=$?",
		 "R=7\n"},

		{"conditional_ice", "",
		 "printf 'int v; enum E{Z=1?3:v};\\n' > {W}/cic.c && "
		 "printf 'enum F{A=1?3:4,B=1?3:(int)sizeof(int)};\\nint main(void){return A+B;}\\n' > {W}/cicok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/cic.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -pedantic-errors -c {W}/cicok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'not an integer constant expression|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 not an integer constant expression\n"},

		{"designated_init_continuation", "",
		 "printf 'struct O{int in[2];int t;};\\nstruct N{struct{int a,b;}s;int t;};\\nstruct F{int f1,f2;int fa[3];};\\n"
		 "int main(void){\\n"
		 "struct O o={.in[0]=1,2,3};\\n"
		 "struct N n={.s.a=1,2,3};\\n"
		 "struct F f={.f2=2,3,.f1=1,.fa[0]=10,.fa[1]=11,.fa[2]=12};\\n"
		 "int ok=o.in[0]==1&&o.in[1]==2&&o.t==3 && n.s.a==1&&n.s.b==2&&n.t==3 "
		 "&& f.f1==1&&f.f2==2&&f.fa[0]==10&&f.fa[1]==11&&f.fa[2]==12;\\n"
		 "return ok?9:0;}\\n' > {W}/dic.c && "
		 "{MCC} -B{B} -I{I} {W}/dic.c -o {W}/dic && {W}/dic; echo R=$?",
		 "R=9\n"},

		{"generic_atomic_restrict_constraints", "",
		 "printf 'struct S; int x=_Generic(1,struct S:1,int:2);\\n' > {W}/gi.c && "
		 "printf 'int x=_Generic(1,void:1,int:2);\\n' > {W}/gv.c && "
		 "printf '_Atomic(const int) x;\\n' > {W}/ac.c && "
		 "printf 'int restrict a[10];\\n' > {W}/ra.c && "
		 "printf 'int x=_Generic(1,long:1,int:2); _Atomic int y; int*restrict p;\\nint main(void){return 0;}\\n' > {W}/gok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/gi.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/gv.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/ac.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/ra.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -pedantic-errors -c {W}/gok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE '_Generic. association with an incomplete|_Atomic cannot be applied to a qualified|requires a pointer type|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 _Atomic cannot be applied to a qualified\n2 _Generic' association with an incomplete\n1 requires a pointer type\n"},

		{"param_and_blockfn_storage", "",
		 "printf 'void f(static int x);\\n' > {W}/ps.c && "
		 "printf 'void g(void){ register int h(void); (void)h; }\\n' > {W}/bf.c && "
		 "printf 'void f(register int x){(void)x;}\\nvoid g(void){ extern int h(void); (void)h; }\\nint main(void){return 0;}\\n' > {W}/sok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/ps.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/bf.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/sok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'storage class specified for parameter|invalid storage class for function|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 invalid storage class for function\n1 storage class specified for parameter\n"},

		{"expr_constraints", "",
		 "printf 'void f(int c){ c?(void)0:1; }\\n' > {W}/cv.c && "
		 "printf 'void*f(void){return (void*)2.5;}\\n' > {W}/fp.c && "
		 "printf 'double d=(double)(void*)0;\\n' > {W}/pf.c && "
		 "printf 'struct S{const int x;int y;}; void f(void){struct S a={1,2},b={3,4}; a=b;(void)a;}\\n' > {W}/cm.c && "
		 "printf 'int f(int c){return c?1:2;} void*g(void){return (void*)5;}\\nstruct T{int x;}; void h(void){struct T a={1},b={2}; a=b;(void)a;}\\nint main(void){return 0;}\\n' > {W}/eok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/cv.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/fp.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/pf.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/cm.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -pedantic-errors -c {W}/eok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'only one void operand|cast between a floating type and a pointer|assignment of read-only|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 assignment of read-only\n2 cast between a floating type and a pointer\n1 only one void operand\n"},

		{"ice_float_constraints", "",
		 "printf 'struct S{int b:(int)(2.5*2);};\\n' > {W}/ib.c && "
		 "printf 'enum E{X=(int)(2.5*2)};\\n' > {W}/ie.c && "
		 "printf 'int f(int x){switch(x){case (int)(2.5*2): return 1;} return 0;}\\n' > {W}/ic.c && "
		 "printf 'struct S{int b:(int)2.5;}; enum E{X=(int)3.0,Y=5}; int f(int x){switch(x){case (int)3.9:return 1;}return 0;}\\nint main(void){return 0;}\\n' > {W}/iok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/ib.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/ie.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/ic.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -pedantic-errors -c {W}/iok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'bit-field width that is not|enumerator value that is not|case label that is not|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 bit-field width that is not\n1 case label that is not\n1 enumerator value that is not\n"},

		{"static_init_and_ucn", "",
		 "printf 'void f(void){ static int *p=(int[]){10,20,30}; (void)p; }\\n' > {W}/sc.c && "
		 "printf 'char *s=\"\\\\u0041\";\\n' > {W}/uc.c && "
		 "printf 'int*p=(int[]){1,2,3}; void g(void){int*q=(int[]){4,5}; (void)q;}\\nint main(void){return 0;}\\n' > {W}/nok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/sc.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/uc.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -pedantic-errors -c {W}/nok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'initializer element is not constant|not a valid universal|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 initializer element is not constant\n1 not a valid universal\n"},

		{"int64_c_type", "",
		 "printf '#include <stdint.h>\\n"
		 "_Static_assert(_Generic(INT64_C(1), int_least64_t:1, default:0)==1, \"t1\");\\n"
		 "_Static_assert(_Generic(UINT64_C(1), uint_least64_t:1, default:0)==1, \"t2\");\\n"
		 "int main(void){return 0;}\\n' > {W}/i64.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/i64.c -o /dev/null 2>&1 && echo HOSTED_OK; "
		 "{MCC} -B{B} -I{I} -std=c11 -ffreestanding -nostdinc -Werror -c {W}/i64.c -o /dev/null 2>&1 && echo FREESTANDING_OK; } | sort",
		 "FREESTANDING_OK\nHOSTED_OK\n"},

		{"bool_bitfield_packing", "",
		 "printf '#include <stdio.h>\\n"
		 "struct B{_Bool a:1;_Bool b:1;};\\n"
		 "struct C{_Bool a:1,b:1,c:1,d:1;};\\n"
		 "int main(void){ struct B v={1,1}; printf(\"%%zu %%zu %%d%%d\\\\n\",sizeof(struct B),sizeof(struct C),v.a,v.b); return 0; }\\n' > {W}/bfp.c && "
		 "{MCC} -B{B} -I{I} {W}/bfp.c -o {W}/bfp && {W}/bfp",
		 "1 1 11\n"},

		{"complex_real_precision", "",
		 "printf '#include <complex.h>\\n#include <stdio.h>\\n"
		 "int main(void){ volatile double r=0.1; double _Complex z=r*I; printf(\"%%.17g\\\\n\", cimag(z)); return 0; }\\n' > {W}/cxp.c && "
		 "{MCC} -B{B} -I{I} {W}/cxp.c -lm -o {W}/cxp && {W}/cxp",
		 "0.10000000000000001\n"},

		{"pedantic_extension_diagnostics", "",
		 "printf 'int x=0b1010;\\n' > {W}/pb.c && "
		 "printf 'char *s=\"\\\\e[0m\";\\n' > {W}/pe.c && "
		 "printf 'int a[0];\\n' > {W}/pa.c && "
		 "printf 'int x=0x1f,y=42; const char*s=\"\\\\n\\\\t\"; int arr[3]; struct S{int n;int d[];};\\nint main(void){return 0;}\\n' > {W}/pok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/pb.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/pe.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/pa.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -pedantic-errors -c {W}/pok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'binary integer constants|non-ISO escape sequence|forbids zero-size array|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 binary integer constants\n1 forbids zero-size array\n1 non-ISO escape sequence\n"},

		{"c11_features_in_c99", "",
		 "printf 'const void*p=u\"hi\";\\n' > {W}/cu.c && "
		 "printf 'static _Thread_local int x;\\n' > {W}/ct.c && "
		 "printf '_Atomic(int) a;\\n' > {W}/ca.c && "
		 "printf 'const void*p=u\"hi\"; static _Thread_local int x; _Atomic(int) a; const void*q=L\"w\";\\nint main(void){return 0;}\\n' > {W}/cok.c && "
		 "{ {MCC} -fno-diagnostics-show-caret -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/cu.c -o /dev/null 2>&1; "
		 "{MCC} -fno-diagnostics-show-caret -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/ct.c -o /dev/null 2>&1; "
		 "{MCC} -fno-diagnostics-show-caret -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/ca.c -o /dev/null 2>&1; "
		 "{MCC} -fno-diagnostics-show-caret -B{B} -I{I} -std=c11 -Werror -c {W}/cok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'character/string prefix|_Thread_local|_Atomic|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 _Atomic\n1 _Thread_local\n1 character/string prefix\n"},

		{"pragma_vla_typedef_constraints", "",
		 "printf 'void f(void){ _Pragma(foo); }\\n' > {W}/pn.c && "
		 "printf 'void f(int n){ typedef int T[n]; typedef int T[n]; (void)sizeof(T[0]); }\\n' > {W}/tv.c && "
		 "printf 'void g(void){ _Pragma(\"pack(1)\"); }\\ntypedef int U; typedef int U; U z;\\nint main(void){return 0;}\\n' > {W}/pvok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/pn.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/tv.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/pvok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE '_Pragma takes a parenthesized|redefinition of variably modified|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 _Pragma takes a parenthesized\n1 redefinition of variably modified\n"},

		{"kr_identifier_list_declaration", "",
		 "printf 'int f(a,b);\\nint main(void){return 0;}\\n' > {W}/kr_bad.c && "
		 "printf 'int def(a,b) int a,b; { return a+b; }\\nint proto();\\nint main(void){return def(1,2)+proto();}\\n' > {W}/kr_ok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/kr_bad.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Wno-old-style-definition -Werror -c {W}/kr_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'parameter names \\(without types\\)|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 parameter names (without types)\n"},

		{"function_definition_complete_types", "",
		 "printf 'struct S; struct S g(void){ }\\n' > {W}/ic_ret.c && "
		 "printf 'struct S; int h(struct S s){ return 0; }\\n' > {W}/ic_par.c && "
		 "printf 'struct C{int x;}; struct C cc(struct C a){return a;}\\nstruct S; struct S *p(void){return 0;} int q(struct S *s){return !!s;}\\nvoid v(void){}\\nint main(void){return 0;}\\n' > {W}/ic_ok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/ic_ret.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/ic_par.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/ic_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'return type is an incomplete|has incomplete type|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 has incomplete type\n1 return type is an incomplete\n"},

		{"typedef_ordinary_name_space", "",
		 "printf 'typedef int T; int T;\\n' > {W}/ns_obj.c && "
		 "printf 'typedef int T; int T(void){return 0;}\\n' > {W}/ns_fn.c && "
		 "printf 'typedef int T; void f(void){ int T; T=1; (void)T; }\\nextern int x; int x=5;\\nint main(void){return x;}\\n' > {W}/ns_ok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/ns_obj.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/ns_fn.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/ns_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE \"redeclared as different kind|redefinition of 'T'|CLEAN_OK\" | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 redeclared as different kind\n1 redefinition of 'T'\n"},

		{"function_definition_typedef_type", "",
		 "printf 'typedef int F(void); F f { return 0; }\\n' > {W}/tdf_bad.c && "
		 "printf 'typedef int T; T h(void){ return 0; }\\nint def(a,b) int a,b; { return a+b; }\\ntypedef int F(void); F *fp;\\nint main(void){return h()+def(1,2)+(fp!=0);}\\n' > {W}/tdf_ok.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/tdf_bad.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Wno-old-style-definition -Werror -c {W}/tdf_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE \"declared with a typedef'd function type|CLEAN_OK\" | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 declared with a typedef'd function type\n"},

		{"imaginary_integer_constants", "",
		 "printf '#include <complex.h>\\n#include <stdio.h>\\nint main(void){ double complex z = 2 + 3i; long double complex w = 4 + 5j; printf(\"%%g %%g %%Lg %%Lg\\\\n\", creal(z), cimag(z), creall(w), cimagl(w)); return 0; }\\n' > {W}/imag_rt.c && "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/imag_rt.c -o {W}/imag_rt && {W}/imag_rt",
		 "2 3 4 5\n"},

		{"tgmath_nexttoward_first_arg",
		 "os!=WIN32:PE has long double==double (8), so nexttoward(long double) is 8 not 16",
		 "printf '#include <tgmath.h>\\n#include <stdio.h>\\nint main(void){ float f=1; double d=1; long double l=1; printf(\"%%d %%d %%d\\\\n\", (int)(sizeof(nexttoward(f,2.0L))==sizeof(f)), (int)(sizeof(nexttoward(d,2.0L))==sizeof(d)), (int)(sizeof(nexttoward(l,2.0L))==sizeof(l))); return 0; }\\n' > {W}/ntg.c && "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/ntg.c -o {W}/ntg && {W}/ntg",
		 "1 1 1\n"},

		{"ucn_identifier_initial_combining", "",
		 "printf 'int \\\\u0300x;\\n' > {W}/ucn_bad.c && "
		 "printf 'int a\\\\u0300b = 5;\\nint \\\\u00C0v = 7;\\nint main(void){ return a\\\\u0300b + \\\\u00C0v; }\\n' > {W}/ucn_ok.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/ucn_bad.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/ucn_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE 'not valid as the first character|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 not valid as the first character\n"},

		{"complex_const_init_overflow", "os!=WIN32:msvcrt prints infinity as \"1.#INF\", not glibc's \"inf\"",
		 "printf '#include <complex.h>\\n#include <stdio.h>\\ndouble complex gz = 4.0e38f + 0.0*I;\\nint main(void){ double complex lz = 4.0e38f + 0.0*I; double complex lf = 2.0 + 3.0*I; printf(\"%%g %%g %%g %%g\\\\n\", creal(gz), creal(lz), creal(lf), cimag(lf)); return 0; }\\n' > {W}/imc.c && "
		 "{MCC} -B{B} -I{I} -std=c11 {W}/imc.c -lm -o {W}/imc && {W}/imc",
		 "inf inf 2 3\n"},

		{"wformat_printf_scanf_checking", "",
		 "printf '#include <stdio.h>\\nint main(void){ printf(\"%%d\\\\n\",\"x\"); printf(\"%%s\\\\n\",1); printf(\"%%f\\\\n\",2); printf(\"%%d %%d\\\\n\",1); return 0; }\\n' > {W}/wf_bad.c && "
		 "printf '#include <stdio.h>\\nint main(void){ char b[8]; int x; printf(\"%%d %%s %%f %%c %%p %%%%\\\\n\",1,\"x\",2.0,(int)0x61,(void*)0); printf(\"%%*.*f\\\\n\",4,2,3.14); scanf(\"%%d %%7s\",&x,b); return x; }\\n' > {W}/wf_ok.c && "
		 "{ {MCC} -B{B} -I{I} -Wformat -c {W}/wf_bad.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -Wformat -Werror -c {W}/wf_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; "
		 "{MCC} -B{B} -I{I} -c {W}/wf_bad.c -o /dev/null 2>&1 && echo SILENT_DEFAULT; } | "
		 "grep -oE 'expects an integer argument|expects a pointer argument|expects a floating argument|more conversions than arguments|CLEAN_OK|SILENT_DEFAULT' | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 SILENT_DEFAULT\n1 expects a floating argument\n1 expects a pointer argument\n1 expects an integer argument\n1 more conversions than arguments\n"},

		{"wformat_length_modifier_width", "",
		 "printf '#include <stdio.h>\\nint main(void){ int i=1; long long ll=1; printf(\"%%lld\\\\n\", i); printf(\"%%d\\\\n\", ll); return 0; }\\n' > {W}/wfw_bad.c && "
		 "printf '#include <stdio.h>\\nint main(void){ long long ll=1; int i=1; printf(\"%%lld %%d\\\\n\", ll, i); return (int)ll+i; }\\n' > {W}/wfw_ok.c && "
		 "{ {MCC} -B{B} -I{I} -Wformat -c {W}/wfw_bad.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -Wformat -Werror -c {W}/wfw_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; "
		 "{MCC} -B{B} -I{I} -c {W}/wfw_bad.c -o /dev/null 2>&1 && echo SILENT_DEFAULT; } | "
		 "grep -oE \"expects argument of type 'long long'|expects argument of type 'int'|CLEAN_OK|SILENT_DEFAULT\" | LC_ALL=C sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 SILENT_DEFAULT\n1 expects argument of type 'int'\n1 expects argument of type 'long long'\n"},

		{"wparentheses_bitwise_vs_comparison", "",
		 "printf 'int f(int a,int b,int c){ return (a & b==c) + (a==b & c); }\\n' > {W}/wpp_bad.c && "
		 "printf 'int g(int a,int b,int c){ return ((a&b)==c) + (a & (b==c)); }\\n' > {W}/wpp_ok.c && "
		 "{ {MCC} -B{B} -I{I} -Wparentheses -c {W}/wpp_bad.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -Wparentheses -Werror -c {W}/wpp_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; "
		 "{MCC} -B{B} -I{I} -c {W}/wpp_bad.c -o /dev/null 2>&1 && echo SILENT_DEFAULT; } | "
		 "grep -oE \"operand of '&'|CLEAN_OK|SILENT_DEFAULT\" | LC_ALL=C sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 SILENT_DEFAULT\n2 operand of '&'\n"},

		{"wsign_compare_constant_operand", "",
		 "printf 'int f(int i,unsigned u){int r=0;\\n r+=(i<5U);\\n r+=(u==-1);\\n return r;}\\n' > {W}/sc_bad.c && "
		 /* -1LL (not -1L): a *wider* signed constant is value-preserving vs
		  * unsigned int on BOTH LP64 and LLP64. On win (LLP64) `long` is 32-bit,
		  * so `u==-1L` is a genuine same-rank signed/unsigned mismatch that
		  * gcc-16 AND mcc correctly warn on -- `-1LL` keeps this "wider signed
		  * type" clean case portable across ABIs. */
		 "printf 'int g(int i,unsigned u){int r=0;\\n r+=(i<0U);\\n r+=(u==5);\\n r+=(i>=0U);\\n r+=(u==-1LL);\\n return r;}\\n' > {W}/sc_ok.c && "
		 "{ {MCC} -B{B} -I{I} -Wsign-compare -c {W}/sc_bad.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -Wsign-compare -Werror -c {W}/sc_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; "
		 "{MCC} -B{B} -I{I} -c {W}/sc_bad.c -o /dev/null 2>&1 && echo SILENT_DEFAULT; } | "
		 "grep -oE \"different signedness|CLEAN_OK|SILENT_DEFAULT\" | LC_ALL=C sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 SILENT_DEFAULT\n2 different signedness\n"},

		{"arm64_disasm_adr_pc_relative", "cpu=arm64",
		 "printf '.text\\n.globl _f\\n_f:\\n nop\\n nop\\n adr x0, _f\\n ret\\n' > {W}/adrt.s && "
		 "clang -c {W}/adrt.s -o {W}/adrt.o 2>/dev/null && "
		 "{MCC} -B{B} -S {W}/adrt.o -o {W}/adrt.dis.s 2>/dev/null && "
		 "grep -oE 'adr[[:space:]]+x0, 0x[0-9a-f]+' {W}/adrt.dis.s | sed 's/[[:space:]]\\+/ /g'",
		 "adr x0, 0x0\n"},

		{"wpedantic_alias", "",
		 "printf 'int x = 0o5;\\nint main(void){return x;}\\n' > {W}/wp.c && "
		 "{ {MCC} -B{B} -I{I} -Wpedantic -c {W}/wp.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -Wpedantic -Wno-pedantic -Werror -c {W}/wp.c -o /dev/null 2>&1 && echo OFF_OK; "
		 "{MCC} -B{B} -I{I} -Wpedantic -pedantic-errors -c {W}/wp.c -o /dev/null 2>&1; } | "
		 "grep -oE 'C2Y feature or GCC extension|OFF_OK' | sort | uniq -c | sed 's/^ *//'",
		 "2 C2Y feature or GCC extension\n1 OFF_OK\n"},

		{"attr_deprecated_unavailable", "",
		 "printf '__attribute__((deprecated)) void od(void);\\n"
		 "__attribute__((deprecated(\"m\"))) extern int ox;\\n"
		 "__attribute__((unavailable)) void ou(void);\\n"
		 "void f(void){ od(); (void)ox; }\\n' > {W}/dep.c && "
		 "printf '__attribute__((unavailable)) void ou(void);\\n"
		 "void g(void){ ou(); }\\n' > {W}/unav.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/dep.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -Wno-deprecated-declarations -Werror -c {W}/dep.c -o /dev/null 2>&1 && echo OFF_OK; "
		 "{MCC} -B{B} -I{I} -c {W}/unav.c -o /dev/null 2>&1; } | "
		 "grep -oE \"is deprecated|is unavailable|OFF_OK\" | sort | uniq -c | sed 's/^ *//'",
		 "1 OFF_OK\n2 is deprecated\n1 is unavailable\n"},

		{"malformed_pragma_is_not_fatal", "",
		 "printf '#pragma pack(show)\\n#pragma pack(3)\\n#pragma options align=mac68k\\n"
		 "struct s { char a; int b; };\\n"
		 "int main(void){ return sizeof(struct s) == 8 ? 0 : 1; }\\n' > {W}/pg.c && "
		 "{ {MCC} -B{B} -I{I} -Wall -o {W}/pg {W}/pg.c 2>&1 && {W}/pg && echo RUN_OK; "
		 "{MCC} -B{B} -I{I} -Wno-unknown-pragmas -Werror -c {W}/pg.c -o /dev/null 2>&1 && echo QUIET_OK; } | "
		 "grep -oE 'ignored|RUN_OK|QUIET_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 QUIET_OK\n1 RUN_OK\n3 ignored\n"},

		{"unknown_w_option_is_not_fatal", "",
		 "printf 'int main(void){return 0;}\\n' > {W}/uw.c && "
		 "{ {MCC} -B{B} -I{I} -Werror -Wno-error=made-up-warning -c {W}/uw.c -o /dev/null 2>&1 && echo WERROR_OK; "
		 "{MCC} -B{B} -I{I} -Werror -Wno-made-up-warning -c {W}/uw.c -o /dev/null 2>&1 && echo WNO_OK; "
		 "{MCC} -B{B} -I{I} -Wno-unsupported-option -Werror -c {W}/uw.c -o /dev/null 2>&1 && echo QUIET_OK; } | "
		 "grep -oE \"unsupported option|WERROR_OK|WNO_OK|QUIET_OK\" | sort | uniq -c | sed 's/^ *//'",
		 "1 QUIET_OK\n1 WERROR_OK\n1 WNO_OK\n2 unsupported option\n"},

		{"attr_declaration_appertains_to_nothing", "",
		 "printf '[[deprecated]];\\nint main(void){return 0;}\\n' > {W}/ad.c && "
		 "printf '[[gnu::const]];\\nint main(void){return 0;}\\n' > {W}/av.c && "
		 "{ {MCC} -B{B} -I{I} -std=c23 -pedantic-errors -c {W}/ad.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c23 -pedantic-errors -c {W}/av.c -o /dev/null 2>&1 && echo VENDOR_OK; } | "
		 "grep -oE \"attribute ignored|VENDOR_OK\" | sort | uniq -c | sed 's/^ *//'",
		 "1 VENDOR_OK\n2 attribute ignored\n"},

		{"typeof_is_not_a_keyword_when_std_says_so", "",
		 "printf 'int typeof = 1;\\nlong typeof_unqual = 2;\\nint main(void){return typeof + (int)typeof_unqual - 3;}\\n' > {W}/tq.c && "
		 "{ {MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/tq.c -o /dev/null 2>&1 && echo C11_OK; "
		 "{MCC} -B{B} -I{I} -std=gnu11 -fno-asm -c {W}/tq.c -o /dev/null 2>&1 && echo NOASM_OK; "
		 "{MCC} -B{B} -I{I} -std=gnu11 -c {W}/tq.c -o /dev/null 2>&1 || echo GNU_KEYWORD; "
		 "{MCC} -B{B} -I{I} -std=c23 -c {W}/tq.c -o /dev/null 2>&1 || echo C23_KEYWORD; } | "
		 "grep -oE 'C11_OK|NOASM_OK|GNU_KEYWORD|C23_KEYWORD' | sort | uniq -c | sed 's/^ *//'",
		 "1 C11_OK\n1 C23_KEYWORD\n1 GNU_KEYWORD\n1 NOASM_OK\n"},

		{"fsyntax_only_no_output", "",
		 "printf 'int main(void){return 0;}\\n' > {W}/so_ok.c && "
		 "printf 'int main(void){ int 3x; }\\n' > {W}/so_bad.c && "
		 "rm -f {W}/so_out.o && "
		 "{ {MCC} -B{B} -I{I} -fsyntax-only -c {W}/so_ok.c -o {W}/so_out.o 2>&1 && "
		 "{ [ -f {W}/so_out.o ] && echo HAS_OUTPUT || echo NO_OUTPUT; }; "
		 "{MCC} -B{B} -I{I} -fsyntax-only -c {W}/so_bad.c -o /dev/null >/dev/null 2>&1 && echo BAD_OK || echo BAD_REJECTED; }",
		 "NO_OUTPUT\nBAD_REJECTED\n"},

		{"deps_target_MT_MQ", "",
		 "printf 'int main(void){return 0;}\\n' > {W}/dep.c && "
		 "{ {MCC} -B{B} -I{I} -M -MT custom.o {W}/dep.c 2>&1 | head -1; "
		 "{MCC} -B{B} -I{I} -M -MQ 'x/$(N).o' {W}/dep.c 2>&1 | head -1; "
		 "{MCC} -B{B} -I{I} -M -MT a.o -MT b.o {W}/dep.c 2>&1 | head -1; } | sed 's/ .$//'",
		 "custom.o:\nx/$$(N).o:\na.o b.o:\n"},

		{"iquote_idirafter_paths", "",
		 "rm -rf {W}/t_iq {W}/t_aft && mkdir -p {W}/t_iq {W}/t_aft && "
		 "printf 'int q=1;\\n' > {W}/t_iq/qh.h && printf 'int a=1;\\n' > {W}/t_aft/ah.h && "
		 "printf '#include \"qh.h\"\\nint main(void){return q;}\\n' > {W}/c_q.c && "
		 "printf '#include <qh.h>\\nint main(void){return q;}\\n' > {W}/c_qa.c && "
		 "printf '#include <ah.h>\\nint main(void){return a;}\\n' > {W}/c_a.c && "
		 "{ {MCC} -B{B} -I{I} -iquote {W}/t_iq -c {W}/c_q.c -o /dev/null 2>&1 && echo IQUOTE_QUOTE_OK; "
		 "{MCC} -B{B} -I{I} -iquote {W}/t_iq -c {W}/c_qa.c -o /dev/null >/dev/null 2>&1 && echo IQUOTE_ANGLE_FOUND || echo IQUOTE_ANGLE_SKIPPED; "
		 "{MCC} -B{B} -I{I} -idirafter {W}/t_aft -c {W}/c_a.c -o /dev/null 2>&1 && echo IDIRAFTER_OK; }",
		 "IQUOTE_QUOTE_OK\nIQUOTE_ANGLE_SKIPPED\nIDIRAFTER_OK\n"},

		{"wvla_variable_length_array", "",
		 "printf 'int f(int n){ int a[n]; return a[0]; }\\nint main(void){return f(3);}\\n' > {W}/vla.c && "
		 "printf 'int main(void){ int a[5]; return a[0]; }\\n' > {W}/fixed.c && "
		 "{ {MCC} -B{B} -I{I} -Wvla -c {W}/vla.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -Wvla -Werror -c {W}/fixed.c -o /dev/null 2>&1 && echo FIXED_CLEAN; "
		 "{MCC} -B{B} -I{I} -c {W}/vla.c -o /dev/null 2>&1 && echo DEFAULT_SILENT; } | "
		 "grep -oE 'forbids variable length array|FIXED_CLEAN|DEFAULT_SILENT' | sort | uniq -c | sed 's/^ *//'",
		 "1 DEFAULT_SILENT\n1 FIXED_CLEAN\n1 forbids variable length array\n"},

		{"wundef_if_undefined", "",
		 "printf '#if FOO\\n#endif\\n#define BAR 1\\n#if BAR\\n#endif\\nint main(void){return 0;}\\n' > {W}/u.c && "
		 "{ {MCC} -B{B} -I{I} -Wundef -c {W}/u.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -c {W}/u.c -o /dev/null 2>&1 && echo DEFAULT_SILENT; } | "
		 "grep -oE 'is not defined, evaluates to 0|DEFAULT_SILENT' | sort | uniq -c | sed 's/^ *//'",
		 "1 DEFAULT_SILENT\n1 is not defined, evaluates to 0\n"},

		{"wunknown_pragmas", "",
		 "printf '#pragma frobnicate\\nint main(void){return 0;}\\n' > {W}/p.c && "
		 "{ {MCC} -B{B} -I{I} -Wunknown-pragmas -c {W}/p.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -Wall -Wno-unknown-pragmas -Werror -c {W}/p.c -o /dev/null 2>&1 && echo OFF_OK; "
		 "{MCC} -B{B} -I{I} -c {W}/p.c -o /dev/null 2>&1 && echo DEFAULT_SILENT; } | "
		 "grep -oE 'frobnicate ignored|OFF_OK|DEFAULT_SILENT' | sort | uniq -c | sed 's/^ *//'",
		 "1 DEFAULT_SILENT\n1 OFF_OK\n1 frobnicate ignored\n"},

		{"pragma_pack_unknown_action", "",
		 "printf '#pragma pack(garbage\\nint main(void){return 0;}\\n' > {W}/ppa.c && "
		 "{ {MCC} -B{B} -I{I} -Wall -c {W}/ppa.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -Wall -Wno-unknown-pragmas -Werror -c {W}/ppa.c -o /dev/null 2>&1 && echo OFF_OK; "
		 "{MCC} -B{B} -I{I} -c {W}/ppa.c -o /dev/null 2>&1 && echo DEFAULT_SILENT; } | "
		 "grep -oE \"unknown action 'garbage' for '#pragma pack' - ignored|OFF_OK|DEFAULT_SILENT\" | sort | uniq -c | sed 's/^ *//'",
		 "1 DEFAULT_SILENT\n1 OFF_OK\n1 unknown action 'garbage' for '#pragma pack' - ignored\n"},

		{"wimplicit_int_flag", "",
		 "printf 'foo(void){ return 0; }\\nint main(void){return foo();}\\n' > {W}/ii.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/ii.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -Wno-implicit-int -Werror -c {W}/ii.c -o /dev/null 2>&1 && echo OFF_OK; } | "
		 "grep -oE \"return type defaults to 'int'|OFF_OK\" | sort | uniq -c | sed 's/^ *//'",
		 "1 OFF_OK\n1 return type defaults to 'int'\n"},

		{"wsign_compare", "",
		 "printf 'int bad(int a, unsigned b){ return a < b; }\\n' > {W}/sc_bad.c && "
		 "printf 'int ok(unsigned u, int a, int b){ return (u < 5) + (u == 0) + (a < b); }\\n' > {W}/sc_ok.c && "
		 "{ {MCC} -B{B} -I{I} -Wsign-compare -c {W}/sc_bad.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -Wsign-compare -Werror -c {W}/sc_ok.c -o /dev/null 2>&1 && echo OK_CLEAN; "
		 "{MCC} -B{B} -I{I} -c {W}/sc_bad.c -o /dev/null 2>&1 && echo DEFAULT_SILENT; } | "
		 "grep -oE 'different signedness|OK_CLEAN|DEFAULT_SILENT' | sort | uniq -c | sed 's/^ *//'",
		 "1 DEFAULT_SILENT\n1 OK_CLEAN\n1 different signedness\n"},

		{"wextra_umbrella", "",
		 "printf 'int bad(int a, unsigned b){ return a < b; }\\n' > {W}/sc.c && "
		 "{ {MCC} -B{B} -I{I} -Wextra -c {W}/sc.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -Wextra -Wno-sign-compare -Werror -c {W}/sc.c -o /dev/null 2>&1 && echo MEMBER_OFF_OK; "
		 "{MCC} -B{B} -I{I} -Wno-extra -Werror -c {W}/sc.c -o /dev/null 2>&1 && echo NOEXTRA_OK; } | "
		 "grep -oE 'different signedness|MEMBER_OFF_OK|NOEXTRA_OK' | sort | uniq -c | sed 's/^ *//'",
		 "1 MEMBER_OFF_OK\n1 NOEXTRA_OK\n1 different signedness\n"},

		{"wparentheses_assignment", "",
		 "printf 'int g(void){return 0;}\\nint main(void){int x; if (x = 1){} while(x = g()){} return x;}\\n' > {W}/pp_bad.c && "
		 "printf 'int main(void){int x=0; if ((x = 1)){} if (x == 1){} for(x=0;x<2;x++){} return x;}\\n' > {W}/pp_ok.c && "
		 "echo \"$({MCC} -B{B} -I{I} -Wparentheses -c {W}/pp_bad.c -o /dev/null 2>&1 | grep -c 'truth value') warns\"; "
		 "{MCC} -B{B} -I{I} -Wall -Wno-parentheses -Werror -c {W}/pp_bad.c -o /dev/null 2>&1 && echo OFF_OK; "
		 "{MCC} -B{B} -I{I} -Wparentheses -Werror -c {W}/pp_ok.c -o /dev/null 2>&1 && echo OK_CLEAN; "
		 "{MCC} -B{B} -I{I} -c {W}/pp_bad.c -o /dev/null 2>&1 && echo DEFAULT_SILENT",
		 "2 warns\nOFF_OK\nOK_CLEAN\nDEFAULT_SILENT\n"},

		{"wswitch_enum", "",
		 "printf 'enum E{A,B,C}; int f(enum E e){switch(e){case A:return 1;case C:return 3;} return 0;}\\n' > {W}/sw_bad.c && "
		 "printf 'enum E{A,B}; int g(enum E e){switch(e){case A:return 1;case B:return 2;} return 0;}\\nint h(enum E e){switch(e){case A:return 1;default:return 0;}}\\n' > {W}/sw_ok.c && "
		 "echo \"$({MCC} -B{B} -I{I} -Wswitch -c {W}/sw_bad.c -o /dev/null 2>&1 | grep -c 'not handled') warn\"; "
		 "{MCC} -B{B} -I{I} -Wswitch -Werror -c {W}/sw_ok.c -o /dev/null 2>&1 && echo OK_CLEAN; "
		 "{MCC} -B{B} -I{I} -c {W}/sw_bad.c -o /dev/null 2>&1 && echo DEFAULT_SILENT",
		 "1 warn\nOK_CLEAN\nDEFAULT_SILENT\n"},

		{"wall_enables_format", "",
		 "printf '#include <stdio.h>\\nint main(void){ printf(\"%%d\\\\n\", \"x\"); return 0; }\\n' > {W}/f.c && "
		 "echo \"$({MCC} -B{B} -I{I} -Wall -c {W}/f.c -o /dev/null 2>&1 | grep -c 'expects an integer') warn\"; "
		 "{MCC} -B{B} -I{I} -Wall -Wno-format -Werror -c {W}/f.c -o /dev/null 2>&1 && echo NOFORMAT_OK",
		 "1 warn\nNOFORMAT_OK\n"},

		{"wunused_variable", "",
		 "printf 'int main(void){ int unused; int used=5; return used; }\\n' > {W}/uv.c && "
		 "printf 'void g(int*); int ok(void){ int x; g(&x); int y=1; return y; }\\n' > {W}/uv_ok.c && "
		 "echo \"$({MCC} -B{B} -I{I} -Wunused-variable -c {W}/uv.c -o /dev/null 2>&1 | grep -c \"unused variable 'unused'\") warn\"; "
		 "{MCC} -B{B} -I{I} -Wunused-variable -Werror -c {W}/uv_ok.c -o /dev/null 2>&1 && echo OK_CLEAN; "
		 "{MCC} -B{B} -I{I} -c {W}/uv.c -o /dev/null 2>&1 && echo DEFAULT_SILENT",
		 "1 warn\nOK_CLEAN\nDEFAULT_SILENT\n"},

		{"wunused_parameter", "",
		 "printf 'int f(int a, int unusedp){ return a; }\\nint main(void){return f(1,2);}\\n' > {W}/up.c && "
		 "printf 'int g(int a, int b){ return a+b; }\\nint main(void){return g(1,2);}\\n' > {W}/up_ok.c && "
		 "echo \"$({MCC} -B{B} -I{I} -Wunused-parameter -c {W}/up.c -o /dev/null 2>&1 | grep -c \"unused parameter 'unusedp'\") warn\"; "
		 "{MCC} -B{B} -I{I} -Wextra -Werror -c {W}/up_ok.c -o /dev/null 2>&1 && echo OK_CLEAN; "
		 "{MCC} -B{B} -I{I} -c {W}/up.c -o /dev/null 2>&1 && echo DEFAULT_SILENT",
		 "1 warn\nOK_CLEAN\nDEFAULT_SILENT\n"},

		{"wunused_function", "",
		 "printf 'static int helper(void){return 1;}\\nint main(void){return 0;}\\n' > {W}/uf.c && "
		 "printf 'static int used(void){return 1;}\\nint main(void){return used();}\\n' > {W}/uf_ok.c && "
		 "echo \"$({MCC} -B{B} -I{I} -Wunused-function -c {W}/uf.c -o /dev/null 2>&1 | grep -c \"'helper' defined but not used\") warn\"; "
		 "{MCC} -B{B} -I{I} -Wunused-function -Werror -c {W}/uf_ok.c -o /dev/null 2>&1 && echo OK_CLEAN; "
		 "{MCC} -B{B} -I{I} -c {W}/uf.c -o /dev/null 2>&1 && echo DEFAULT_SILENT",
		 "1 warn\nOK_CLEAN\nDEFAULT_SILENT\n"},

		{"fatal_errors_and_max_errors", "",
		 "printf 'int main(void){ return 0; }\\n' > {W}/ok.c && "
		 "printf 'int main(void){ undefined_thing; return 0; }\\n' > {W}/bad.c && "
		 "{ {MCC} -B{B} -I{I} -Wfatal-errors -fmax-errors=3 -Werror -c {W}/ok.c -o /dev/null 2>&1 && echo CLEAN_ACCEPTED; "
		 "{MCC} -B{B} -I{I} -Wfatal-errors -c {W}/bad.c -o /dev/null >/dev/null 2>&1 || echo BAD_STOPS; }",
		 "CLEAN_ACCEPTED\nBAD_STOPS\n"},

		{"wshadow_declaration", "",
		 "printf 'int x;\\nvoid f(int p){ int x; { int p; (void)x; (void)p; } }\\n' > {W}/sh.c && "
		 "printf 'int x; void g(void){ int y; { extern int x; (void)x; } (void)y; }\\n' > {W}/sh_ok.c && "
		 "echo \"$({MCC} -B{B} -I{I} -Wshadow -c {W}/sh.c -o /dev/null 2>&1 | grep -c shadow) warn\"; "
		 "{MCC} -B{B} -I{I} -Wshadow -Werror -c {W}/sh_ok.c -o /dev/null 2>&1 && echo OK_CLEAN; "
		 "{MCC} -B{B} -I{I} -c {W}/sh.c -o /dev/null 2>&1 && echo DEFAULT_SILENT",
		 "2 warn\nOK_CLEAN\nDEFAULT_SILENT\n"},

		{"wunused_value", "",
		 "printf 'int g(void); void f(int a,int b){ a==b; g()+1; }\\n' > {W}/uvv.c && "
		 "printf 'int g(void); void f(int x){ x=1; g(); x++; (void)x; }\\n' > {W}/uvv_ok.c && "
		 "echo \"$({MCC} -B{B} -I{I} -Wunused-value -c {W}/uvv.c -o /dev/null 2>&1 | grep -c 'not used') warn\"; "
		 "{MCC} -B{B} -I{I} -Wunused-value -Werror -c {W}/uvv_ok.c -o /dev/null 2>&1 && echo OK_CLEAN; "
		 "{MCC} -B{B} -I{I} -c {W}/uvv.c -o /dev/null 2>&1 && echo DEFAULT_SILENT",
		 "2 warn\nOK_CLEAN\nDEFAULT_SILENT\n"},

		{"dash_S_emits_assembly", "os!=WIN32",
		 "printf 'int answer(void){return 42;}\\n' > {W}/t.c && "
		 "{MCC} -B{B} -I{I} -S {W}/t.c -o {W}/t.s && "
		 "grep -qE '^_?answer:' {W}/t.s && "
		 "grep -qE '[.]text' {W}/t.s && "
		 "grep -qE '_?answer, @function' {W}/t.s && echo S_OK",
		 "S_OK\n"},

		{"imacros_macro_header", "",
		 "printf '#define CFG 42\\n#define DBL(x) ((x)*2)\\n' > {W}/macros.h && "
		 "printf 'int main(void){ return CFG - DBL(21); }\\n' > {W}/imain.c && "
		 "{MCC} -B{B} -I{I} -imacros {W}/macros.h {W}/imain.c -o {W}/imbin && {W}/imbin && echo RAN_OK_EXIT0",
		 "RAN_OK_EXIT0\n"},

		{"wuninitialized", "",
		 "printf 'int f(void){ int x; return x; }\\nint g(void){ int y,z; z=y; return z; }\\n' > {W}/un.c && "
		 "printf 'int f(int c){ int x; if(c) x=1; else x=2; return x; }\\nint h(void){ int a=5; void g(int*); int b; g(&b); return a+b; }\\n' > {W}/un_ok.c && "
		 "echo \"$({MCC} -B{B} -I{I} -Wuninitialized -c {W}/un.c -o /dev/null 2>&1 | grep -c 'is used uninitialized') warn\"; "
		 "{MCC} -B{B} -I{I} -Wuninitialized -Werror -c {W}/un_ok.c -o /dev/null 2>&1 && echo OK_CLEAN; "
		 "{MCC} -B{B} -I{I} -c {W}/un.c -o /dev/null 2>&1 && echo DEFAULT_SILENT",
		 "2 warn\nOK_CLEAN\nDEFAULT_SILENT\n"},

		{"tss_dtor_iterations_ice", "",
		 "printf '#include <threads.h>\\n_Static_assert(TSS_DTOR_ITERATIONS>=1,\"ice\");\\nint a[TSS_DTOR_ITERATIONS];\\nint main(void){return (int)(sizeof a/sizeof a[0])==4 ? 0 : 1;}\\n' > {W}/tss.c && "
		 "{MCC} -B{B} -I{I} -c {W}/tss.c -o /dev/null 2>&1 && echo ICE_OK",
		 "ICE_OK\n"},

		{"tgmath_creal_cimag_precision", "",

		 "printf '#include <tgmath.h>\\n#include <stdio.h>\\nint main(void){ long double complex l=1; float complex f=1; double complex d=1; printf(\"%%d %%d %%d\\\\n\",(int)(sizeof(creal(l))==sizeof(long double)),(int)(sizeof(cimag(f))==sizeof(float)),(int)(sizeof(creal(d))==sizeof(double))); return 0; }\\n' > {W}/cgt.c && "
		 "{MCC} -B{B} -I{I} {W}/cgt.c -lm -o {W}/cgt && {W}/cgt",
		 "1 1 1\n"},

		{"ucn_identifier_range2", "",
		 "printf 'int a\\\\uFFFFb;\\n' > {W}/ucr_bad.c && "
		 "printf 'int a\\\\u00C0b=1; int c\\\\u2460d=2;\\nint main(void){return a\\\\u00C0b + c\\\\u2460d;}\\n' > {W}/ucr_ok.c && "
		 "{ {MCC} -B{B} -I{I} -c {W}/ucr_bad.c -o /dev/null 2>&1 | grep -c 'not valid in an identifier'; "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/ucr_ok.c -o /dev/null 2>&1 && echo OK_CLEAN; }",
		 "1\nOK_CLEAN\n"},

		{"va_start_last_param_clean", "",
		 "printf '#include <stdarg.h>\\nint f(int a,int b,...){va_list ap;va_start(ap,b);int r=va_arg(ap,int);va_end(ap);return r+a;}\\nint main(void){return f(1,2,3);}\\n' > {W}/vc.c && "
		 "{MCC} -B{B} -I{I} -Werror -c {W}/vc.c -o /dev/null 2>&1 && echo CLEAN",
		 "CLEAN\n"},

		{"static_assert_fail", "",
		 "printf '_Static_assert(1==2, \"sizes differ\");\\n' > {W}/static_assert_fail.c && {MCC} -fno-diagnostics-show-caret -c {W}/static_assert_fail.c -o {W}/static_assert_fail.o 2>&1 | grep -oE 'sizes differ' || echo FIXED_OK",
		 "sizes differ\n"},

		{"static_assert_nonconst", "",
		 "printf 'int x; _Static_assert(x, \"bad\");\\n' > {W}/static_assert_nonconst.c && {MCC} -c {W}/static_assert_nonconst.c -o {W}/static_assert_nonconst.o 2>&1 | grep -oE 'constant expression expected' || echo FIXED_OK",
		 "constant expression expected\n"},

		{"diagnostics_caret", "",
		 "printf 'int main(void){\\n\\tint x = 1\\n\\treturn x;\\n}\\n' > {W}/caret.c; "
		 "echo \"on:$({MCC} -c {W}/caret.c -o /dev/null 2>&1 | grep -cF '^')\"; "
		 "echo \"off:$({MCC} -fno-diagnostics-show-caret -c {W}/caret.c -o /dev/null 2>&1 | grep -cF '^')\"",
		 "on:1\noff:0\n"},

		{"diagnostics_color", "",
		 "printf 'int main(void){\\n\\tint x = 1\\n\\treturn x;\\n}\\n' > {W}/color.c; "
		 "echo \"always:$({MCC} -fdiagnostics-color=always -c {W}/color.c -o /dev/null 2>&1 | grep -c '\\[1;3')\"; "
		 "echo \"never:$({MCC} -fdiagnostics-color=never -c {W}/color.c -o /dev/null 2>&1 | grep -c '\\[1;3')\"; "
		 "echo \"auto:$({MCC} -c {W}/color.c -o /dev/null 2>&1 | grep -c '\\[1;3')\"",
		 "always:2\nnever:0\nauto:0\n"},

		{"switch_duplicate_case", "",
		 "printf 'int f(int a){switch(a){case 1: return 1; case 1: return 2;} return 0;}\\n' > {W}/switch_duplicate_case.c && {MCC} -c {W}/switch_duplicate_case.c -o {W}/switch_duplicate_case.o 2>&1 | grep -oE 'duplicate case value' || echo FIXED_OK",
		 "duplicate case value\n"},

		{"goto_undefined_label", "",
		 "printf 'int f(void){ goto nowhere; return 0; }\\n' > {W}/goto_undefined_label.c && {MCC} -c {W}/goto_undefined_label.c -o {W}/goto_undefined_label.o 2>&1 | grep -oE 'used but not defined' || echo FIXED_OK",
		 "used but not defined\n"},

		{"redefinition_object", "",
		 "printf 'int x=1; int x=2;\\n' > {W}/redefinition_object.c && {MCC} -c {W}/redefinition_object.c -o {W}/redefinition_object.o 2>&1 | grep -oE 'redefinition of' || echo FIXED_OK",
		 "redefinition of\n"},

		{"array_of_functions", "",
		 "printf 'int a[3](void);\\n' > {W}/array_of_functions.c && {MCC} -c {W}/array_of_functions.c -o {W}/array_of_functions.o 2>&1 | grep -oE 'array of functions' || echo FIXED_OK",
		 "array of functions\n"},

		{"conflicting_redecl", "",
		 "printf 'int f(void); double f(void);\\n' > {W}/conflicting_redecl.c && {MCC} -c {W}/conflicting_redecl.c -o {W}/conflicting_redecl.o 2>&1 | grep -oE 'incompatible types for redefinition' || echo FIXED_OK",
		 "incompatible types for redefinition\n"},

		{"bitfield_nonint", "",
		 "printf 'struct S { float b:3; };\\n' > {W}/bitfield_nonint.c && {MCC} -c {W}/bitfield_nonint.c -o {W}/bitfield_nonint.o 2>&1 | grep -oE 'bitfields must have scalar type' || echo FIXED_OK",
		 "bitfields must have scalar type\n"},

		{"void_param_named", "",
		 "printf 'int f(void x){ return 0; }\\n' > {W}/void_param_named.c && {MCC} -c {W}/void_param_named.c -o {W}/void_param_named.o 2>&1 | grep -oE 'parameter declared as void' || echo FIXED_OK",
		 "parameter declared as void\n"},

		{"computed_goto_ext", "",
		 "printf 'int f(void){ void *p = &&L; goto *p; L: return 0; }\\n' > {W}/computed_goto_ext.c && {MCC} -c {W}/computed_goto_ext.c -o {W}/computed_goto_ext.o 2>&1 | grep -oE 'FIXED_OK' || echo FIXED_OK",
		 "FIXED_OK\n"},

		{"c99_fam_not_last", "",
		 "printf 'struct s{int f[];int x;};\\n' > {W}/fam.c && "
		 "{MCC} -c {W}/fam.c -o {W}/fam.o 2>&1 | "
		 "grep -oE \"flexible array member .* not at the end of struct\"",
		 "flexible array member 'f' not at the end of struct\n"},
		{"c11_alignas_underalign", "",
		 "printf 'int main(void){_Alignas(1) int x;return x=0;}\\n' > {W}/al.c && "
		 "{MCC} -c {W}/al.c -o {W}/al.o 2>&1 | "
		 "grep -oE \"requested alignment is less than the minimum alignment of the type\"",
		 "requested alignment is less than the minimum alignment of the type\n"},
		{"c99_vla_goto_into_scope", "",
		 "printf 'int main(int c){goto L;{int a[c];L:return a[0];}}\\n' > {W}/vj.c && "
		 "{MCC} -c {W}/vj.c -o {W}/vj.o 2>&1 | "
		 "grep -oE \"goto jumps into the scope of a variably modified declaration\"",
		 "goto jumps into the scope of a variably modified declaration\n"},
		{"c99_vla_switch_into_scope", "",
		 "printf 'int main(int c){switch(c){case 1:{int a[c];case 2:return a[0];}}return 0;}\\n' > {W}/vs.c && "
		 "{MCC} -c {W}/vs.c -o {W}/vs.o 2>&1 | "
		 "grep -oE \"switch jumps into the scope of a variably modified declaration\"",
		 "switch jumps into the scope of a variably modified declaration\n"},
		{"c11_noreturn_returns", "",
		 "printf '#include <stdnoreturn.h>\\nnoreturn void f(int x){if(x)return;}\\nint main(void){return 0;}\\n' > {W}/nr.c && "
		 "{MCC} -B{B} -I{I} -c {W}/nr.c -o {W}/nr.o 2>&1 | "
		 "grep -oE \"function declared .noreturn. has a .return. statement\"",
		 "function declared 'noreturn' has a 'return' statement\n"},
		{"c99_kr_implicit_int", "",
		 "printf 'int g(x){return x;}\\n' > {W}/kri.c && "
		 "{MCC} -c {W}/kri.c -o {W}/kri.o 2>&1 | "
		 "grep -oE \"type of .x. defaults to .int. .implicit int removed in C99.\"",
		 "type of 'x' defaults to 'int' (implicit int removed in C99)\n"},
		{"c89_kr_implicit_int_ok", "",
		 "printf 'int g(x){return x;}\\nint main(void){return g(0);}\\n' > {W}/kro.c && "
		 "{MCC} -std=c89 -c {W}/kro.c -o {W}/kro.o 2>&1 | "
		 "grep -oE \"type of .x. defaults to .int.$\"",
		 "type of 'x' defaults to 'int'\n"},
		{"c99_inline_no_extern_def", "",
		 "printf 'inline int f(void){return 42;}\\nint g(void){return f();}\\n' > {W}/inl_u.c && "
		 "{MCC} -c {W}/inl_u.c -o {W}/inl_u.o >/dev/null 2>&1 && "
		 "nm {W}/inl_u.o | sed -E 's/ ([A-Za-z]) _/ \\1 /' | grep -oE 'U f'",
		 "U f\n"},
		{"c99_inline_extern_makes_def", "",
		 "printf 'extern int f(void);\\ninline int f(void){return 42;}\\nint g(void){return f();}\\n' > {W}/inl_t.c && "
		 "{MCC} -c {W}/inl_t.c -o {W}/inl_t.o >/dev/null 2>&1 && "
		 "nm {W}/inl_t.o | sed -E 's/ ([A-Za-z]) _/ \\1 /' | grep -oE 'T f'",
		 "T f\n"},
		{"c99_inline_emission_matrix", "",
		 "{MCC} -c {D}/../exec/functions_abi/inline.c -o {W}/inlmat.o >/dev/null 2>&1 && "
		 "nm {W}/inlmat.o | sed -E 's/ ([A-Za-z]) _/ \\1 /' | grep -oE '(U|[Tt]) (inline_inline_undeclared|extern_extern_undeclared|noinst_static_inline_predeclared|static_func|main)$' | LC_ALL=C sort",
		 "T extern_extern_undeclared\nT main\nU inline_inline_undeclared\nt noinst_static_inline_predeclared\nt static_func\n"},
		{"gnu89_plain_inline_emits_def", "",
		 "printf 'inline int f(void){return 42;}\\nint g(void){return f();}\\n' > {W}/g89e.c && "
		 "{MCC} -c -fgnu89-inline {W}/g89e.c -o {W}/g89e.o >/dev/null 2>&1 && "
		 "nm {W}/g89e.o | sed -E 's/ ([A-Za-z]) _/ \\1 /' | grep -oE 'T f'",
		 "T f\n"},
		{"gnu89_plain_inline_links_and_runs", "",
		 "printf 'inline int f(void){return 42;}\\nint main(void){return f();}\\n' > {W}/g89p.c && "
		 "{MCC} -B{B} -I{I} -fgnu89-inline {W}/g89p.c -o {W}/g89p >/dev/null 2>&1 && "
		 "{W}/g89p; echo rc=$?",
		 "rc=42\n"},
		{"c99_plain_inline_default_link_error", "",
		 "printf 'inline int f(void){return 42;}\\nint main(void){return f();}\\n' > {W}/c99p.c && "
		 "{MCC} -B{B} -I{I} {W}/c99p.c -o {W}/c99p 2>&1 | grep -oE 'unresolved reference to' | head -1",
		 "unresolved reference to\n"},
		{"gnu_extern_inline_no_static_copy", "",
		 "printf 'extern inline int f(void){return 42;}\\nint main(void){return f();}\\n' > {W}/g89x.c && "
		 "{MCC} -B{B} -I{I} -fgnu89-inline {W}/g89x.c -o {W}/g89x 2>&1 | grep -oE 'unresolved reference' ; "
		 "printf 'extern inline __attribute__((gnu_inline)) int f(void){return 42;}\\nint main(void){return f();}\\n' > {W}/g89a.c && "
		 "{MCC} -B{B} -I{I} {W}/g89a.c -o {W}/g89a 2>&1 | grep -oE 'unresolved reference' ; "
		 "printf 'extern inline int f(void){return 42;}\\nint f(void){return 7;}\\nint main(void){return f();}\\n' > {W}/g89y.c && "
		 "{MCC} -B{B} -I{I} -fgnu89-inline {W}/g89y.c -o {W}/g89y >/dev/null 2>&1 && {W}/g89y; echo rc=$?",
		 "unresolved reference\nunresolved reference\nrc=7\n"},
		{"c11_ucn_basic_latin_reject", "",
		 "printf '%s\\n' 'int a\\u0041b;' > {W}/ucnbl.c && "
		 "{MCC} -c {W}/ucnbl.c -o {W}/ucnbl.o 2>&1 | "
		 "grep -oE 'universal character .u0041 is not valid in an identifier'",
		 "universal character \\u0041 is not valid in an identifier\n"},
		{"c11_ucn_surrogate_reject", "",
		 "printf '%s\\n' 'int a\\uD800b;' > {W}/ucnsur.c && "
		 "{MCC} -c {W}/ucnsur.c -o {W}/ucnsur.o 2>&1 | "
		 "grep -oE 'universal character .ud800 is not valid in an identifier'",
		 "universal character \\ud800 is not valid in an identifier\n"},
		{"c11_signed_unsigned_reject", "",
		 "printf 'signed unsigned int x;\\n' > {W}/su.c && "
		 "{MCC} -c {W}/su.c -o {W}/su.o 2>&1 | "
		 "grep -oE 'signed and unsigned modifier'",
		 "signed and unsigned modifier\n"},
		{"run_bt_dwarf4_subdir_path", "backtrace",
		 "mkdir -p {W}/btsub && "
		 "printf 'int mcc_backtrace(const char *, ...);\\nvoid f(void) {\\nmcc_backtrace(\"here\");\\n}\\nint main(void) {\\nf();\\nreturn 0;\\n}\\n' > {W}/btsub/btp.c && "
		 "cd {W} && {MCC} -B{B} -bt -gdwarf-4 -run btsub/btp.c 2>&1",
		 "btsub/btp.c:3: at f: here\nbtsub/btp.c:6: by main\n"},

		{"run_bt_dwarf5_subdir_path", "backtrace",
		 "mkdir -p {W}/btsub && "
		 "printf 'int mcc_backtrace(const char *, ...);\\nvoid f(void) {\\nmcc_backtrace(\"here\");\\n}\\nint main(void) {\\nf();\\nreturn 0;\\n}\\n' > {W}/btsub/btp.c && "
		 "cd {W} && {MCC} -B{B} -bt -gdwarf-5 -run btsub/btp.c 2>&1",
		 "btsub/btp.c:3: at f: here\nbtsub/btp.c:6: by main\n"},

		{"bcheck_exe_static_bounds", "bcheck",
		 "printf 'char g_arr[10];\\nint main(void) {\\nchar *p = g_arr;\\np[12] = 1;\\nreturn 0;\\n}\\n' > {W}/gb.c && "
		 "{MCC} -B{B} -b {W}/gb.c -o {W}/gb && {W}/gb 2>&1 | grep -oE 'at main: RUNTIME ERROR: invalid memory access'",
		 "at main: RUNTIME ERROR: invalid memory access\n"},

		{"sanitize_address_heap_overflow", "bcheck",
		 "printf 'void *malloc(__SIZE_TYPE__);\\nint main(void){char *p=malloc(10);p[12]=1;return 0;}\\n' > {W}/asan.c && "
		 "{MCC} -B{B} -fsanitize=address {W}/asan.c -o {W}/asan && {W}/asan 2>&1 | grep -oE 'is outside of the region'",
		 "is outside of the region\n"},
		{"sanitize_address_macro", "bcheck",
		 "printf '#ifdef __SANITIZE_ADDRESS__\\nint main(void){return 5;}\\n#else\\nint main(void){return 0;}\\n#endif\\n' > {W}/asm.c && "
		 "{MCC} -B{B} -fsanitize=address {W}/asm.c -o {W}/asm && {W}/asm; echo rc=$?",
		 "rc=5\n"},
		{"sanitize_address_use_after_free", "bcheck",
		 "printf 'void *malloc(__SIZE_TYPE__);void free(void*);\\nint main(void){int*p=malloc(4);*p=5;free(p);return *p;}\\n' > {W}/uaf.c && "
		 "{MCC} -B{B} -fsanitize=address {W}/uaf.c -o {W}/uaf && {W}/uaf 2>&1 | grep -oE 'invalid memory access' | head -1",
		 "invalid memory access\n"},
		{"asan_shadow_native_overflow", "cpu=x86_64,os=linux",
		 "printf 'extern void*malloc(unsigned long);\\nint main(void){int*p=malloc(40);p[0]=1;return p[100];}\\n' > {W}/an.c && "
		 "{MCC} -B{B} -fasan-shadow {W}/an.c -o {W}/an && "
		 "{W}/an 2>&1 | grep -oE 'AddressSanitizer: heap-buffer-overflow' | head -1",
		 "AddressSanitizer: heap-buffer-overflow\n"},
		{"asan_shadow_native_use_after_free", "cpu=x86_64,os=linux",
		 "printf 'extern void*malloc(unsigned long);extern void free(void*);\\nint main(void){int*p=malloc(16);p[0]=7;free(p);return p[0];}\\n' > {W}/au.c && "
		 "{MCC} -B{B} -fasan-shadow {W}/au.c -o {W}/au && "
		 "{W}/au 2>&1 | grep -oE 'AddressSanitizer: heap-use-after-free' | head -1",
		 "AddressSanitizer: heap-use-after-free\n"},
		{"asan_shadow_native_global_overflow", "cpu=x86_64,os=linux",
		 "printf 'int g[10];\\nint main(void){g[0]=1;volatile int i=10;return g[i];}\\n' > {W}/ag.c && "
		 "{MCC} -B{B} -fasan-shadow {W}/ag.c -o {W}/ag && "
		 "{W}/ag 2>&1 | grep -oE 'AddressSanitizer: global-buffer-overflow' | head -1",
		 "AddressSanitizer: global-buffer-overflow\n"},
		{"asan_shadow_native_global_clean", "cpu=x86_64,os=linux",
		 "printf 'int bss[16];int data[4]={1,2,3,4};char nm[6]=\"hello\";\\nint main(void){int s=0;for(int i=0;i<16;i++)bss[i]=i;s=bss[15]+data[3]+nm[4];return s==(15+4+111)?0:7;}\\n' > {W}/agc.c && "
		 "{MCC} -B{B} -fasan-shadow {W}/agc.c -o {W}/agc && "
		 "{W}/agc; echo rc=$?",
		 "rc=0\n"},
		{"asan_shadow_native_stack_overflow", "cpu=x86_64,os=linux",
		 "printf 'int main(void){volatile int i=20;char buf[10];buf[0]=1;return buf[i];}\\n' > {W}/as.c && "
		 "{MCC} -B{B} -fasan-shadow {W}/as.c -o {W}/as && "
		 "{W}/as 2>&1 | grep -oE 'AddressSanitizer: stack-buffer-overflow' | head -1",
		 "AddressSanitizer: stack-buffer-overflow\n"},
		{"asan_shadow_native_stack_clean", "cpu=x86_64,os=linux",
		 "printf 'struct P{int a,b;};\\nint main(void){char buf[10];struct P p;int x=5;int*px=&x;for(int i=0;i<10;i++)buf[i]=i;p.a=buf[9];p.b=*px;return (p.a==9&&p.b==5)?0:7;}\\n' > {W}/asc.c && "
		 "{MCC} -B{B} -fasan-shadow {W}/asc.c -o {W}/asc && "
		 "{W}/asc; echo rc=$?",
		 "rc=0\n"},
		{"asan_shadow_access_type_write", "cpu=x86_64,os=linux",
		 "printf 'extern void*malloc(unsigned long);\\nint main(void){char*p=malloc(8);p[12]=1;return 0;}\\n' > {W}/atw.c && "
		 "{MCC} -B{B} -fasan-shadow {W}/atw.c -o {W}/atw && "
		 "{W}/atw 2>&1 | grep -oE '(READ|WRITE) of size [0-9]+' | head -1",
		 "WRITE of size 1\n"},
		{"asan_shadow_access_type_read", "cpu=x86_64,os=linux",
		 "printf 'extern void*malloc(unsigned long);\\nint main(void){char*p=malloc(8);char*q=malloc(64);q[0]=p[12];return 0;}\\n' > {W}/atr.c && "
		 "{MCC} -B{B} -fasan-shadow {W}/atr.c -o {W}/atr && "
		 "{W}/atr 2>&1 | grep -oE '(READ|WRITE) of size [0-9]+' | head -1",
		 "READ of size 1\n"},
		{"asan_shadow_access_type_width", "cpu=x86_64,os=linux",
		 "printf 'extern void*malloc(unsigned long);\\nint main(void){char*p=malloc(8);*(int*)(p+12)=5;return 0;}\\n' > {W}/atx.c && "
		 "{MCC} -B{B} -fasan-shadow {W}/atx.c -o {W}/atx && "
		 "{W}/atx 2>&1 | grep -oE '(READ|WRITE) of size [0-9]+' | head -1",
		 "WRITE of size 4\n"},
		{"asan_shadow_struct_member_overflow", "cpu=x86_64,os=linux",
		 "printf 'extern void*malloc(unsigned long);\\nstruct S{int a,b,c,d,e;};\\nint main(void){struct S*s=malloc(8);s->e=1;return 0;}\\n' > {W}/asm.c && "
		 "{MCC} -B{B} -fasan-shadow {W}/asm.c -o {W}/asm && "
		 "{W}/asm 2>&1 | grep -oE '(READ|WRITE) of size [0-9]+' | head -1",
		 "WRITE of size 4\n"},
		{"asan_shadow_struct_member_clean", "cpu=x86_64,os=linux",
		 "printf 'extern void*malloc(unsigned long);\\nstruct S{int a,b,c,d,e;};\\nint main(void){struct S*s=malloc(sizeof(struct S));s->a=1;s->e=5;return s->a+s->e==6?0:7;}\\n' > {W}/asmc.c && "
		 "{MCC} -B{B} -fasan-shadow {W}/asmc.c -o {W}/asmc && "
		 "{W}/asmc; echo rc=$?",
		 "rc=0\n"},
		{"zero_bss_under_asan", "cpu=x86_64,os=linux",
		 "printf 'int z0=0;long za[16]={0};int nz=7;\\nint main(void){za[3]=z0+nz;return za[3]==7?0:1;}\\n' > {W}/zba.c && "
		 "{MCC} -B{B} -O2 -fasan-shadow -c {W}/zba.c -o {W}/zba.o && "
		 "readelf -SW {W}/zba.o | awk '/ [.]bss /{print ($6==\"000000\")?\"bss-empty\":\"bss-used\"}'",
		 "bss-used\n"},
		{"zero_bss_under_bcheck", "cpu=x86_64,os=linux,bcheck",
		 "printf 'int z0=0;long za[16]={0};int nz=7;\\nint main(void){za[3]=z0+nz;return za[3]==7?0:1;}\\n' > {W}/zbb.c && "
		 "{MCC} -B{B} -O2 -b -c {W}/zbb.c -o {W}/zbb.o && "
		 "readelf -SW {W}/zbb.o | awk '/ [.]bss /{print ($6==\"000000\")?\"bss-empty\":\"bss-used\"}'",
		 "bss-used\n"},
		{"zero_bss_asan_still_catches_global", "cpu=x86_64,os=linux",
		 "printf 'int g[10]={0};\\nint main(void){g[0]=1;volatile int i=10;return g[i];}\\n' > {W}/zbg.c && "
		 "{MCC} -B{B} -O2 -fasan-shadow {W}/zbg.c -o {W}/zbg && "
		 "{W}/zbg 2>&1 | grep -oE 'AddressSanitizer: global-buffer-overflow' | head -1",
		 "AddressSanitizer: global-buffer-overflow\n"},
		{"asan_shadow_manual_link", "cpu=x86_64,os=linux",
		 "cc -O2 -c {D}/../../runtime/lib/mccasan.c -o {W}/mccasan_m.o 2>/dev/null && "
		 "printf 'extern void*malloc(unsigned long);\\nint main(void){int*p=malloc(40);p[0]=1;return p[100];}\\n' > {W}/anm.c && "
		 "{MCC} -B{B} -fasan-shadow -c {W}/anm.c -o {W}/anm.o && cc {W}/anm.o {W}/mccasan_m.o -o {W}/anm 2>/dev/null && "
		 "{W}/anm 2>&1 | grep -oE 'AddressSanitizer: heap-buffer-overflow' | head -1",
		 "AddressSanitizer: heap-buffer-overflow\n"},

		{"macro_eval_recursive", "",
		 "printf '#define fact(n) (n <= 1 ? 1 : n * fact(n - 1))\\nint main(void) { return fact(5) == 120 ? 0 : 1; }\\n' > {W}/me.c && "
		 "{MCC} -B{B} -fmacro-eval -run {W}/me.c && echo evaluated",
		 "evaluated\n"},

		{"macro_eval_off_by_default", "",
		 "printf '#define fact(n) (n <= 1 ? 1 : n * fact(n - 1))\\nint main(void) { return fact(5) == 120 ? 0 : 1; }\\n' > {W}/me2.c && "
		 "{MCC} -B{B} -run {W}/me2.c 2>&1 | grep -oE \"implicit declaration of function 'fact'\"",
		 "implicit declaration of function 'fact'\n"},

		{"x86_64_reloc_32s_range", "cpu=x86_64,os=linux,asm",
		 "printf '%s\\n' 'char a[1500000000];' 'char b[1500000000];' 'int main(void){int r;__asm__ volatile(\"movl $b+1499999000, %0\":\"=r\"(r));return r?0:1;}' > {W}/r32s.c && "
		 "{MCC} -no-pie {W}/r32s.c -o {W}/r32s 2>&1 | grep -oE \"relocation .R_X86_64_32.* out of range\"",
		 "relocation 'R_X86_64_32[S]' out of range\n"},

		{"c90_selection_stmt_tag_scope", "",
		 "printf '%s\\n' 'extern int printf(const char *, ...);' "
		 "'struct foo { char a; };' "
		 "'static int sfoo(void) { if (sizeof (struct foo { int a; double b; char *c; void *d; })) (void)0; return (int)sizeof(struct foo); }' "
		 "'int main(void) { printf(\"%d %d\\n\", sfoo(), (int)sizeof(struct foo)); return 0; }' > {W}/c90scope.c && "
		 "{MCC} -B{B} -I{I} -std=iso9899:1990 -O2 {W}/c90scope.c -o {W}/c90scope90 && {W}/c90scope90 && "
		 "{MCC} -B{B} -I{I} -std=c99 -O2 {W}/c90scope.c -o {W}/c90scope99 && {W}/c90scope99",
		 "32 1\n1 1\n"},

		{"no_wrapv_folds_mul_div_by_same_constant", "",
		 "printf '%s\\n' 'extern int printf(const char *, ...);' "
		 "'static int t(int x) { return (2 * x) / 2; }' "
		 "'int main(void) { volatile int v = 2147483647; printf(\"%d\\n\", t(v)); return 0; }' > {W}/wrapv2.c && "
		 "for o in -O1 -O2 -O3; do "
		 "{MCC} -B{B} -I{I} $o -fno-wrapv {W}/wrapv2.c -o {W}/wrapv2off && {W}/wrapv2off; "
		 "{MCC} -B{B} -I{I} $o -fwrapv {W}/wrapv2.c -o {W}/wrapv2on && {W}/wrapv2on; "
		 "done",
		 "2147483647\n-1\n2147483647\n-1\n2147483647\n-1\n"},

		{"abs_family_outranks_a_local_definition", "",
		 "printf '%s\\n' 'extern int printf(const char *, ...);' "
		 "'long long a = -1;' "
		 "'long long llabs(long long);' "
		 "'int main(void) { printf(\"%lld\\n\", llabs(a)); return 0; }' "
		 "'long long llabs(long long b) { return b; }' > {W}/llabs.c && "
		 "for o in -O0 -O1 -O2 -O3; do "
		 "{MCC} -B{B} -I{I} -w -std=c99 $o {W}/llabs.c -o {W}/llabsx && {W}/llabsx; "
		 "done; "
		 "{MCC} -B{B} -I{I} -w -std=c99 -O2 -fno-builtin {W}/llabs.c -o {W}/llabsnb && {W}/llabsnb",
		 "1\n1\n1\n1\n-1\n"},

		{"gnu_range_designator_nested_and_partial_override", "",
		 "printf '%s\\n' 'extern int printf(const char *, ...);' "
		 "'int a[][2][4] = {[2 ... 4][0 ... 1][2 ... 3] = 1, [2] = 2, [2][0][2] = 3};' "
		 "'struct I { int J; int K[3]; int L; };' "
		 "'struct M { int N; struct I O[3]; int P; };' "
		 "'struct M n[] = {[0 ... 5].O[1 ... 2].K[0 ... 1] = 4, 5, 6, 7};' "
		 "'struct M o[] = {[0 ... 5].O = {[1 ... 2].K[0 ... 1] = 4}, [5].O[2].K[2] = 5, 6, 7};' "
		 "'int main(void) { unsigned i, d = 0;' "
		 "'  unsigned char *pn = (unsigned char *)n, *po = (unsigned char *)o;' "
		 "'  for (i = 0; i < sizeof n; i++) if (pn[i] != po[i]) d++;' "
		 "'  printf(\"%d %d %d %d %d %d %d %d %d %d %d %d %u %u\\n\", a[2][0][0], a[2][0][2],' "
		 "'    a[2][0][3], a[2][1][2], a[4][1][3], n[0].P, n[0].O[1].K[2], n[0].O[2].L,' "
		 "'    n[3].O[2].K[1], n[5].O[2].K[2], n[5].O[2].L, n[5].P,' "
		 "'    (unsigned)(sizeof n / sizeof n[0]), d);' "
		 "'  return 0; }' > {W}/gnurng.c && "
		 "for o in -O0 -O2; do "
		 "{MCC} -B{B} -I{I} -w -std=gnu99 $o {W}/gnurng.c -o {W}/gnurngx && {W}/gnurngx; "
		 "done",
		 "2 3 1 1 1 0 0 0 4 5 6 7 6 0\n2 3 1 1 1 0 0 0 4 5 6 7 6 0\n"},

		{"xmm_hi_promoted_float_store_to_global", "cpu=x86_64,os=linux",
		 "printf '%s\\n' 'float gf[6];' 'double gd[6];' "
		 "'void ff(int n){float a=gf[0],b=gf[1];while(n--){a+=gf[2];b+=a;}gf[4]=a;gf[5]=b;}' "
		 "'void fd(int n){double a=gd[0],b=gd[1];while(n--){a+=gd[2];b+=a;}gd[4]=a;gd[5]=b;}' "
		 "'int main(void){gf[0]=1;gf[1]=2;gf[2]=3;gd[0]=1;gd[1]=2;gd[2]=3;ff(3);fd(3);' "
		 "'if(gf[4]!=10||gf[5]!=23)return 1;if(gd[4]!=10||gd[5]!=23)return 2;return 0;}' > {W}/xh.c && "
		 "{MCC} -B{B} -I{I} -w -O0 {W}/xh.c -o {W}/xh0 && "
		 "MCC_DEV=1 {MCC} -B{B} -I{I} -w -O1 -fpromote-locals -fpromote-leaf-xmm -fxmm-hi {W}/xh.c -o {W}/xhk && "
		 "MCC_DEV=1 {MCC} -B{B} -I{I} -w -O5 {W}/xh.c -o {W}/xh5 && "
		 "{W}/xh0; printf 'O0=%s ' $?; {W}/xhk; printf 'knobs=%s ' $?; {W}/xh5; printf 'O5=%s\\n' $?",
		 "O0=0 knobs=0 O5=0\n"},

		{"unreferenced_static_inline_is_not_emitted", "cpu=x86_64,os=linux",
		 "printf '%s\\n' 'extern void exit(int);' "
		 "'static inline int dead_body(void){extern int mcc_never_defined_probe;return mcc_never_defined_probe;}' "
		 "'static inline int dead_caller(void){if(dead_body==dead_body)return 1;return dead_body();}' "
		 "'int main(void){exit(0);}' > {W}/dsi.c && "
		 "{MCC} -B{B} -I{I} -w -O0 {W}/dsi.c -o {W}/dsi0 && {W}/dsi0 && echo O0-ok && "
		 "{MCC} -B{B} -I{I} -w -O4 {W}/dsi.c -o {W}/dsi4 && {W}/dsi4 && echo O4-ok && "
		 "{MCC} -B{B} -I{I} -w -O13 {W}/dsi.c -o {W}/dsi13 && {W}/dsi13 && echo O13-ok",
		 "O0-ok\nO4-ok\nO13-ok\n"},

};
static const int cli_cases_count = (int)(sizeof(cli_cases) / sizeof(cli_cases[0]));

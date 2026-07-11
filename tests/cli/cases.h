typedef struct
{
	const char *name, *req, *cmd, *expect;
} cli_case_t;

static const cli_case_t cli_cases[] = {

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
		 "XDG_CACHE_HOME={W}/cache {MCC} -B{B} -I{I} -O4 -c {W}/so.c -o {W}/so.o && "
		 "XDG_CACHE_HOME={W}/cache {MCC} -B{B} -I{I} -O4 -c {W}/so.c -o {W}/so_warm.o && "
		 "test $(wc -c < {W}/so_warm.o) -le $(wc -c < {W}/so.o) && echo WARMOK ; "
		 "{MCC} -B{B} -I{I} {W}/so.o -o {W}/so && {W}/so ; echo rc=$? ; "
		 "{MCC} -B{B} -I{I} --no-embed-jit -O0 -c {W}/so.c -o {W}/so2.o && echo FLAGOK",
		 "WARMOK\nrc=55\nFLAGOK\n"},

		{"embed_jit_manifest", "cpu=x86_64,os=linux,optimizer",
		 "printf 'int main(void){return 0;}\\n' > {W}/mf.c && "
		 "XDG_CACHE_HOME={W}/mfc {MCC} -B{B} -I{I} -O4 -v --jit-functions main,helper --jit-max-duration 120 -c {W}/mf.c -o {W}/mf.o 2>&1 | grep 'embed-jit manifest' ; "
		 "XDG_CACHE_HOME={W}/mfc {MCC} -B{B} -I{I} -O4 -v --no-embed-jit -c {W}/mf.c -o {W}/mf2.o 2>&1 | grep -c 'embed-jit manifest'",
		 "embed-jit manifest: functions=main,helper max-duration=120s\n0\n"},

		{"bitflag_detect", "cpu=x86_64,os=linux,optimizer",
		 "printf 'int classify(int x){if(x==1)return 10;else if(x==3)return 30;else if(x==5)return 50;else if(x==7)return 70;return 0;}int two(int y){if(y==2)return 1;if(y==4)return 2;return 0;}int main(void){return classify(5)+two(4);}\\n' > {W}/bf.c && "
		 "MCC_AST_BITFLAG=1 {MCC} -B{B} -I{I} -O1 -c {W}/bf.c -o {W}/bf.o 2>&1 | grep bitflag ; "
		 "{MCC} -B{B} -I{I} -O1 {W}/bf.c -o {W}/bfx && {W}/bfx ; echo rc=$?",
		 "bitflag-candidate: classify cluster=4\nrc=52\n"},

		{"ast_poison_lowering", "optimizer",
		 "printf 'int dse(int x){int a;a=5;a=7;a=x+1;return a;}\\nint sccp(int x){if(1)return x*2;else return -999;}\\nint bf(int f){int r=0;if(f&1)r+=1;if(f&2)r+=2;if(f&4)r+=4;return r;}\\nint main(void){int ok=dse(10)==11&&sccp(3)==6&&bf(5)==5;return ok?0:1;}\\n' > {W}/pz.c && "
		 "{MCC} -B{B} -O1 {W}/pz.c -o {W}/pz && {W}/pz && echo o1ok && "
		 "{MCC} -B{B} -O2 {W}/pz.c -o {W}/pz2 && {W}/pz2 && echo o2ok && "
		 "MCC_AST_REPLAY_DUMP=1 {MCC} -B{B} -O1 -c {W}/pz.c -o {W}/pz.o 2>&1 | grep -oE 'ast-dse|ast-sccp|Poison' | sort -u | tr '\\n' ','",
		 "o1ok\no2ok\nPoison,ast-dse,ast-sccp,"},

		{"bitflag_transform", "cpu=x86_64,os=linux,optimizer",
		 "printf 'int g;int f(int x){if(x==1||x==3||x==5||x==7||x==9)return 1;return 0;}int c(int x){if(x==2)g=4;else if(x==4)g=4;else if(x==6)g=4;else if(x==8)g=4;else if(x==10)g=4;else g=7;return g;}int main(void){int k[10]={-1,0,1,2,3,64,65,7,9,10},s=0;for(int i=0;i<10;i++)s+=f(k[i])+c(k[i]);return s;}\\n' > {W}/bft.c && "
		 "MCC_AST_REPLAY_DUMP=1 MCC_AST_BITFLAG=1 {MCC} -B{B} -I{I} -O1 -c {W}/bft.c -o {W}/bft.o 2>&1 | grep -c 'ast-bitflag' ; "
		 "MCC_AST_BITFLAG=1 {MCC} -B{B} -I{I} -O1 {W}/bft.c -o {W}/bft && {W}/bft ; echo rc=$? ; "
		 "MCC_AST_BITFLAG=1 {MCC} -B{B} -I{I} -O3 {W}/bft.c -o {W}/bft3 && {W}/bft3 ; echo rc=$? ; "
		 "{MCC} -B{B} -I{I} -O1 {W}/bft.c -o {W}/bft0 && {W}/bft0 ; echo rc=$?",
		 "2\nrc=68\nrc=68\nrc=68\n"},

		{"bitflag_ifne", "cpu=x86_64,os=linux,optimizer",
		 "printf 'int ni(int x){if(x!=1){if(x!=3){if(x!=5){if(x!=7){if(x!=9){return 1;}}}}}return 0;}int main(void){int k[10]={-1,0,1,2,3,64,65,7,9,10},s=0;for(int i=0;i<10;i++)s+=ni(k[i]);return s;}\\n' > {W}/bfn.c && "
		 "MCC_AST_REPLAY_DUMP=1 MCC_AST_BITFLAG=1 {MCC} -B{B} -I{I} -O1 -c {W}/bfn.c -o {W}/bfn.o 2>&1 | grep -c 'ast-bitflag' ; "
		 "MCC_AST_BITFLAG=1 {MCC} -B{B} -I{I} -O1 {W}/bfn.c -o {W}/bfn && {W}/bfn ; echo rc=$? ; "
		 "MCC_AST_BITFLAG=1 {MCC} -B{B} -I{I} -O3 {W}/bfn.c -o {W}/bfn3 && {W}/bfn3 ; echo rc=$? ; "
		 "{MCC} -B{B} -I{I} -O1 {W}/bfn.c -o {W}/bfn0 && {W}/bfn0 ; echo rc=$?",
		 "1\nrc=6\nrc=6\nrc=6\n"},

		{"pre_diamond", "cpu=x86_64,os=linux,optimizer",
		 "printf 'int f(int a,int b,int c){int x=0,y=0,r;if(c){x=a+b;}else{y=a+b;}r=a+b;return x+y+r;}int main(void){return f(3,4,1)+f(3,4,0);}\\n' > {W}/pre.c && "
		 "{MCC} -B{B} -I{I} -O2 -c {W}/pre.c -o {W}/pre.off.o && "
		 "MCC_AST_PRE=1 {MCC} -B{B} -I{I} -O2 -c {W}/pre.c -o {W}/pre.on.o && "
		 "( cmp -s {W}/pre.off.o {W}/pre.on.o && echo SAME || echo DIFFER ) ; "
		 "MCC_AST_PRE=1 {MCC} -B{B} -I{I} -O2 {W}/pre.c -o {W}/pre2 && {W}/pre2 ; echo rc=$? ; "
		 "MCC_AST_PRE=1 {MCC} -B{B} -I{I} -O3 {W}/pre.c -o {W}/pre3 && {W}/pre3 ; echo rc=$? ; "
		 "{MCC} -B{B} -I{I} -O2 {W}/pre.c -o {W}/pre0 && {W}/pre0 ; echo rc=$?",
		 "DIFFER\nrc=28\nrc=28\nrc=28\n"},

		{"perfn_inproc", "cpu=x86_64,os=linux,optimizer",
		 "printf 'static int big(int x,int y){int s=0;for(int i=0;i<x;i++){s+=(i*y)^(i+x);s-=(i&y)|(x^i);s+=(i*i)-(y*y);}return s;}static int tiny(int x){return x+1;}int main(void){int s=0;s+=big(5,3)+big(7,2)+big(9,4)+big(3,6);s+=tiny(10)+tiny(20)+tiny(30);return s&0x7f;}\\n' > {W}/pfi.c && "
		 "{MCC} -B{B} -I{I} -O3 -c {W}/pfi.c -o {W}/pfi.off.o && "
		 "MCC_AST_PERFN_INPROC=1 {MCC} -B{B} -I{I} -O3 -c {W}/pfi.c -o {W}/pfi.on.o && "
		 "( cmp -s {W}/pfi.off.o {W}/pfi.on.o && echo SAME || echo DIFFER ) ; "
		 "MCC_AST_PERFN_INPROC=1 {MCC} -B{B} -I{I} -O3 {W}/pfi.c -o {W}/pfi.on && {W}/pfi.on ; echo rc=$? ; "
		 "{MCC} -B{B} -I{I} -O0 {W}/pfi.c -o {W}/pfi.o0 && {W}/pfi.o0 ; echo rc=$?",
		 "DIFFER\nrc=28\nrc=28\n"},

		{"perfn_search", "cpu=x86_64,os=linux,optimizer",
		 "printf 'static int sq(int x){return x*x;}static int cube(int x){return x*x*x;}int main(void){int s=0;for(int i=0;i<8;i++)s+=sq(i)+cube(i);return s;}\\n' > {W}/pfs.c && "
		 "XDG_CACHE_HOME={W}/pfsc MCC_AST_PERFN=1 {MCC} -B{B} -I{I} -O4 -c {W}/pfs.c -o {W}/pfs.o && "
		 "{MCC} -B{B} -I{I} {W}/pfs.o -o {W}/pfs && {W}/pfs ; echo rc=$?",
		 "rc=156\n"},

		{"per_fn_config", "cpu=x86_64,os=linux,optimizer",
		 "printf 'static int sq(int x){return x*x;}int main(void){int s=0;for(int i=0;i<10;i++)s+=sq(i);return s;}\\n' > {W}/pf.c && "
		 "MCC_AST_TEMPLATES=1 MCC_AST_FN_CONFIG='main=1;sq=1' {MCC} -B{B} -I{I} -O3 -c {W}/pf.c -o {W}/pf1.o && "
		 "MCC_AST_TEMPLATES=1 MCC_AST_FN_CONFIG='main=7;sq=7' {MCC} -B{B} -I{I} -O3 -c {W}/pf.c -o {W}/pf7.o && "
		 "( cmp -s {W}/pf1.o {W}/pf7.o && echo SAME || echo DIFFER ) ; "
		 "{MCC} -B{B} -I{I} -O3 {W}/pf.c -o {W}/pf && {W}/pf ; echo rc=$?",
		 "DIFFER\nrc=29\n"},

		{"ast_cost_report", "cpu=x86_64,os=linux,optimizer",
		 "printf 'static int inner(int x){int s=0;for(int i=0;i<x;i++)for(int j=0;j<x;j++)s+=i*j;return s;}\\nint main(void){return inner(5);}\\n' > {W}/cost.c && "
		 "MCC_AST_COST=1 {MCC} -B{B} -I{I} -O1 -c {W}/cost.c -o {W}/cost.o 2>&1 | grep '^ast-cost' | awk '{print $2, $4}' | sort",
		 "inner loopdepth=2\nmain loopdepth=0\n"},

		{"clear_cache_and_jit_flags", "cpu=x86_64,os=linux,optimizer",
		 "printf 'int main(void){return 0;}\\n' > {W}/cc.c && "
		 "XDG_CACHE_HOME={W}/cch {MCC} -B{B} -I{I} -O4 -c {W}/cc.c -o {W}/cc.o && "
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

		{"dM_dump_macros", "",
		 "printf '\\n' > {W}/empty.c && {MCC} -B{B} -E -dM {W}/empty.c | grep -cE '^#define __STDC__ '",
		 "1\n"},

		{"stdc_utf_encoding_macros", "",
		 "printf '\\n' > {W}/utf.c && {MCC} -B{B} -E -dM {W}/utf.c | grep -cE '^#define __STDC_UTF_(16|32)__ 1$'",
		 "2\n"},

		{"nostdinc_drops_system", "os!=WIN32",
		 "printf '#include <stdio.h>\\n' > {W}/ns.c && {MCC} -B{B} -nostdinc -E {W}/ns.c 2>&1 | grep -coE 'not found|No such'",
		 "1\n"},

		{"dumpmachine", "os!=WIN32",
		 "{MCC} -dumpmachine | grep -qE '^(x86_64|i386|i686|aarch64|arm64|arm|riscv64)-' && echo TRIPLE_OK",
		 "TRIPLE_OK\n"},

		{"dumpversion_format", "",
		 "{MCC} -dumpversion | grep -cE '^[0-9]+\\.[0-9]+'",
		 "1\n"},

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
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/dn3.c -o {W}/dn3.o 2>&1 | wc -l | tr -d ' '; "
		 "{MCC} -B{B} -I{I} -std=c11 -fno-ms-extensions -c {W}/dn3.c -o {W}/dn3.o 2>&1 | "
		 "grep -c 'declaration does not declare anything'",
		 "2\nDN_OK\nidentifier expected\n0\n1\n"},

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

		{"pedantic_diagnostics", "",
		 "printf 'int f(void){ int n=3; struct S{int a[n];}; struct S s; return sizeof s; }\\n' > {W}/pd1.c && "
		 "{MCC} -B{B} -I{I} -std=c11 -c {W}/pd1.c -o {W}/pd1.o 2>&1 | wc -l | tr -d ' '; "
		 "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/pd1.c -o {W}/pd1.o 2>&1 | "
		 "grep -oE 'variably modified type'; "
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
		 "0\nvariably modified type\nrange of 'int'\ncomma operator in a constant expression\nin a 'for' loop initializer\n'_Noreturn' used outside of a function\nflexible array member\nforward references to 'enum' types\n'sizeof' applied to a function type\ndoes not support '_Static_assert' before C11\nflexible array member in a struct with no named members\nEND\n"},

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
		 "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/ii.c -o {W}/ii.o 2>&1; "
		 "{MCC} -B{B} -I{I} -std=c11 -Wall {W}/iv.c -o {W}/iv 2>&1 && echo CLEAN_OK; } | "
		 "grep -oE \"type defaults to 'int' in declaration|return type defaults to 'int'|CLEAN_OK\" | sort | uniq -c | sed 's/^ *//'",
		 "1 CLEAN_OK\n1 return type defaults to 'int'\n2 type defaults to 'int' in declaration\n"},

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
		 "{ {MCC} -B{B} -I{I} -c {W}/sav.c -o {W}/sav.o 2>&1; "
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
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/kr_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
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
		 "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/tdf_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
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

		{"wpedantic_alias", "",
		 "printf 'int x = 0b101;\\nint main(void){return x;}\\n' > {W}/wp.c && "
		 "{ {MCC} -B{B} -I{I} -Wpedantic -c {W}/wp.c -o /dev/null 2>&1; "
		 "{MCC} -B{B} -I{I} -Wpedantic -Wno-pedantic -Werror -c {W}/wp.c -o /dev/null 2>&1 && echo OFF_OK; "
		 "{MCC} -B{B} -I{I} -Wpedantic -pedantic-errors -c {W}/wp.c -o /dev/null 2>&1; } | "
		 "grep -oE 'C23/GNU extension|OFF_OK' | sort | uniq -c | sed 's/^ *//'",
		 "2 C23/GNU extension\n1 OFF_OK\n"},

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
		{"gnu89_extern_inline_static_copy_diff", "",
		 "printf 'extern inline int f(void){return 42;}\\nint main(void){return f();}\\n' > {W}/g89x.c && "
		 "{MCC} -B{B} -I{I} -fgnu89-inline {W}/g89x.c -o {W}/g89x >/dev/null 2>&1 && "
		 "{W}/g89x; echo rc=$?",
		 "rc=42\n"},
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
		 "printf 'void *malloc(unsigned long);\\nint main(void){char *p=malloc(10);p[12]=1;return 0;}\\n' > {W}/asan.c && "
		 "{MCC} -B{B} -fsanitize=address {W}/asan.c -o {W}/asan && {W}/asan 2>&1 | grep -oE 'is outside of the region'",
		 "is outside of the region\n"},
		{"sanitize_address_macro", "bcheck",
		 "printf '#ifdef __SANITIZE_ADDRESS__\\nint main(void){return 5;}\\n#else\\nint main(void){return 0;}\\n#endif\\n' > {W}/asm.c && "
		 "{MCC} -B{B} -fsanitize=address {W}/asm.c -o {W}/asm && {W}/asm; echo rc=$?",
		 "rc=5\n"},
		{"sanitize_address_use_after_free", "bcheck",
		 "printf 'void *malloc(unsigned long);void free(void*);\\nint main(void){int*p=malloc(4);*p=5;free(p);return *p;}\\n' > {W}/uaf.c && "
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

};
static const int cli_cases_count = (int)(sizeof(cli_cases) / sizeof(cli_cases[0]));

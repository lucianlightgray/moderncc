/* Structural CLI test cases for mcc. See runner.c for the format.
 * Commands use grep -o / awk to reduce tool output to a small deterministic
 * shape; "..." in expect is a wildcard. Most assert genuine behavior; a few
 * are smoke tests for flags that are accepted-but-no-op today (noted inline). */
typedef struct { const char *name, *req, *cmd, *expect; } cli_case_t;

static const cli_case_t cli_cases[] = {

/* ---- output modes ---------------------------------------------------- */
{ "shared_dyn_soname", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -shared -Wl,-soname,libfoo.so.1 {D}/lib.c -o {W}/x.so && "
  "readelf -h {W}/x.so | grep -oE 'DYN' && readelf -d {W}/x.so | grep -oE 'libfoo\\.so\\.1'",
  "DYN\nlibfoo.so.1\n" },

{ "relocatable_partial_link", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -r {D}/lib.c {D}/sec.c -o {W}/m.o && "
  "readelf -h {W}/m.o | grep -oE 'REL' && "
  "nm {W}/m.o | grep -oE 'exported_fn|second_fn|placed_var' | sort -u",
  "REL\nexported_fn\nplaced_var\nsecond_fn\n" },

/* -s: honored (no -Wunsupported warning under -Werror) and the linked output
   carries no .symtab.  mcc never emits a static symbol table into linked output
   anyway, so this also locks that in. */
{ "strip_symbols", "cpu=x86_64,os=linux",
  "printf 'int f(int x){return x+1;}\\nint main(void){return f(0);}\\n' > {W}/st.c && "
  "{MCC} -B{B} -I{I} -Werror -s {W}/st.c -o {W}/st && "
  "readelf -S {W}/st | grep -c '\\.symtab'",
  "0\n" },

/* -fPIC -pie produces a PIE (ET_DYN) with no text relocations; -fno-pic
   -no-pie produces a plain executable (ET_EXEC). Runtime PIC on i386/arm is
   additionally exercised under qemu (qemu matrix builds every program -fPIC). */
{ "fpic_pie_dyn", "cpu=x86_64,os=linux",
  "printf 'extern int g; int f(void){return g;}\\nint g=41; int main(void){return f()-g+1;}\\n' > {W}/pc.c && "
  "{MCC} -B{B} -I{I} -fPIC -pie {W}/pc.c -o {W}/pc && "
  "readelf -h {W}/pc | grep -oE 'DYN' && readelf -d {W}/pc | grep -c TEXTREL",
  "DYN\n0\n" },

{ "fno_pic_exec", "cpu=x86_64,os=linux",
  "printf 'int main(void){return 0;}\\n' > {W}/np.c && "
  "{MCC} -B{B} -I{I} -fno-pic -no-pie {W}/np.c -o {W}/np && readelf -h {W}/np | grep -oE 'EXEC'",
  "EXEC\n" },

/* ---- symbol attributes / visibility ---------------------------------- */
{ "visibility_attribute", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -c {D}/vis.c -o {W}/v.o && "
  "readelf -s {W}/v.o | grep -E 'hidden_att|shown_one|plain_one' | awk '{print $6, $8}' | sort",
  "DEFAULT plain_one\nDEFAULT shown_one\nHIDDEN hidden_att\n" },

{ "fvisibility_hidden_default_wins", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -fvisibility=hidden -c {D}/vis.c -o {W}/vh.o && "
  "readelf -s {W}/vh.o | grep -E 'hidden_att|shown_one|plain_one' | awk '{print $6, $8}' | sort",
  "DEFAULT shown_one\nHIDDEN hidden_att\nHIDDEN plain_one\n" },

{ "section_attribute", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -c {D}/sec.c -o {W}/s.o && readelf -S {W}/s.o | grep -oE '\\.mysec' | head -1",
  ".mysec\n" },

{ "leading_underscore", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -fleading-underscore -c {D}/lib.c -o {W}/lu.o && nm {W}/lu.o | grep -oE '_exported_fn'",
  "_exported_fn\n" },

{ "rdynamic_exports_main", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -rdynamic {D}/hello.c -o {W}/hr && readelf --dyn-syms {W}/hr | grep -cE ' main$'",
  "1\n" },

/* function/data-sections are currently accepted no-ops: smoke-test that the
   flags parse and still produce a working object. */
{ "function_data_sections_accepted", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -ffunction-sections -fdata-sections -c {D}/lib.c -o {W}/fsd.o && echo OK",
  "OK\n" },

/* ---- stack protector ------------------------------------------------- */
{ "stack_protector_on", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -fstack-protector-all -c {D}/sp.c -o {W}/sp.o && nm {W}/sp.o | grep -oE '__stack_chk_fail' | head -1",
  "__stack_chk_fail\n" },

{ "stack_protector_off", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -fno-stack-protector -c {D}/sp.c -o {W}/sp2.o && nm {W}/sp2.o | grep -c __stack_chk_fail",
  "0\n" },

/* ---- debug info ------------------------------------------------------ */
{ "debug_default_stabs", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -g -c {D}/lib.c -o {W}/g.o && readelf -S {W}/g.o | grep -oE '\\.stab' | sort -u",
  ".stab\n" },

{ "debug_gstabs", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -gstabs -c {D}/lib.c -o {W}/st.o && readelf -S {W}/st.o | grep -oE '\\.stab' | sort -u",
  ".stab\n" },

{ "debug_dwarf5_info", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -gdwarf-5 -c {D}/lib.c -o {W}/g5.o && "
  "readelf --debug-dump=info {W}/g5.o 2>/dev/null | grep -oE 'DW_TAG_subprogram|exported_fn' | sort -u",
  "DW_TAG_subprogram\nexported_fn\n" },

{ "debug_dwarf_version_select", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -gdwarf-5 -c {D}/lib.c -o {W}/g5b.o && "
  "readelf --debug-dump=info {W}/g5b.o 2>/dev/null | awk '/Version/{print $2; exit}'",
  "5\n" },

/* ---- constructors ---------------------------------------------------- */
{ "constructor_init_array", "cpu=x86_64,os=linux",
  "printf '__attribute__((constructor)) void c1(void){}\\nint main(void){return 0;}\\n' > {W}/ctor.c && "
  "{MCC} -B{B} -I{I} -c {W}/ctor.c -o {W}/ctor.o && readelf -S {W}/ctor.o | grep -oE '\\.init_array' | head -1",
  ".init_array\n" },

/* ---- dependency generation ------------------------------------------- */
{ "deps_M_rule", "",
  "cd {D} && {MCC} -B{B} -I{I} -M dep.c",
  "dep.o: \\\ndep.c \\\ndep.h\n" },

{ "deps_MD_MF_file", "",
  "cd {D} && {MCC} -B{B} -I{I} -MD -MF {W}/out.d -c dep.c -o {W}/dep.o && grep -oE 'dep\\.(c|h)' {W}/out.d",
  "dep.c\ndep.h\n" },

/* ---- preprocessor flags ---------------------------------------------- */
{ "include_next_directive", "",
  "{MCC} -B{B} -I{I} -I{D}/incnext/d1 -I{D}/incnext/d2 {D}/incnext/incnext_main.c -o {W}/incn && {W}/incn",
  "1 2\n" },

{ "undef_flag", "",
  "printf 'X\\n' > {W}/u.c && {MCC} -B{B} -DX=1 -UX -E -P {W}/u.c",
  "X\n" },

{ "dM_dump_macros", "",
  "printf '\\n' > {W}/empty.c && {MCC} -B{B} -E -dM {W}/empty.c | grep -cE '^#define __STDC__ '",
  "1\n" },

{ "nostdinc_drops_system", "os=linux",
  "printf '#include <stdio.h>\\n' > {W}/ns.c && {MCC} -B{B} -nostdinc -E {W}/ns.c 2>&1 | grep -coE 'not found|No such'",
  "1\n" },

/* ---- info / dump ----------------------------------------------------- */
{ "dumpmachine", "cpu=x86_64,os=linux",
  "{MCC} -dumpmachine | grep -oE 'x86_64'",
  "x86_64\n" },

{ "dumpversion_format", "",
  "{MCC} -dumpversion | grep -cE '^[0-9]+\\.[0-9]+'",
  "1\n" },

/* crt: is an ELF-only section (Mach-O links crt via the SDK, not a crtprefix),
   so assert only the sections every target prints. */
{ "print_search_dirs", "",
  "{MCC} -B{B} -print-search-dirs | grep -oE '^(install|include|libraries):'",
  "install:\ninclude:\nlibraries:\n" },

/* ---- sub-tools / driver --------------------------------------------- */
{ "ar_create_list", "",
  "{MCC} -B{B} -I{I} -c {D}/lib.c -o {W}/al.o && {MCC} -B{B} -I{I} -c {D}/sec.c -o {W}/as.o && "
  "{MCC} -ar rcs {W}/libcli.a {W}/al.o {W}/as.o && {MCC} -ar t {W}/libcli.a",
  "al.o\nas.o\n" },

{ "response_file", "",
  "printf -- '-c {D}/lib.c -o {W}/resp.o\\n' > {W}/a.rsp && {MCC} -B{B} -I{I} @{W}/a.rsp && "
  "nm {W}/resp.o | grep -oE 'exported_fn'",
  "exported_fn\n" },

/* ---- symbol metadata / assembler driver ----------------------------- */
{ "symbol_type_func_object", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -c {D}/lib.c -o {W}/ts.o && "
  "readelf -s {W}/ts.o | grep -E 'exported_fn|global_var' | awk '{print $4}' | sort -u",
  "FUNC\nOBJECT\n" },

{ "assemble_dot_s_file", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} {D}/asmadd.s {D}/asmmain.c -o {W}/ae && {W}/ae",
  "42\n" },

{ "weak_override_multi_tu", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -c {D}/wstrong.c -o {W}/wstrong.o && "
  "{MCC} -B{B} -I{I} {D}/wmain.c {W}/wstrong.o -o {W}/we && {W}/we",
  "1\n" },

/* ---- dynamic / debug / TLS structure -------------------------------- */
{ "shared_dynamic_tags", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -shared -Wl,-soname,libt.so.1 {D}/lib.c -o {W}/lt.so && "
  "readelf -d {W}/lt.so | grep -oE 'SONAME|GNU_HASH|BIND_NOW' | sort -u && "
  "readelf -l {W}/lt.so | grep -oE 'GNU_RELRO' | head -1",
  "BIND_NOW\nGNU_HASH\nSONAME\nGNU_RELRO\n" },

{ "rpath_new_dtags_runpath", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -Wl,-rpath,/opt/x -Wl,--enable-new-dtags -shared {D}/lib.c -o {W}/rp.so && "
  "readelf -d {W}/rp.so | grep -oE 'RUNPATH'",
  "RUNPATH\n" },

{ "dwarf_line_table", "cpu=x86_64,os=linux",
  "{MCC} -B{B} -I{I} -gdwarf-5 -c {D}/lib.c -o {W}/gl.o && "
  "readelf --debug-dump=decodedline {W}/gl.o 2>/dev/null | grep -oE 'lib\\.c' | head -1",
  "lib.c\n" },

{ "tls_segment_and_run", "os=linux",
  "{MCC} -B{B} -I{I} {D}/tlsvar.c -o {W}/te && {W}/te && "
  "readelf -l {W}/te | grep -oE 'TLS' | head -1",
  "7\nTLS\n" },

{ "fcommon_vs_default", "cpu=x86_64,os=linux",
  "printf 'int gg;\\n' > {W}/cm.c && "
  "{MCC} -B{B} -I{I} -c {W}/cm.c -o {W}/cm.o && nm {W}/cm.o | awk '/ gg$/{print $2}' && "
  "{MCC} -B{B} -I{I} -fcommon -c {W}/cm.c -o {W}/cmc.o && nm {W}/cmc.o | awk '/ gg$/{print $2}'",
  "B\nC\n" },

/* ---- warnings / preprocessor flags ---------------------------------- */
{ "werror_promotes_to_error", "",
  "printf 'int main(void){ undeclared_fn(); return 0; }\\n' > {W}/werr.c && "
  "{MCC} -B{B} -I{I} -Werror -c {W}/werr.c -o {W}/werr.o 2>&1 | grep -oE 'error: implicit declaration' | head -1",
  "error: implicit declaration\n" },

{ "wwrite_strings_warns", "",
  "printf 'char *p = \\\"x\\\"; void f(void){ *p = 0; }\\n' > {W}/ws.c && "
  "{MCC} -B{B} -I{I} -Wwrite-strings -c {W}/ws.c -o {W}/ws.o 2>&1 | grep -coE 'discards qualifiers|read-only'",
  "1\n" },

{ "multichar_warning", "",
  "{MCC} -B{B} -I{I} -Wall -c {D}/multichar.c -o {W}/mc.o 2>&1 | grep -oE 'multi-character'",
  "multi-character\n" },

{ "integer_suffix_error", "",
  "{MCC} -B{B} -I{I} -c {D}/suffix_bad.c -o {W}/sb.o 2>&1 | grep -oE \"three 'l's\"",
  "three 'l's\n" },

{ "include_flag", "",
  "printf '#define INCV 7\\n' > {W}/inc.h && printf 'int x = INCV;\\n' > {W}/iu.c && "
  "{MCC} -B{B} -I{I} -include {W}/inc.h -E -P {W}/iu.c | grep -oE 'int x = 7'",
  "int x = 7\n" },

{ "isystem_include", "",
  "mkdir -p {W}/sysinc && printf '#define SYSVAL 11\\n' > {W}/sysinc/syshdr.h && "
  "printf '#include <syshdr.h>\\nint v = SYSVAL;\\n' > {W}/ui.c && "
  "{MCC} -B{B} -I{I} -isystem {W}/sysinc -E -P {W}/ui.c | grep -oE 'int v = 11'",
  "int v = 11\n" },

{ "pragma_comment_lib", "",
  "printf '#pragma comment(lib,\\\"m\\\")\\nint main(void){ return 0; }\\n' > {W}/pc.c && "
  "{MCC} -B{B} -I{I} {W}/pc.c -o {W}/pce && echo OK",
  "OK\n" },

{ "x_force_language", "",
  "printf '#include <stdio.h>\\nint main(void){ puts(\\\"xc\\\"); return 0; }\\n' > {W}/notc.txt && "
  "{MCC} -B{B} -I{I} -x c {W}/notc.txt -o {W}/xce && {W}/xce",
  "xc\n" },

/* 6.7.2.4p3: _Atomic rejects array and function types — both the explicit
   _Atomic(type-name) form and the bare qualifier on a typedef'd array/function
   (e.g. `typedef int A[3]; _Atomic A a;`). _Atomic on a typedef'd pointer, and
   `_Atomic int x[3]` (array of atomic int), remain valid. */
{ "atomic_type_constraints", "",
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
  "VALID_OK\n_Atomic cannot be applied to a function type\n_Atomic cannot be applied to an array type\n" },

/* 6.7.4p2 / 6.7.1p3,p4: function specifier on non-function, _Thread_local on
   a function, and block-scope _Thread_local without static/extern. */
{ "storage_specifier_constraints", "",
  "printf 'inline int x;\\n' > {W}/sc1.c && "
  "printf '_Thread_local void f(void);\\n' > {W}/sc2.c && "
  "printf 'void g(void){ _Thread_local int y; (void)y; }\\n' > {W}/sc3.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/sc1.c -o {W}/sc1.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/sc2.c -o {W}/sc2.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/sc3.c -o {W}/sc3.o 2>&1; } | "
  "grep -oE \"'(inline|_Thread_local)'.*\" | sort -u",
  "'_Thread_local' applied to a function\n'_Thread_local' at block scope requires 'static' or 'extern'\n'inline' used outside of a function declaration\n" },

/* 6.5.3.2 / 6.5.3.4: & , sizeof, _Alignof may not be applied to a bit-field. */
{ "bitfield_operand_constraints", "",
  "printf 'struct S{int b:3;}s; int *p(void){return &s.b;}\\n' > {W}/bf1.c && "
  "printf 'struct S{int b:3;}s; int n(void){return (int)sizeof(s.b);}\\n' > {W}/bf2.c && "
  "printf 'struct S{int b:3;}s; int a(void){return (int)_Alignof(s.b);}\\n' > {W}/bf3.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/bf1.c -o {W}/bf1.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/bf2.c -o {W}/bf2.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/bf3.c -o {W}/bf3.o 2>&1; } | "
  "grep -oE '(cannot take address of|(sizeof|_Alignof). cannot be applied to a) bit-field' | sort -u",
  "_Alignof' cannot be applied to a bit-field\ncannot take address of bit-field\nsizeof' cannot be applied to a bit-field\n" },

/* 6.6: an integer constant expression must have integer type (case labels,
   enum values, array sizes). A cast to int is still a valid ICE. */
{ "integer_constant_expr_type", "",
  "printf 'int f(int x){switch(x){case 1.5: return 1; default: return 0;}}\\n' > {W}/ic1.c && "
  "printf 'enum E{A=1.5};\\n' > {W}/ic2.c && "
  "printf 'int v[(int)1.5];int main(void){return (int)sizeof v;}\\n' > {W}/ic3.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/ic1.c -o {W}/ic1.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/ic2.c -o {W}/ic2.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 {W}/ic3.c -o {W}/ic3 2>&1 && echo CAST_OK; } | "
  "grep -oE 'integer constant expression must have integer type|CAST_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CAST_OK\n2 integer constant expression must have integer type\n" },

/* 6.4.4.1: a decimal/hex integer constant too large for any standard type is a
   warning (and accepted), not a hard error — matching gcc (only clang escalates
   to an error). Covers a suffixless decimal > LLONG_MAX and a hex > ULLONG_MAX. */
{ "integer_constant_overflow", "",
  "printf 'unsigned long long a=99999999999999999999;\\n"
  "unsigned long long b=0xFFFFFFFFFFFFFFFF0;\\nint main(void){return (a!=0)&&(b!=0)?0:1;}\\n' > {W}/ov.c && "
  "{MCC} -B{B} -I{I} -std=c11 {W}/ov.c -o {W}/ov 2>&1 | grep -c 'integer constant overflow'; "
  "{W}/ov && echo OVF_RUN_OK",
  "2\nOVF_RUN_OK\n" },

/* 6.5.4: no cast between a floating type and a pointer (either direction). */
{ "float_pointer_cast_constraint", "",
  "printf 'void *p(double d){return (void*)d;}\\n' > {W}/fp1.c && "
  "printf 'double g(void*q){return (double)q;}\\n' > {W}/fp2.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/fp1.c -o {W}/fp1.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/fp2.c -o {W}/fp2.o 2>&1; } | "
  "grep -oE 'cannot cast between a floating type and a pointer' | sort | uniq -c | sed 's/^ *//'",
  "2 cannot cast between a floating type and a pointer\n" },

/* 6.5.1.1p2: no two _Generic associations may specify compatible types. */
{ "generic_duplicate_assoc", "",
  "printf 'int f(void){return _Generic(1,long:1,long:2,int:3);}\\n' > {W}/gd.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/gd.c -o {W}/gd.o 2>&1 | "
  "grep -oE '_Generic specifies two compatible types'",
  "_Generic specifies two compatible types\n" },

/* 6.9p2: file-scope declarations may not specify auto or register. */
{ "file_scope_storage_class", "",
  "printf 'auto int x;\\n' > {W}/fs1.c && "
  "printf 'register int y;\\n' > {W}/fs2.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/fs1.c -o {W}/fs1.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/fs2.c -o {W}/fs2.o 2>&1; } | "
  "grep -oE \"file-scope declaration of '.' specifies '(auto|register)'\" | sort",
  "file-scope declaration of 'x' specifies 'auto'\nfile-scope declaration of 'y' specifies 'register'\n" },

/* 6.7.3p2: specifier-level restrict on a non-pointer type is a constraint
   violation; restrict on a pointer (incl. via a pointer typedef) is fine. */
{ "restrict_requires_pointer", "",
  "printf 'int restrict x;\\n' > {W}/rr1.c && "
  "printf 'typedef int* IP; restrict IP q; int *restrict p;\\nint main(void){return !!p+!!q;}\\n' > {W}/rr2.c && "
  /* 6.7.3p2: a restrict-qualified pointer to a *function* (the function part is
     parsed after the *restrict) is a constraint violation; a restrict pointer
     to an object (incl. pointer-to-array) is fine. */
  "printf 'void (*restrict fp)(void);\\n' > {W}/rr3.c && "
  "printf 'int (*restrict pa)[3]; int *restrict *pp;\\nint main(void){return !!pa+!!pp;}\\n' > {W}/rr4.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/rr1.c -o {W}/rr1.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/rr3.c -o {W}/rr3.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 {W}/rr2.c -o {W}/rr2 2>&1 && echo PTR_OK; "
  "{MCC} -B{B} -I{I} -std=c11 {W}/rr4.c -o {W}/rr4 2>&1 && echo OBJPTR_OK; } | "
  "grep -oE \"'restrict' requires a pointer type|pointer to function type may not be 'restrict'-qualified|PTR_OK|OBJPTR_OK\"",
  "'restrict' requires a pointer type\npointer to function type may not be 'restrict'-qualified\nPTR_OK\nOBJPTR_OK\n" },

/* 6.7.5p2,p4: _Alignas constraints (typedef, function, register, bit-field,
   under-alignment). Valid over-alignment must still compile. */
{ "alignas_constraints", "",
  "printf 'typedef _Alignas(16) int T;\\n' > {W}/aa1.c && "
  "printf '_Alignas(16) void f(void);\\n' > {W}/aa2.c && "
  "printf '_Alignas(1) double d;\\n' > {W}/aa3.c && "
  "printf 'struct S{_Alignas(16) int b:3;};\\n' > {W}/aa4.c && "
  "printf 'void f(_Alignas(16) int x);\\n' > {W}/aa6.c && "
  "printf '_Alignas(64) int a; int main(void){return (int)_Alignof(a);}\\n' > {W}/aa5.c && "
  "{ for f in aa1 aa2 aa3 aa4 aa6; do {MCC} -B{B} -I{I} -std=c11 -c {W}/$f.c -o {W}/$f.o 2>&1; done; "
  "{MCC} -B{B} -I{I} -std=c11 {W}/aa5.c -o {W}/aa5 2>&1 && echo OVERALIGN_OK; } | "
  "grep -oE \"'_Alignas' specified for a (typedef|function|bit-field|function parameter)|requested alignment is less than the minimum alignment of the type|OVERALIGN_OK\" | sort",
  "'_Alignas' specified for a bit-field\n'_Alignas' specified for a function\n'_Alignas' specified for a function parameter\n'_Alignas' specified for a typedef\nOVERALIGN_OK\nrequested alignment is less than the minimum alignment of the type\n" },

/* 6.10.6: #pragma STDC FP_CONTRACT/FENV_ACCESS/CX_LIMITED_RANGE recognized and
   accepted (no "ignored" warning, even under -Werror); unknown pragmas still
   warn. */
{ "pragma_stdc_recognized", "",
  "printf '#pragma STDC FP_CONTRACT ON\\n#pragma STDC FENV_ACCESS OFF\\n"
  "#pragma STDC CX_LIMITED_RANGE DEFAULT\\nint main(void){return 0;}\\n' > {W}/ps.c && "
  "{MCC} -B{B} -I{I} -std=c11 -Wall -Werror -c {W}/ps.c -o {W}/ps.o && echo OK; "
  "printf '#pragma frobnicate q\\nint main(void){return 0;}\\n' > {W}/pf.c && "
  "{MCC} -B{B} -I{I} -std=c11 -Wall -c {W}/pf.c -o {W}/pf.o 2>&1 | grep -oE 'frobnicate ignored'",
  "OK\nfrobnicate ignored\n" },

/* 6.7.2p2: C11 removed implicit int — a K&R parameter with no declaration
   defaults to int and is diagnosed (gcc/clang error; mcc warns, matching its
   lenient legacy-code stance). A fully-declared old-style list is fine. */
{ "knr_implicit_int_param", "",
  "printf 'int f(x){ return x; }\\nint h(a,b) int a; char b; { return a+b; }\\n"
  "int main(void){return f(1)+h(2,3);}\\n' > {W}/kr.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/kr.c -o {W}/kr.o 2>&1 | "
  "grep -c \"type of 'x' defaults to 'int'\"",
  "1\n" },

/* 6.7.6.2p4: `[*]` (unspecified-size VLA) may appear only in function prototype
   scope, not in a definition. gcc and clang both error. A prototype, a nested
   prototype inside a definition's parameter, and ordinary [n]/[static N]/[]
   parameters are all fine. */
{ "star_array_in_funcdef", "",
  "printf 'void f(int a[*]){(void)a;}\\n' > {W}/sd.c && "
  "printf 'void p(int a[*]); void g(void(*fp)(int a[*])){(void)fp;}\\n"
  "int n=3; void h(int a[n],int b[static 3],int c[]){(void)a;(void)b;(void)c;}\\n"
  "int main(void){return 0;}\\n' > {W}/sp.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/sd.c -o {W}/sd.o 2>&1 | "
  "grep -oE \"'\\[\\*\\]' not allowed in a function definition\"; "
  "{MCC} -B{B} -I{I} -std=c11 {W}/sp.c -o {W}/sp && echo PROTO_OK",
  "'[*]' not allowed in a function definition\nPROTO_OK\n" },

/* 6.7.6.3p7: 'static' and type qualifiers in an array parameter's '[]' may
   appear only in the outermost array derivation. gcc+clang reject the inner
   forms; the outermost forms (incl. VLA size) stay valid. */
{ "array_param_static_outermost", "",
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
  "non-outermost array declarator\nnon-outermost array declarator\nOUTERMOST_OK\n" },

/* 6.8.6.1/6.8.4.2: a goto or switch that jumps into the scope of a variably
   modified (VLA) declaration is a constraint violation; gcc+clang both reject.
   Forward goto, backward goto into a closed VLA scope, and switch-into-VLA all
   error; valid jumps within/around a VLA scope stay accepted. */
{ "jump_into_vla_scope", "",
  "printf 'int n=3; int f(void){goto L; { int a[n]; L: return sizeof a; } }\\n' > {W}/j1.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/j1.c -o {W}/j1.o 2>&1 | "
  "grep -oE 'variably modified declaration'; "
  "printf 'int n=3; int g(int c){switch(c){ int a[n]; case 1: return sizeof a; default: return 0;} }\\n' > {W}/j2.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/j2.c -o {W}/j2.o 2>&1 | "
  "grep -oE 'variably modified declaration'; "
  "printf 'int n=3; int h(void){ { int a[n]; L: (void)a; } goto L; return 0; }\\n' > {W}/j3.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/j3.c -o {W}/j3.o 2>&1 | "
  "grep -oE 'variably modified declaration'; "
  "printf 'int n=3;\\nint ok(int c){ int a[n]; L: if(a[0]) goto L;"
  " switch(c){ case 1: { int b[n]; return sizeof b; } default: return sizeof a; } }\\n"
  "int main(void){return 0;}\\n' > {W}/j4.c && "
  "{MCC} -B{B} -I{I} -std=c11 {W}/j4.c -o {W}/j4 && echo VALID_OK",
  "variably modified declaration\nvariably modified declaration\nvariably modified declaration\nVALID_OK\n" },

/* 6.2.6/7.17: atomics on objects larger than a machine word use the size-
   generic, pointer-based libatomic helpers (__atomic_load/store/exchange/
   compare_exchange — needs -latomic at link, like gcc/clang); 1/2/4/8-byte
   objects keep the lock-free size-suffixed path (no -latomic). */
{ "atomic_large_generic", "",
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
  "__atomic_compare_exchange\n__atomic_exchange\n__atomic_load\n__atomic_store\n0\n" },

/* 6.5.16.2/6.5.2.4: a read-modify-write on an _Atomic object is atomic. The
   integer ops (+= -= &= |= ^=, ++/--) use direct __atomic_* helpers; the other
   integer ops (*= /= %= <<= >>=) and float atomics use a compare-exchange loop;
   only types larger than a machine word (e.g. long double) are rejected. */
/* elf: on Mach-O/arm64 (and PE) long double == double, so the _Atomic long
   double RMW is a small lock-free op mcc accepts rather than rejecting. */
{ "atomic_rmw_unsupported", "elf",
  "printf '_Atomic long double ld; void f(void){ ld*=2; }\\n' > {W}/ar1.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/ar1.c -o {W}/ar1.o 2>&1 | "
  "grep -oE 'compound assignment to an ._Atomic. object is not supported'; "
  "printf '#include <stdatomic.h>\\nint main(void){ atomic_int g=7; g*=3; g%%=5;"
  " g<<=4; _Atomic double d=2; d*=2.5; d+=1; return ((int)g==16 && d==6)?0:1; }\\n' > {W}/ar3.c && "
  "{MCC} -B{B} -I{I} -std=c11 {W}/ar3.c -o {W}/ar3 && {W}/ar3 && echo RMW_OK",
  "compound assignment to an '_Atomic' object is not supported\nRMW_OK\n" },

/* 6.7.2.1: a struct/union member with a type specifier but no declarator
   (e.g. 'int;' or a tagged enum) declares nothing; gcc/clang warn and accept
   (previously mcc errored "identifier expected"). Anonymous bit-fields and
   normal members are unaffected; a malformed declarator still errors. */
{ "member_declares_nothing", "",
  "printf 'struct S{int;}; struct T{enum E{A=5};};\\n"
  "int main(void){ return (sizeof(struct S)>=0 && A==5)?0:1; }\\n' > {W}/dn.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/dn.c -o {W}/dn.o 2>&1 | "
  "grep -c 'declaration does not declare anything'; "
  "{MCC} -B{B} -I{I} -std=c11 {W}/dn.c -o {W}/dn >/dev/null 2>&1 && {W}/dn && echo DN_OK; "
  "printf 'struct U{int @;};\\n' > {W}/dn2.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/dn2.c -o {W}/dn2.o 2>&1 | "
  "grep -oE 'identifier expected'; "
  /* a named-tag struct member with no declarator: silent under MS extensions
     (mcc default, == gcc -fms-extensions), warn+accept under
     -fno-ms-extensions (== gcc/clang default), never reject-valid. */
  "printf 'struct W{struct T{int a;};};\\n' > {W}/dn3.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/dn3.c -o {W}/dn3.o 2>&1 | wc -l | tr -d ' '; "
  "{MCC} -B{B} -I{I} -std=c11 -fno-ms-extensions -c {W}/dn3.c -o {W}/dn3.o 2>&1 | "
  "grep -c 'declaration does not declare anything'",
  "2\nDN_OK\nidentifier expected\n0\n1\n" },

/* 6.5.4p2: the type name in a cast shall be void or scalar; casting to an
   array type or to a struct unrelated to the operand is a constraint
   violation (gcc+clang reject). No-op casts to a compatible struct and casts
   to a complex type stay accepted; va_start/va_arg (which cast through
   'struct __va_list_tag *', not the array type) still compile and run. */
{ "cast_to_nonscalar", "",
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
  "conversion to non-scalar type requested\nconversion to non-scalar type requested\nCAST_OK\n" },

/* 7.17.2.1: atomic_load/atomic_exchange of an aggregate _Atomic now route
   through the size-generic libatomic helper (no more register-return crash; it
   delivers the result through a pointer). The .o references __atomic_load /
   __atomic_exchange (needs -latomic to link); aggregate atomic_store stays on
   the lock-free size-suffixed path, and scalar atomics still compile + run. */
{ "atomic_aggregate_load_generic", "",
  "printf '#include <stdatomic.h>\\nstruct P{int x,y;};\\n_Atomic struct P p;\\n"
  "void f(struct P v){ struct P r=atomic_load(&p);(void)r;"
  " struct P o=atomic_exchange(&p,v);(void)o; atomic_store(&p,v); }\\n' > {W}/aa.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/aa.c -o {W}/aa.o && "
  "nm {W}/aa.o | grep -oE '__atomic_(load|exchange|store_8)$' | sort -u; "
  "printf '#include <stdatomic.h>\\nint main(void){ atomic_int x=1; atomic_store(&x,5);"
  " return (int)atomic_load(&x)==5?0:1; }\\n' > {W}/as.c && "
  "{MCC} -B{B} -I{I} -std=c11 {W}/as.c -o {W}/as && {W}/as && echo SCALAR_OK",
  "__atomic_exchange\n__atomic_load\n__atomic_store_8\nSCALAR_OK\n" },

/* -pedantic / -pedantic-errors: diagnose strict-ISO-C constraint violations
   that mcc (like gcc) accepts as extensions by default. Silent without the
   flag, a warning with -pedantic, a hard error with -pedantic-errors. Covers
   6.7.2.1p9 (VLA member), 6.7.2.2p2 (enum value range), 6.6p3 (comma in ICE). */
{ "pedantic_diagnostics", "",
  "printf 'int f(void){ int n=3; struct S{int a[n];}; struct S s; return sizeof s; }\\n' > {W}/pd1.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/pd1.c -o {W}/pd1.o 2>&1 | wc -l | tr -d ' '; "        /* default: silent (0 lines) */
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
  "0\nvariably modified type\nrange of 'int'\ncomma operator in a constant expression\nin a 'for' loop initializer\n'_Noreturn' used outside of a function\nflexible array member\nforward references to 'enum' types\n'sizeof' applied to a function type\ndoes not support '_Static_assert' before C11\nflexible array member in a struct with no named members\nEND\n" },

/* 6.2.5p27: a plain in-language store to / read from an _Atomic aggregate or
   >8-byte object (e.g. _Atomic struct, _Atomic long double) must be indivisible
   — lowered to the generic __atomic_store/__atomic_load(size,...) libcall (needs
   -latomic), not a non-atomic struct/multi-word copy. */
/* elf: nm output and the __atomic_* symbol spelling assume ELF naming; Mach-O
   prefixes them with an extra underscore (___atomic_load) and PE differs. */
{ "atomic_inlang_aggregate", "elf",
  "printf '#include <stdatomic.h>\\ntypedef struct{long a,b,c;}Big;\\n"
  "_Atomic Big g; _Atomic long double ld;\\n"
  "void f(Big v){ g = v; }\\nvoid h(long double x){ ld = x; }\\n"
  "void r(Big *p){ *p = g; }\\n' > {W}/ia.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/ia.c -o {W}/ia.o && "
  "nm {W}/ia.o | grep -oE 'U __atomic_(store|load)' | sort -u",
  "U __atomic_load\nU __atomic_store\n" },

/* 6.4.1/Annex G: _Imaginary is a reserved keyword but imaginary types are not
   implemented (nor by gcc/clang); using it gives a clear diagnostic rather than
   being treated as an undeclared identifier. _Complex is unaffected. */
{ "imaginary_not_supported", "",
  "printf '_Imaginary float x;\\n' > {W}/im.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/im.c -o {W}/im.o 2>&1 | "
  "grep -oE 'imaginary types are not supported'; "
  "printf '#include <complex.h>\\ndouble _Complex z=1.0;\\nint main(void){return 0;}\\n' > {W}/cx.c && "
  "{MCC} -B{B} -I{I} -std=c11 {W}/cx.c -o {W}/cx && echo COMPLEX_OK",
  "imaginary types are not supported\nCOMPLEX_OK\n" },

/* 6.7.4p9: a _Noreturn function that returns to its caller is UB; gcc and clang
   both warn on a return statement inside one. A non-noreturn function is fine. */
{ "noreturn_returns", "",
  "printf '_Noreturn void f(int x){ if(x) return; for(;;); }\\n"
  "void ok(void){ return; }\\nint main(void){return 0;}\\n' > {W}/nr.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/nr.c -o {W}/nr.o 2>&1 | "
  "grep -c \"function declared 'noreturn' has a 'return' statement\"",
  "1\n" },

/* 6.10.3p5: __VA_ARGS__ may appear only in the replacement list of a variadic
   (...) macro; gcc warns by default. A genuinely variadic macro is fine. */
{ "va_args_non_variadic", "",
  "printf '#define F(a) __VA_ARGS__\\n#define V(a, ...) __VA_ARGS__\\n"
  "int main(void){return 0;}\\n' > {W}/va.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/va.c -o {W}/va.o 2>&1 | "
  "grep -c 'can only appear in the expansion'",
  "1\n" },

/* 6.10p1: an unknown #-directive is a constraint violation; gcc and clang both
   make it a hard error. #ident/#sccs remain accepted (ignored) extensions. */
{ "unknown_directive_error", "",
  "printf '#frobnicate xyz\\nint main(void){return 0;}\\n' > {W}/ud.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/ud.c -o {W}/ud.o 2>&1 | "
  "grep -oE 'invalid preprocessing directive #frobnicate'; "
  "printf '#ident \\\"v\\\"\\n#sccs \\\"w\\\"\\nint main(void){return 0;}\\n' > {W}/ui.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/ui.c -o {W}/ui.o && echo IDENT_OK",
  "invalid preprocessing directive #frobnicate\nIDENT_OK\n" },

/* 6.10.6: #pragma message("...") emits a note (gcc/clang compatible), not
   gated on -Wall; accepts both the parenthesized and bare-string forms; does
   not raise an error even under -Werror. */
{ "pragma_message_note", "",
  "printf '#pragma message(\\\"hi there\\\")\\n#pragma message \\\"bare form\\\"\\n"
  "int main(void){return 0;}\\n' > {W}/pm.c && "
  "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/pm.c -o {W}/pm.o 2>&1 | "
  "grep -oE 'note: #pragma message: (hi there|bare form)'",
  "note: #pragma message: hi there\nnote: #pragma message: bare form\n" },

/* 6.7.6.3p1 / 6.9.1: a function shall not return a function or array type
   (incl. via a typedef name); returning a pointer to one is fine. */
{ "function_return_type_constraint", "",
  "printf 'typedef int AT[3]; AT f(void);\\n' > {W}/rt1.c && "
  "printf 'typedef int FT(void); FT g(void);\\n' > {W}/rt2.c && "
  "printf 'typedef int AT[3]; AT *ok1(void); typedef int FT(void); FT *ok2(void);\\n"
  "int main(void){return 0;}\\n' > {W}/rt3.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/rt1.c -o {W}/rt1.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/rt2.c -o {W}/rt2.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 {W}/rt3.c -o {W}/rt3 2>&1 && echo PTR_OK; } | "
  "grep -oE 'function cannot return an? (array|function) type|PTR_OK' | sort",
  "PTR_OK\nfunction cannot return a function type\nfunction cannot return an array type\n" },

/* 6.4.2.2: __func__ is only defined inside a function (warn at file scope). */
{ "func_outside_function", "",
  "printf 'const char *p = __func__;\\n' > {W}/fn.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/fn.c -o {W}/fn.o 2>&1 | "
  "grep -oE \"'__func__' is not defined outside of function scope\"",
  "'__func__' is not defined outside of function scope\n" },

/* 6.4.4.1: the ll/LL suffix must be case-uniform (1Ll / 1lL are invalid);
   6.5.3.4: _Alignof must not be applied to an incomplete type. */
{ "lexical_alignof_constraints", "",
  "printf 'long long a=1Ll;\\n' > {W}/ix1.c && "
  "printf 'long long a=1lL;\\n' > {W}/ix2.c && "
  "printf 'struct S; int n=_Alignof(struct S);\\n' > {W}/ix3.c && "
  "printf 'long long a=1LL,b=1ll; unsigned long long c=1ull,d=1ULL;\\n"
  "struct T{int x;}; int n=(int)_Alignof(struct T)+(int)_Alignof(int);\\n"
  "int main(void){return 0;}\\n' > {W}/ix4.c && "
  "{ for f in ix1 ix2 ix3; do {MCC} -B{B} -I{I} -std=c11 -c {W}/$f.c -o {W}/$f.o 2>&1; done; "
  "{MCC} -B{B} -I{I} -std=c11 {W}/ix4.c -o {W}/ix4 2>&1 && echo VALID_OK; } | "
  "grep -oE 'incorrect integer suffix|_Alignof. applied to an incomplete type|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 VALID_OK\n1 _Alignof' applied to an incomplete type\n2 incorrect integer suffix\n" },

/* 6.4.3p2: a universal character name in an identifier may not designate a
   code point < 00A0 (except $ @ `) nor a surrogate (D800-DFFF). gcc and clang
   both reject these. A legal UCN (e.g. é) and the $ exception compile. */
{ "ucn_identifier_range", "",
  "printf 'int \\\\u0041 = 5;\\n' > {W}/un1.c && "
  "printf 'int \\\\U0000d800x;\\n' > {W}/un2.c && "
  "printf 'int \\\\u00e9 = 5;\\nint main(void){return \\\\u00e9-5;}\\n' > {W}/un3.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/un1.c -o {W}/un1.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/un2.c -o {W}/un2.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 {W}/un3.c -o {W}/un3 2>&1 && echo UCN_OK; } | "
  "grep -oE 'universal character .u(0041|d800) is not valid in an identifier|UCN_OK' | sort",
  "UCN_OK\nuniversal character \\u0041 is not valid in an identifier\nuniversal character \\ud800 is not valid in an identifier\n" },

/* 6.5.1: implicit function declaration is diagnosed even inside a K&R-style
   (empty-paren) function body, not only inside prototyped bodies. */
{ "implicit_decl_in_knr_body", "",
  "printf 'int main(){ return foo(); }\\n' > {W}/id.c && "
  "{MCC} -B{B} -I{I} -std=c11 -Wall -c {W}/id.c -o {W}/id.o 2>&1 | "
  "grep -oE \"implicit declaration of function 'foo'\"",
  "implicit declaration of function 'foo'\n" },

/* 6.7.2.1p4: a _Bool bit-field may not be wider than 1. */
{ "bool_bitfield_width", "",
  "printf 'struct S{_Bool b:2;};\\n' > {W}/bb.c && "
  "printf 'struct T{_Bool b:1; int d:8;}; int main(void){return 0;}\\n' > {W}/bg.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/bb.c -o {W}/bb.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 {W}/bg.c -o {W}/bg 2>&1 && echo VALID_OK; } | "
  "grep -oE \"width of '.' exceeds its type|VALID_OK\"",
  "width of 'b' exceeds its type\nVALID_OK\n" },

/* 6.5.3.2/6.7.1: the address of a register-qualified object may not be taken. */
{ "register_address_constraint", "",
  "printf 'int *f(void){register int x=0; return &x;}\\n' > {W}/rga.c && "
  "printf 'int main(void){register int x=5; int y=x; int *p=&y; return *p+x-10;}\\n' > {W}/rgb.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/rga.c -o {W}/rga.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 {W}/rgb.c -o {W}/rgb 2>&1 && echo VALID_OK; } | "
  "grep -oE \"address of register variable '.' requested|VALID_OK\"",
  "address of register variable 'x' requested\nVALID_OK\n" },

/* 6.7.2p2: C11 removed implicit int — diagnose specifier-only object decls and
   implicit-int function return types; valid typed declarations stay quiet. */
{ "implicit_int_diag", "",
  "printf 'const x = 3;\\nstatic y = 7;\\nfoo(void){return 0;}\\n' > {W}/ii.c && "
  "printf 'long a; unsigned b; const int c; int g(void){return 0;}\\nint main(void){return g();}\\n' > {W}/iv.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/ii.c -o {W}/ii.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Wall {W}/iv.c -o {W}/iv 2>&1 && echo CLEAN_OK; } | "
  "grep -oE \"type defaults to 'int' in declaration|return type defaults to 'int'|CLEAN_OK\" | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 return type defaults to 'int'\n2 type defaults to 'int' in declaration\n" },

/* 6.5p2 -Wsequence-point: an object modified twice with no intervening
   sequence point warns (i=i++); the neighboring well-defined forms (i=i+1,
   the comma operator, distinct call arguments, ?: arms) stay quiet, and
   -Wno-sequence-point silences the warning entirely. */
{ "wsequence_point_diag", "",
  "printf 'void g(void){int i=0;i=i++;}\\n' > {W}/spw.c && "
  "printf 'int f(int a,int b){return a+b;}\\n"
  "void h(void){int i=0,j=0,a=0;i=i+1;i++,j++;f(i++,j++);a=i?i++:j++;}\\n' > {W}/spo.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/spw.c -o {W}/spw.o 2>&1; "
  "{MCC} -B{B} -I{I} -Wno-sequence-point -c {W}/spw.c -o {W}/spw.o 2>&1; "
  "{MCC} -B{B} -I{I} -c {W}/spo.c -o {W}/spo.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE \"operation on 'i' may be undefined|CLEAN_OK\" | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 operation on 'i' may be undefined\n" },

/* 6.5p2 (10.1.9): objects are keyed by (symbol, constant offset), so a constant
   array element / aggregate member modified twice with no sequence point warns
   (a[2]=a[2]++, s.a=s.a++) while distinct siblings (a[0]=a[1], s.a=s.b) stay
   quiet — sub-objects are tracked without conflation. */
{ "wsequence_point_subobject", "",
  "printf 'struct S{int a,b;};\\n"
  "void g(void){struct S s;int a[4];s.a=s.a++;a[2]=a[2]++;(void)s;(void)a;}\\n' > {W}/spsw.c && "
  "printf 'struct S{int a,b;};\\n"
  "void h(void){struct S s;int a[4];s.a=s.b;a[0]=a[1];(void)s;(void)a;}\\n' > {W}/spso.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/spsw.c -o {W}/spsw.o 2>&1; "
  "{MCC} -B{B} -I{I} -c {W}/spso.c -o {W}/spso.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE \"operation on '[sa]' may be undefined|CLEAN_OK\" | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 operation on 'a' may be undefined\n1 operation on 's' may be undefined\n" },

/* §6.8 jump/selection constraints (10.7.2/10.7.3): classic constraints that
   predate the conformance tracker — break/continue outside a loop, case/default
   outside a switch, a duplicate case label — each must be diagnosed, while the
   well-formed loop+switch neighbor still compiles. mcc aborts on the first
   error, so each bad form is compiled on its own. */
{ "jump_constraints", "",
  "printf 'void f(void){break;}\\n' > {W}/j1.c && "
  "printf 'void f(void){continue;}\\n' > {W}/j2.c && "
  "printf 'void f(void){case 1:;}\\n' > {W}/j3.c && "
  "printf 'void f(int x){switch(x){case 1:;case 1:;}}\\n' > {W}/j4.c && "
  "printf 'int f(int x){int s=0;for(int i=0;i<x;i++){if(i==2)continue;if(i==5)break;s+=i;}"
  "switch(x){case 1:s++;break;default:s--;}return s;}\\n' > {W}/jok.c && "
  "{ for n in j1 j2 j3 j4; do {MCC} -B{B} -I{I} -c {W}/$n.c -o {W}/$n.o 2>&1; done; "
  "{MCC} -B{B} -I{I} -c {W}/jok.c -o {W}/jok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE \"cannot break|cannot continue|duplicate case value|switch expected|CLEAN_OK\" | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 cannot break\n1 cannot continue\n1 duplicate case value\n1 switch expected\n" },

{ "common_symbol_merge", "cpu=x86_64,os=linux",
  "printf 'int shared_g;\\nvoid set_it(void){ shared_g = 5; }\\n' > {W}/cm1.c && "
  "printf '#include <stdio.h>\\nint shared_g; void set_it(void);\\n"
  "int main(void){ set_it(); printf(\\\"%%d\\\\n\\\", shared_g); return 0; }\\n' > {W}/cm2.c && "
  "{MCC} -B{B} -I{I} -fcommon {W}/cm1.c {W}/cm2.c -o {W}/cme && {W}/cme",
  "5\n" },

/* §6.5.3.3p1: the operand of unary - shall have arithmetic type; a pointer
   operand is a constraint violation (gcc+clang both error). The valid integer
   form still compiles. */
{ "unary_minus_pointer", "",
  "printf 'int f(int*p){return (-p)==0;}\\n' > {W}/umn.c && "
  "printf 'int f(int x){return -x;}\\nint main(void){return 0;}\\n' > {W}/umok.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/umn.c -o {W}/umn.o 2>&1; "
  "{MCC} -B{B} -I{I} {W}/umok.c -o {W}/umok 2>&1 && echo VALID_OK; } | "
  "grep -oE 'pointer not accepted for unary minus|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 VALID_OK\n1 pointer not accepted for unary minus\n" },

/* §6.4.4.4p9: a non-wide octal/hex escape shall be representable in unsigned
   char; out-of-range narrow escapes warn (gcc warns, clang errors), while
   in-range escapes compile clean under -Werror. */
{ "escape_out_of_range", "",
  "printf 'char *s=\"\\\\777\";\\n' > {W}/eo.c && "
  "printf 'char *s=\"\\\\xfff\";\\n' > {W}/ex.c && "
  "printf 'char *s=\"\\\\77\\\\xff\";\\nint main(void){return 0;}\\n' > {W}/eok.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/eo.c -o {W}/eo.o 2>&1; "
  "{MCC} -B{B} -I{I} -c {W}/ex.c -o {W}/ex.o 2>&1; "
  "{MCC} -B{B} -I{I} -Werror {W}/eok.c -o {W}/eok 2>&1 && echo VALID_OK; } | "
  "grep -oE 'octal escape sequence out of range|hex escape sequence out of range|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 VALID_OK\n1 hex escape sequence out of range\n1 octal escape sequence out of range\n" },

/* §6.7.6.3p10 (qualified lone 'void' param), §6.7.1p3 (_Thread_local + typedef),
   §6.7.6.2p2 (extern/linkage on a VLA type) — each rejected by gcc+clang, while
   the valid neighbors (plain void param, _Thread_local static, local VLA) still
   compile. */
{ "decl_storage_type_constraints", "",
  "printf 'void f(const void);\\n' > {W}/dq.c && "
  "printf '_Thread_local typedef int T;\\n' > {W}/dt.c && "
  "printf 'void f(int n){ extern int a[n]; }\\n' > {W}/dv.c && "
  "printf 'void f(void); _Thread_local static int x;\\n"
  "void g(int n){int a[n]; a[0]=0; (void)a;}\\nint main(void){return 0;}\\n' > {W}/dok.c && "
  "{ for n in dq dt dv; do {MCC} -B{B} -I{I} -std=c11 -c {W}/$n.c -o {W}/$n.o 2>&1; done; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/dok.c -o {W}/dok.o 2>&1 && echo VALID_OK; } | "
  "grep -oE \"only parameter may not be qualified|'_Thread_local' used with 'typedef'|must have no linkage|VALID_OK\" | sort | uniq -c | sed 's/^ *//'",
  "1 '_Thread_local' used with 'typedef'\n1 VALID_OK\n1 must have no linkage\n1 only parameter may not be qualified\n" },

/* §6.10.8p2: a predefined macro name shall not be the subject of #define/#undef.
   The magic builtin tokens (__LINE__/__FILE__/__DATE__/__TIME__) now warn like
   gcc/clang; ordinary macro define/undef stays clean under -Werror. */
{ "builtin_macro_redefine", "",
  "printf '#define __LINE__ 5\\n#undef __FILE__\\nint x;\\n' > {W}/bm.c && "
  "printf '#define FOO 1\\n#undef FOO\\nint y;\\n' > {W}/bmok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/bm.c -o {W}/bm.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/bmok.c -o {W}/bmok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE '__LINE__ redefined|undefining __FILE__|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 __LINE__ redefined\n1 undefining __FILE__\n" },

/* §6.5.4 (a cast does not yield an lvalue) and §6.5.17 (the comma operator does
   not yield an lvalue): `(int)a=9`, `&(int)a`, `(a,b)=7`, `&(a,b)` are all
   constraint violations (gcc+clang reject), while using the cast/comma result as
   an rvalue still compiles. */
{ "lvalue_cast_comma_constraints", "",
  "printf 'int f(int a){ (int)a = 9; return a; }\\n' > {W}/l1.c && "
  "printf 'int f(int a){ return *&(int)a; }\\n' > {W}/l2.c && "
  "printf 'int f(int a,int b){ (a,b) = 7; return b; }\\n' > {W}/l3.c && "
  "printf 'int f(int a,int b){ return *&(a,b); }\\n' > {W}/l4.c && "
  "printf 'int f(int a,int b){ int c=(int)a+1; c=(a,b); return c; }\\nint main(void){return 0;}\\n' > {W}/lok.c && "
  "{ for n in l1 l2 l3 l4; do {MCC} -B{B} -I{I} -c {W}/$n.c -o {W}/$n.o 2>&1; done; "
  "{MCC} -B{B} -I{I} {W}/lok.c -o {W}/lok 2>&1 && echo VALID_OK; } | "
  "grep -oE 'lvalue expected|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 VALID_OK\n4 lvalue expected\n" },

/* §6.10.3.3p3: a ## that forms a comment introducer ('//' or '/*') is not a
   valid preprocessing token; mcc must diagnose and terminate (it used to run the
   comment scanner off the synthetic paste buffer and loop forever). The
   `timeout` guards against a hang regression; a valid paste still works. */
{ "paste_comment_introducer", "",
  "printf '#define C(a,b) a ## b\\nC(/,/)\\n' > {W}/pc1.c && "
  "printf '#define C(a,b) a ## b\\nC(/,*)\\n' > {W}/pc2.c && "
  "printf '#define C(a,b) a ## b\\nint C(foo,bar)=5;\\nint main(void){return foobar-5;}\\n' > {W}/pcok.c && "
  "{ timeout 10 {MCC} -B{B} -I{I} -std=c11 -E -P {W}/pc1.c 2>&1; "
  "timeout 10 {MCC} -B{B} -I{I} -std=c11 -E -P {W}/pc2.c 2>&1; "
  "{MCC} -B{B} -I{I} {W}/pcok.c -o {W}/pcok 2>&1 && echo VALID_OK; } | "
  "grep -oE 'invalid preprocessing token|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 VALID_OK\n2 invalid preprocessing token\n" },

/* §7.1.4p1 / §7.3.9.5-6: creal/cimag are functions that may also be macros;
   `(creal)(z)` (parenthesized, function) and `&creal` must work alongside the
   fast `creal(z)` macro. (Previously creal/cimag were macro-only.) */
{ "complex_creal_function", "cpu=x86_64,os=linux",
  "printf '#include <complex.h>\\n#include <stdio.h>\\n"
  "int main(void){ double _Complex z=3.0+4.0*I; double(*p)(double _Complex)=creal;\\n"
  "if((int)(creal)(z)==3 && (int)cimag(z)==4 && (int)p(z)==3) puts(\\\"OK\\\"); return 0; }\\n' > {W}/cf.c && "
  "{MCC} -B{B} -I{I} {W}/cf.c -lm -o {W}/cf && {W}/cf",
  "OK\n" },

/* §7.28: <uchar.h> provides char16_t/char32_t/mbstate_t both hosted (via the
   system header) and freestanding (-nostdinc, via the bundled fallback). */
{ "uchar_header", "cpu=x86_64,os=linux",
  "printf '#include <uchar.h>\\nint main(void){char16_t a=0; char32_t b=0; mbstate_t s;\\n"
  "(void)a;(void)b;(void)s; return (sizeof(char16_t)==2 && sizeof(char32_t)==4)?0:1;}\\n' > {W}/uh.c && "
  "{MCC} -B{B} -I{I} {W}/uh.c -o {W}/uh && {W}/uh && echo HOSTED_OK && "
  "{MCC} -B{B} -nostdinc -I{I} {W}/uh.c -o {W}/uhf && {W}/uhf && echo FREE_OK",
  "HOSTED_OK\nFREE_OK\n" },

/* §5.2.1.1 / translation phase 1: trigraphs are processed in a strict ISO mode
   (-std=c11) like gcc/clang, but NOT in -std=gnu11 (or the default). */
{ "trigraphs_strict_std", "",
  "printf 'int a??(2??);\\n' > {W}/tg.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -E -P {W}/tg.c 2>&1; "
  "{MCC} -B{B} -I{I} -std=gnu11 -E -P {W}/tg.c 2>&1; } | "
  "grep -oE 'a\\?\\?\\(|a\\[2\\]' | sort | uniq -c | sed 's/^ *//'",
  "1 a??(\n1 a[2]\n" },

/* §6.10.3p4 (pre-C23): invoking a variadic macro with no argument for the '...'
   is diagnosed under -pedantic (warning) / -pedantic-errors (error); a call that
   supplies the variadic argument stays clean even under -pedantic-errors. */
{ "va_args_empty_pedantic", "",
  "printf '#define V(f,...) f\\nint a = V(1);\\n' > {W}/ve.c && "
  "printf '#define V(f,...) f\\nint b = V(1,2);\\nint main(void){return 0;}\\n' > {W}/vok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/ve.c -o {W}/ve.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/vok.c -o {W}/vok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'no argument for the|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 no argument for the\n" },

/* §7.17.8: atomic_flag_* take volatile atomic_flag *, so a non-pointer argument
   is diagnosed; the correct &atomic_flag form compiles clean under -Werror. */
{ "atomic_flag_type", "",
  "printf '#include <stdatomic.h>\\nvoid f(void){int x=0; atomic_flag_clear(x);}\\n' > {W}/aft.c && "
  "printf '#include <stdatomic.h>\\nvoid g(void){atomic_flag a=ATOMIC_FLAG_INIT; atomic_flag_clear(&a);}\\n' > {W}/afok.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/aft.c -o {W}/aft.o 2>&1; "
  "{MCC} -B{B} -I{I} -Werror -c {W}/afok.c -o {W}/afok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'pointer from integer|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 pointer from integer\n" },

/* §6.2.2p7: a static declaration following a non-static (extern) one is an error
   (both internal+external linkage = UB); the reverse (extern after static) keeps
   internal linkage and is well-formed. */
{ "linkage_static_after_extern", "",
  "printf 'extern int x; static int x;\\n' > {W}/les.c && "
  "printf 'static int y; extern int y;\\n' > {W}/lse.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/les.c -o {W}/les.o 2>&1; "
  "{MCC} -B{B} -I{I} -Werror -c {W}/lse.c -o {W}/lse.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'follows non-static|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 follows non-static\n" },

};
static const int cli_cases_count = (int)(sizeof(cli_cases)/sizeof(cli_cases[0]));

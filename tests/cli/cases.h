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

/* §6.10.8.3 (C11): __STDC_UTF_16__/__STDC_UTF_32__ are predefined to 1 because
   char16_t/char32_t are UTF-16/UTF-32 on every mcc target. gcc/clang predefine
   them in default mode on all targets (Windows included); mcc must too. */
{ "stdc_utf_encoding_macros", "",
  "printf '\\n' > {W}/utf.c && {MCC} -B{B} -E -dM {W}/utf.c | grep -cE '^#define __STDC_UTF_(16|32)__ 1$'",
  "2\n" },

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

/* §6.4.4.4p10: a multi-character character constant warns by default (like
   gcc/clang -Wmultichar), not only under -Wall. */
{ "multichar_warning", "",
  "{MCC} -B{B} -I{I} -c {D}/multichar.c -o {W}/mc.o 2>&1 | grep -oE 'multi-character'",
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

/* 6.5.1.1p2: a _Generic association type must be a complete object type other
   than a variably modified type. A VLA association is an error (gcc+clang both
   hard-error); a function-type association is diagnosed under -pedantic (gcc's
   stance; clang errors). A normal association compiles clean under -Werror. */
{ "generic_assoc_type_completeness", "",
  "printf 'void h(int n){(void)_Generic(1,int[n]:1,default:2);(void)n;}\\n' > {W}/gv.c && "
  "printf 'int x=_Generic(1,int(void):1,int:2,default:3);\\n' > {W}/gf.c && "
  "printf 'int f(void){return _Generic(1,int:0,double:1,default:2);}\\n' > {W}/gok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/gv.c -o {W}/gv.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/gf.c -o {W}/gf.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/gok.c -o {W}/gok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'variably modified type|association with a function type|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 association with a function type\n1 variably modified type\n" },

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
  "printf 'int n=3; int p(void){ goto L; int (*q)[n]; L: q=0; return !!q; }\\n' > {W}/j5.c && "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/j5.c -o {W}/j5.o 2>&1 | "
  "grep -oE 'variably modified declaration'; "
  "printf 'int n=3;\\nint ok(int c){ int a[n]; L: if(a[0]) goto L;"
  " switch(c){ case 1: { int b[n]; return sizeof b; } default: return sizeof a; } }\\n"
  "int main(void){return 0;}\\n' > {W}/j4.c && "
  "{MCC} -B{B} -I{I} -std=c11 {W}/j4.c -o {W}/j4 && echo VALID_OK",
  "variably modified declaration\nvariably modified declaration\nvariably modified declaration\nvariably modified declaration\nVALID_OK\n" },

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

/* §6.10.3.3p3: a ## that forms a comment introducer (a line- or block-comment
   opener) is not a
   valid preprocessing token; mcc must diagnose and terminate (it used to run the
   comment scanner off the synthetic paste buffer and loop forever). The
   {TIMEOUT} guards against a hang regression where available; a valid paste
   still works. */
{ "paste_comment_introducer", "",
  "printf '#define C(a,b) a ## b\\nC(/,/)\\n' > {W}/pc1.c && "
  "printf '#define C(a,b) a ## b\\nC(/,*)\\n' > {W}/pc2.c && "
  "printf '#define C(a,b) a ## b\\nint C(foo,bar)=5;\\nint main(void){return foobar-5;}\\n' > {W}/pcok.c && "
  "{ {TIMEOUT}{MCC} -B{B} -I{I} -std=c11 -E -P {W}/pc1.c 2>&1; "
  "{TIMEOUT}{MCC} -B{B} -I{I} -std=c11 -E -P {W}/pc2.c 2>&1; "
  "{MCC} -B{B} -I{I} {W}/pcok.c -o {W}/pcok 2>&1 && echo VALID_OK; } | "
  "grep -oE 'invalid preprocessing token|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 VALID_OK\n2 invalid preprocessing token\n" },

/* §6.10.3.2p2: stringizing an argument ending in a stray backslash would have
   the trailing '\' escape the closing quote, yielding an invalid string literal.
   mcc now drops the dangling backslash and warns (matching gcc -> "a"), instead
   of silently emitting "a\" (which then failed with "unknown escape sequence").
   An even backslash run and a normal argument stringize cleanly. */
{ "stringize_trailing_backslash", "",
  "printf '#define S(x) #x\\nconst char *p = S(a\\\\);\\nint main(void){return p[0];}\\n' > {W}/sb.c && "
  "printf '#define S(x) #x\\nconst char *a=S(hi);const char *b=S(a\\\\\\\\);"
  "int main(void){return a[0]+b[0];}\\n' > {W}/sbok.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/sb.c -o {W}/sb.o 2>&1; "
  "{MCC} -B{B} -I{I} -Werror -c {W}/sbok.c -o {W}/sbok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'ignoring final|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 ignoring final\n" },

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
  "printf 'int a?" "?(2?" "?);\\n' > {W}/tg.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -E -P {W}/tg.c 2>&1; "
  "{MCC} -B{B} -I{I} -std=gnu11 -E -P {W}/tg.c 2>&1; } | "
  "grep -oE 'a\\?\\?\\(|a\\[2\\]' | sort | uniq -c | sed 's/^ *//'",
  "1 a?" "?(\n1 a[2]\n" },

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

/* §6.10.1: the preprocessor evaluates #if arithmetic in intmax_t; signed
   overflow there is UB and is diagnosed (gcc/clang warn the same). A sum that
   stays within 64-bit (e.g. INT_MAX+1) and unsigned wraparound are silent. */
{ "pp_if_integer_overflow", "",
  "printf '#if 9223372036854775807 + 1 < 0\\nint a;\\n#endif\\nint main(void){return 0;}\\n' > {W}/po.c && "
  "printf '#if 9223372036854775807 * 2 < 0\\nint b;\\n#endif\\nint main(void){return 0;}\\n' > {W}/pm.c && "
  "printf '#if 2147483647 + 1 > 0 && 18446744073709551615U + 1U == 0U\\nint c;\\n#endif\\n"
  "int main(void){return 0;}\\n' > {W}/pok.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/po.c -o {W}/po.o 2>&1; "
  "{MCC} -B{B} -I{I} -c {W}/pm.c -o {W}/pm.o 2>&1; "
  "{MCC} -B{B} -I{I} -Werror -c {W}/pok.c -o {W}/pok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'overflow in preprocessor|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n2 overflow in preprocessor\n" },

/* §6.10.4p3: the #line digit sequence shall not be zero nor exceed 2147483647.
   Out-of-range and zero are diagnosed under -pedantic; the carried __LINE__ is
   clamped to INT_MAX rather than wrapping negative (silent without -pedantic).
   A valid line number is clean even under -pedantic-errors. */
{ "line_number_out_of_range", "",
  "printf '#line 2147483648\\nint x;\\n#line 0\\nint y;\\nint main(void){return 0;}\\n' > {W}/lr.c && "
  "printf '#line 100\\nint z;\\nint main(void){return 0;}\\n' > {W}/lrok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/lr.c -o {W}/lr.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/lrok.c -o {W}/lrok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'line number out of range|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n2 line number out of range\n" },

/* §6.7.9p14: a string literal can initialize an array only when its element
   type matches the array's element type; a wide/narrow mismatch is rejected
   with a clear message (not the old "integer from pointer" + "',' expected"
   cascade). Matching narrow/wide string inits, a braced string, and a nested
   sub-array string element stay valid even under -Werror. */
{ "string_init_element_mismatch", "os!=WIN32:int[]=L\"\" is clean only where wchar_t==int; PE wchar_t is 16-bit so it warns",
  "printf 'int a[4]=\"abc\";\\n' > {W}/sm.c && "
  "printf 'int wmain(void){char a[]=L\"abc\";return a[0];}\\n' > {W}/sm2.c && "
  "printf 'char a[]=\"abc\"; int b[]=L\"abc\"; char c[4]={\"ab\"};"
  " char m[][3]={\"ab\",\"cd\"}; int main(void){return a[0]+b[0]+c[0]+m[0][0];}\\n' > {W}/smok.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/sm.c -o {W}/sm.o 2>&1; "
  "{MCC} -B{B} -I{I} -c {W}/sm2.c -o {W}/sm2.o 2>&1; "
  "{MCC} -B{B} -I{I} -Werror -c {W}/smok.c -o {W}/smok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'from a string literal of a different|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n2 from a string literal of a different\n" },

/* §6.6p6: a file-scope array size that only folds to a constant through a
   floating operation is not an integer constant expression; it is diagnosed
   under -pedantic (a folded-VLA extension, like gcc/clang). A floating constant
   as the immediate operand of a cast ((int)3.0, (int)1.5+(int)2.5) IS a valid
   ICE and stays silent even under -pedantic-errors; block-scope arrays fold
   silently (VLAs are allowed there) — matching both refs. */
{ "float_derived_array_size", "",
  "printf 'int a[(int)(1.0+2.0)]; int b[(1.0<2.0)?4:2];\\n' > {W}/fd.c && "
  "printf 'int c[(int)3.0]; int d[(int)1.5+(int)2.5]; int e[3+4];\\n"
  "int g(void){int z[(int)(1.0+2.0)]; return sizeof z;}\\n"
  "int main(void){return 0;}\\n' > {W}/fdok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/fd.c -o {W}/fd.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/fdok.c -o {W}/fdok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'not an integer constant expression|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n2 not an integer constant expression\n" },

/* §6.8.1: a label (named, case, or default) prefixes a statement; a declaration
   is not a statement before C23, so `L: int x;` is diagnosed under -pedantic
   (matching gcc/clang -Wfree-labels), including a typedef-name declaration. It is
   legal in C23 (silent under -std=c23) and a label-then-statement stays clean. */
{ "label_then_declaration", "",
  "printf 'typedef int T;\\nint f(int c){switch(c){case 1: int z=0; return z; default: return 1;}}\\n"
  "int g(void){ L: int y=0; return y; }\\nint h(void){ M: T t=0; return t; }\\n' > {W}/lab.c && "
  "printf 'int f(void){ L: return 0; }\\n' > {W}/labok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/lab.c -o {W}/lab.o 2>&1 | grep -c 'declaration is not a statement'; "
  "{MCC} -B{B} -I{I} -std=c23 -c {W}/lab.c -o {W}/lab23.o 2>&1 | grep -c 'declaration is not a statement'; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/labok.c -o {W}/labok.o 2>&1 && echo CLEAN_OK; }",
  "3\n0\nCLEAN_OK\n" },

/* §6.7.1/§6.5.3.2: the address of a 'register'-qualified parameter may not be
   taken (now tracked on parameters, not just locals); a non-register parameter
   is fine. (This same tracking drives the §7.16.1.4 va_start-on-register warning
   on the token-va_start targets — verified cross-arch, not here.) */
{ "register_param_address", "",
  "printf 'int f(register int n){ int *p=&n; return *p; }\\n' > {W}/rp.c && "
  "printf 'int g(int n){ int *p=&n; return *p; }\\n' > {W}/rpok.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/rp.c -o {W}/rp.o 2>&1; "
  "{MCC} -B{B} -I{I} -Werror -c {W}/rpok.c -o {W}/rpok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'address of register variable|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 address of register variable\n" },

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

/* §6.7.4p3: an inline definition of an external-linkage function shall not
   define a modifiable static-duration object. A non-const static local in such
   a function warns (gcc/clang agree); a const static local, a static (internal-
   linkage) function, and an extern-inline function are all valid. */
{ "inline_extern_static_object", "",
  "printf 'inline int counter(void){ static int n; return ++n; }\\n"
  "int(*p)(void)=counter;\\n' > {W}/ie.c && "
  "printf 'inline int a(void){static const int c=5;return c;}\\n"
  "static inline int b(void){static int m;return ++m;}\\n"
  "extern inline int d(void){static int k;return ++k;}\\n"
  "int main(void){return a();}\\n' > {W}/ieok.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/ie.c -o {W}/ie.o 2>&1; "
  "{MCC} -B{B} -I{I} -Werror -c {W}/ieok.c -o {W}/ieok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'static but declared in inline|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 static but declared in inline\n" },

/* §6.5.2.2: a by-value struct returned from a call is not a modifiable lvalue,
   so `g().m = x` and `&g().m` are errors; reading the member, copying the whole
   result, and assigning through a returned *pointer* (`gp()->m = x`) stay valid. */
{ "rvalue_struct_member", "",
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
  "1 VALID_OK\n1 address of a function-call\n1 is not assignable\n" },

/* §6.7.1p2: at most one storage-class specifier — auto/register conflict with
   each other and with static/extern/typedef. A single register param, a
   block-scope auto, and static/extern/typedef alone stay valid. */
{ "storage_class_exclusivity", "",
  "printf 'static auto int a;\\n' > {W}/sc1.c && "
  "printf 'register static int b;\\n' > {W}/sc2.c && "
  "printf 'auto auto int c;\\n' > {W}/sc3.c && "
  "printf 'void f(register int n){(void)n; auto int x=5; (void)x;}\\n"
  "static int s; extern int e; typedef int T;\\nint main(void){return 0;}\\n' > {W}/scok.c && "
  "{ for n in sc1 sc2 sc3; do {MCC} -B{B} -I{I} -c {W}/$n.c -o {W}/$n.o 2>&1; done; "
  "{MCC} -B{B} -I{I} {W}/scok.c -o {W}/scok 2>&1 && echo VALID_OK; } | "
  "grep -oE 'multiple storage classes|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 VALID_OK\n3 multiple storage classes\n" },

/* §6.7.6.3p7: `static` in an array parameter requires a size operand; `[static]`
   alone is an error, while `[static N]`, `[const N]`, `[]` stay valid. */
{ "array_static_param", "",
  "printf 'void f(int a[static]);\\n' > {W}/ap.c && "
  "printf 'void g(int a[static 3]); void h(int a[const 2]); void i(int a[]);\\n"
  "int main(void){return 0;}\\n' > {W}/apok.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/ap.c -o {W}/ap.o 2>&1; "
  "{MCC} -B{B} -I{I} -Werror -c {W}/apok.c -o {W}/apok.o 2>&1 && echo VALID_OK; } | "
  "grep -oE 'without an array size|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 VALID_OK\n1 without an array size\n" },

/* §5.1.1.2: `mcc -E` must terminate on an identifier followed by `\`+space+
   newline (it used to spin forever re-reading the stray `\`). The {TIMEOUT}
   guard guards against a hang regression; rc 0 = clean termination. {TIMEOUT}
   expands to nothing where coreutils `timeout` is absent (macOS/BSD), leaving
   ctest's own per-test timeout as the backstop. */
{ "ident_backslash_no_hang", "",
  "printf 'a\\\\ \\nb\\n' > {W}/ib.c && "
  "{TIMEOUT}{MCC} -B{B} -I{I} -E -P {W}/ib.c >/dev/null 2>&1 && echo TERMINATED",
  "TERMINATED\n" },

/* §6.10.8.1/§5.1.2.1: -ffreestanding makes __STDC_HOSTED__ expand to 0 (and
   -fhosted / the default keep it 1), matching gcc/clang. */
{ "freestanding_hosted_macro", "",
  "printf '__STDC_HOSTED__\\n' > {W}/fh.c && "
  "{ {MCC} -B{B} -I{I} -ffreestanding -E -P {W}/fh.c; "
  "{MCC} -B{B} -I{I} -ffreestanding -fhosted -E -P {W}/fh.c; "
  "{MCC} -B{B} -I{I} -E -P {W}/fh.c; } | tr -d ' '",
  "0\n1\n1\n" },

/* §6.7.2.1: an empty struct/union (no members) is a GNU extension diagnosed
   under -pedantic; a named member or an anonymous struct/union member keeps it
   valid even under -pedantic-errors. */
{ "empty_struct_pedantic", "",
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
  "1 VALID_OK\n4 no named members\n" },

/* §6.9p1: a stray ';' at file scope (empty external declaration) is a GNU
   extension diagnosed under -pedantic; a ';' inside a function is a valid null
   statement (silent even under -pedantic-errors). */
{ "empty_declaration_pedantic", "",
  "printf 'int x;;\\n' > {W}/ed.c && "
  "printf 'int f(void){ ; ; return 0; }\\n' > {W}/edok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/ed.c -o {W}/ed.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/edok.c -o {W}/edok.o 2>&1 && echo VALID_OK; } | "
  "grep -oE 'empty declaration|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 VALID_OK\n1 empty declaration\n" },

/* §6.5.6: arithmetic on a 'void *' or a function pointer (++, +/- int, and
   pointer-pointer subtraction) is a GNU extension diagnosed under -pedantic;
   object-pointer arithmetic stays clean even under -pedantic-errors. */
{ "void_fn_pointer_arith", "",
  "printf 'void f(void*p){p++;} long g(void*a,void*b){return a-b;}\\n"
  "void h(int(*fp)(void)){fp++;}\\n' > {W}/pa.c && "
  "printf 'int f(int*p){return *(p+1);} long g(int*a,int*b){return a-b;}\\n"
  "char*h(char*p){return p+3;}\\n' > {W}/paok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/pa.c -o {W}/pa.o 2>&1 | grep -c 'forbids arithmetic'; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/paok.c -o {W}/paok.o 2>&1 && echo VALID_OK; }",
  "3\nVALID_OK\n" },

/* §6.3.2.3: implicit conversion between a function pointer and 'void *' is a GNU
   extension diagnosed under -pedantic; void*<->object* and an explicit cast
   stay valid even under -pedantic-errors. */
{ "fn_pointer_void_conversion", "",
  "printf 'void*f(int(*fp)(void)){void*v=fp;return v;}\\n' > {W}/c1.c && "
  "printf 'int(*g(void*v))(void){int(*fp)(void)=v;return fp;}\\n' > {W}/c2.c && "
  "printf 'void*f(int*p){void*v=p;return v;} void*g(int(*fp)(void)){return (void*)fp;}\\n' > {W}/cok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/c1.c -o {W}/c1.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic -c {W}/c2.c -o {W}/c2.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/cok.c -o {W}/cok.o 2>&1 && echo VALID_OK; } | "
  "grep -oE 'function pointer and|VALID_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 VALID_OK\n2 function pointer and\n" },

/* §6.7p3: redefining a typedef with the same type is a C11 feature; in C99 it
   is diagnosed under -pedantic but accepted silently in C11. An incompatible
   redefinition is always an error. */
{ "typedef_redefinition_c99", "",
  "printf 'typedef int T; typedef int T;\\n' > {W}/td.c && "
  "printf 'typedef int U; typedef long U;\\n' > {W}/tdbad.c && "
  "{ {MCC} -B{B} -I{I} -std=c99 -pedantic -c {W}/td.c -o {W}/td.o 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/td.c -o {W}/td2.o 2>&1 && echo C11_OK; "
  "{MCC} -B{B} -I{I} -c {W}/tdbad.c -o {W}/tdbad.o 2>&1; } | "
  "grep -oE 'redefinition of typedef is a C11 feature|incompatible redefinition|C11_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 C11_OK\n1 incompatible redefinition\n1 redefinition of typedef is a C11 feature\n" },

/* §6.7.2.1 + system-header suppression: an anonymous struct/union member is a
   C11 feature diagnosed under -pedantic in user code, but the same construct in
   a system header (-isystem) is suppressed (only the user's m.c warns -> 1); in
   C11 it is silent everywhere. */
{ "anon_member_c99_and_sysheader", "",
  "mkdir -p {W}/sys && printf 'struct H{ struct { int x; }; };\\n' > {W}/sys/h.h && "
  "printf '#include <h.h>\\nstruct U{ struct { int y; }; }; int main(void){return 0;}\\n' > {W}/m.c && "
  "{ {MCC} -B{B} -I{I} -isystem {W}/sys -std=c99 -pedantic -c {W}/m.c -o {W}/m.o 2>&1 | grep -c 'C11 feature'; "
  "{MCC} -B{B} -I{I} -isystem {W}/sys -std=c11 -pedantic-errors -c {W}/m.c -o {W}/m11.o 2>&1 && echo C11_OK; }",
  "1\nC11_OK\n" },

/* §6.7.2.1 regression: -pedantic-errors at the default (C99) std must not abort
   on mcc's own predef/<command line> buffer (the va_list anon union). The
   suppression in mcc_pedantic covers the -pedantic-errors path too, so a clean
   file compiles — while genuine user-code anon members are still caught. */
{ "pedantic_errors_predef_clean", "",
  "printf 'int main(void){return 0;}\\n' > {W}/pe.c && "
  "printf 'struct S{struct{int x;};}; int main(void){return 0;}\\n' > {W}/peb.c && "
  "{ {MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/pe.c -o {W}/pe.o 2>&1 && echo CLEAN_OK; "
  "{MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/peb.c -o {W}/peb.o 2>&1; } | "
  "grep -oE 'C11 feature|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 C11 feature\n1 CLEAN_OK\n" },

/* §6.5.16.1: adding a qualifier at a nested pointer level (`int** -> const
   int**`) launders it away and is an incompatible-pointer constraint violation;
   a top-level qualifier add (`int* -> const int*`) is allowed, a top-level
   discard (`const int* -> int*`) warns "discards qualifiers". */
{ "nested_pointer_qualifier_launder", "",
  "printf 'void f(int **p){ const int **q = p; (void)q; }\\n' > {W}/nl.c && "
  "printf 'void f(int *ip, const int *cp){ const int *a = ip; int *b = cp; (void)a;(void)b; }\\n' > {W}/nlmix.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/nl.c -o {W}/nl.o 2>&1 | grep -oE 'incompatible pointer'; "
  "{MCC} -B{B} -I{I} -c {W}/nlmix.c -o {W}/nlmix.o 2>&1 | grep -oE 'discards|incompatible'; } | "
  "sort | uniq -c | sed 's/^ *//'",
  "1 discards\n1 incompatible pointer\n" },

/* §6.10.3p6 (duplicate macro parameter), §6.10.8p4 (`defined`/`__VA_ARGS__`
   may not be a macro name), §6.10.8p2 (the `__STDC__` predef family is
   protected from #undef). Each is a constraint violation both gcc and clang
   diagnose; a normal object/variadic macro and #define/#undef stay clean. */
{ "pp_macro_name_constraints", "",
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
  "1 CLEAN_OK\n1 duplicate macro parameter\n2 invalid macro name\n1 undefining __STDC__\n" },

/* §6.5.3.4: the ISO `_Alignof` rejects an expression operand and a void type
   (gcc/clang both reject); `sizeof(void)` is the GNU 1-byte extension diagnosed
   under -pedantic. The GNU `__alignof__` spelling permits both an expression
   and a void operand, so it stays clean. */
{ "sizeof_alignof_void", "",
  "printf 'int a=_Alignof(void);\\n' > {W}/sav.c && "
  "printf 'int f(int x){return _Alignof(x);}\\n' > {W}/sae.c && "
  "printf 'unsigned long g(void){return sizeof(void);}\\n' > {W}/ssv.c && "
  "printf 'int h(int x){return __alignof__(x)+__alignof__(void);}\\nint main(void){return 0;}\\n' > {W}/sgnu.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/sav.c -o {W}/sav.o 2>&1; "
  "{MCC} -B{B} -I{I} -pedantic-errors -c {W}/sae.c -o {W}/sae.o 2>&1; "
  "{MCC} -B{B} -I{I} -pedantic-errors -c {W}/ssv.c -o {W}/ssv.o 2>&1; "
  "{MCC} -B{B} -I{I} -Werror -c {W}/sgnu.c -o {W}/sgnu.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE '_Alignof. applied to a void|sizeof. applied to a void|applied to an expression|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 _Alignof' applied to a void\n1 applied to an expression\n1 sizeof' applied to a void\n" },

/* §6.9.1p4: a function definition's declaration specifiers may specify only
   extern/static; `typedef` is a constraint violation (gcc/clang reject). A
   typedef of a function *type* (no body) stays valid. */
{ "function_def_typedef", "",
  "printf 'typedef int f(void){return 42;}\\n' > {W}/ft.c && "
  "printf 'typedef int myf(void); int g(void){return 1;}\\nint main(void){return 0;}\\n' > {W}/ftok.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/ft.c -o {W}/ft.o 2>&1; "
  "{MCC} -B{B} -I{I} -Werror -c {W}/ftok.c -o {W}/ftok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'function definition declared|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 function definition declared\n" },

/* §6.7.2.1p18 (initializing a struct flexible array member) and §6.7.9p11
   (a second brace level around a scalar, `int x={{1}}`) are ISO violations
   gcc/clang diagnose. A FAM left uninitialized, a single brace around a scalar
   member, a top-level incomplete-array init, and a braced scalar all stay
   clean under -pedantic. */
{ "init_brace_constraints", "",
  "printf 'struct V{int len;int data[];}; struct V v={1,{2,3}};\\n' > {W}/fam.c && "
  "printf 'int x={{1}};\\n' > {W}/sb.c && "
  "printf 'struct V{int len;int data[];}; struct V v={1};\\nstruct S{int a;}; struct S s={{5}};\\nint y={7}; int a3[]={1,2,3};\\nint main(void){return 0;}\\n' > {W}/iok.c && "
  "{ {MCC} -B{B} -I{I} -pedantic-errors -c {W}/fam.c -o {W}/fam.o 2>&1; "
  "{MCC} -B{B} -I{I} -pedantic-errors -c {W}/sb.c -o {W}/sb.o 2>&1; "
  "{MCC} -B{B} -I{I} -Werror -pedantic -c {W}/iok.c -o {W}/iok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'flexible array member|too many braces around scalar|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 flexible array member\n1 too many braces around scalar\n" },

/* §6.5.3.2: indirection on a `void *` yields a void lvalue of incomplete type,
   which ISO C does not allow (gcc warns, clang errors); diagnosed under
   -pedantic. A real-typed dereference stays clean. */
{ "void_pointer_deref", "",
  "printf 'void f(void*p){*p;}\\n' > {W}/vd.c && "
  "printf 'void g(int*p){*p=1; (void)*p;}\\nint main(void){return 0;}\\n' > {W}/vdok.c && "
  "{ {MCC} -B{B} -I{I} -pedantic-errors -c {W}/vd.c -o {W}/vd.o 2>&1; "
  "{MCC} -B{B} -I{I} -Werror -pedantic -c {W}/vdok.c -o {W}/vdok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'dereferencing a .void|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 dereferencing a 'void\n" },

/* §6.6p4: a constant expression shall evaluate to a value representable in its
   type; a signed +, -, or * fold that overflows its result type is diagnosed by
   gcc/clang. Mirrored under -pedantic. In-range folds, unsigned wraparound, and
   a product that fits a wider result stay clean. */
{ "const_integer_overflow", "",
  "printf 'int x = 100000 * 100000;\\n' > {W}/co.c && "
  "printf 'int a = 2000000000 + 100000000;\\nunsigned b = 4000000000u + 1000000000u;\\nlong long c = 2000000000LL * 2000000000LL;\\nint main(void){return 0;}\\n' > {W}/cok.c && "
  "{ {MCC} -B{B} -I{I} -pedantic-errors -c {W}/co.c -o {W}/co.o 2>&1; "
  "{MCC} -B{B} -I{I} -Werror -pedantic -c {W}/cok.c -o {W}/cok.o 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'integer overflow in constant expression|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 integer overflow in constant expression\n" },

/* §6.10.8.1: __LINE__ as a multi-line macro argument expands to the line of its
   own token (here line 3), not the line of the invocation's closing ')'. */
{ "line_macro_arg", "",
  "printf '#define ID(x) x\\nint a=ID(\\n__LINE__\\n);\\nint main(void){return a;}\\n' > {W}/lma.c && "
  "{MCC} -B{B} -I{I} {W}/lma.c -o {W}/lma && {W}/lma; echo L=$?",
  "L=3\n" },

/* §6.4.5p2: a u8 literal cannot concatenate with a wide (L/u/U) literal; u8 with
   a plain narrow literal is fine. The valid forms compile and run (RUN=3 = the
   sizeof("hi")+0+0+0 self-check). */
{ "u8_string_concat", "",
  "printf 'const void*p=L\"a\" u8\"b\";\\n' > {W}/u8c.c && "
  "printf 'char a[]=u8\"hi\"; const char*b=u8\"x\" \"y\"; const char*c=\"p\" u8\"q\";\\n"
  "int main(void){return sizeof(a)+(a[0]!=0x68)+(b[0]!=0x78)+(c[1]!=0x71);}\\n' > {W}/u8ok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/u8c.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Werror {W}/u8ok.c -o {W}/u8ok 2>&1 && {W}/u8ok; echo RUN=$?; } | "
  "grep -oE 'concatenation of string literals|RUN=3'",
  "concatenation of string literals\nRUN=3\n" },

/* §6.7.6.2: a pointer-to-VLA parameter `int (*a)[m]` keeps its VLA dimension —
   sizeof(b[0])==16 and a[1][0] indexes the right element (self-check returns 7). */
{ "pointer_to_vla_param", "",
  "printf '#include <stddef.h>\\nvoid f(int m,int(*a)[m]){a[1][0]=42;}\\n"
  "int main(void){int b[3][4]={0}; f(4,b); size_t s=sizeof(b[0]); return (b[1][0]==42 && s==16)?7:0;}\\n' > {W}/pvp.c && "
  "{MCC} -B{B} -I{I} {W}/pvp.c -o {W}/pvp && {W}/pvp; echo R=$?",
  "R=7\n" },

/* §6.6p6: a conditional with a non-constant operand in the discarded arm is not
   an integer constant expression even when the constant condition picks the other
   arm (`1?3:v`); a constant-only conditional stays a valid ICE. */
{ "conditional_ice", "",
  "printf 'int v; enum E{Z=1?3:v};\\n' > {W}/cic.c && "
  "printf 'enum F{A=1?3:4,B=1?3:(int)sizeof(int)};\\nint main(void){return A+B;}\\n' > {W}/cicok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/cic.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Werror -pedantic-errors -c {W}/cicok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'not an integer constant expression|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 not an integer constant expression\n" },

/* §6.7.9p17-20: after a designator descends into a sub-aggregate, positional
   initializers resume within it and brace-elide out when full (`.in[0]=1, 2, 3`
   → in={1,2}, t=3); independent `.member[k]` designators each apply without
   re-clearing. Self-check returns 9 when all members match gcc/clang. */
{ "designated_init_continuation", "",
  "printf 'struct O{int in[2];int t;};\\nstruct N{struct{int a,b;}s;int t;};\\nstruct F{int f1,f2;int fa[3];};\\n"
  "int main(void){\\n"
  "struct O o={.in[0]=1,2,3};\\n"
  "struct N n={.s.a=1,2,3};\\n"
  "struct F f={.f2=2,3,.f1=1,.fa[0]=10,.fa[1]=11,.fa[2]=12};\\n"
  "int ok=o.in[0]==1&&o.in[1]==2&&o.t==3 && n.s.a==1&&n.s.b==2&&n.t==3 "
  "&& f.f1==1&&f.f2==2&&f.fa[0]==10&&f.fa[1]==11&&f.fa[2]==12;\\n"
  "return ok?9:0;}\\n' > {W}/dic.c && "
  "{MCC} -B{B} -I{I} {W}/dic.c -o {W}/dic && {W}/dic; echo R=$?",
  "R=9\n" },

/* §6.5.1.1p2 (_Generic incomplete/void association), §6.7.2.4p3 (_Atomic on a
   qualified type), §6.7.3p2 (restrict on a non-pointer array). Each diagnosed;
   valid _Generic/_Atomic/restrict forms stay clean under -Werror. */
{ "generic_atomic_restrict_constraints", "",
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
  "1 CLEAN_OK\n1 _Atomic cannot be applied to a qualified\n2 _Generic' association with an incomplete\n1 requires a pointer type\n" },

/* §6.7.6.3p2 (storage class other than register on a parameter), §6.7.1p7
   (auto/register on a block-scope function declaration). register param and
   extern block-scope function stay clean. */
{ "param_and_blockfn_storage", "",
  "printf 'void f(static int x);\\n' > {W}/ps.c && "
  "printf 'void g(void){ register int h(void); (void)h; }\\n' > {W}/bf.c && "
  "printf 'void f(register int x){(void)x;}\\nvoid g(void){ extern int h(void); (void)h; }\\nint main(void){return 0;}\\n' > {W}/sok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/ps.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/bf.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/sok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'storage class specified for parameter|invalid storage class for function|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 invalid storage class for function\n1 storage class specified for parameter\n" },

/* §6.5.15p3 (one void operand in ?:), §6.5.4 (float<->pointer constant cast, both
   directions), §6.5.16.1p1 (assignment to a struct with a const member). */
{ "expr_constraints", "",
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
  "1 CLEAN_OK\n1 assignment of read-only\n2 cast between a floating type and a pointer\n1 only one void operand\n" },

/* §6.6p6: a bit-field width / enumerator value / case label that only folds via
   a floating operation is not an integer constant expression. The immediate
   cast-of-float forms (`(int)2.5`) are valid ICEs and stay clean. */
{ "ice_float_constraints", "",
  "printf 'struct S{int b:(int)(2.5*2);};\\n' > {W}/ib.c && "
  "printf 'enum E{X=(int)(2.5*2)};\\n' > {W}/ie.c && "
  "printf 'int f(int x){switch(x){case (int)(2.5*2): return 1;} return 0;}\\n' > {W}/ic.c && "
  "printf 'struct S{int b:(int)2.5;}; enum E{X=(int)3.0,Y=5}; int f(int x){switch(x){case (int)3.9:return 1;}return 0;}\\nint main(void){return 0;}\\n' > {W}/iok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/ib.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/ie.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/ic.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Werror -pedantic-errors -c {W}/iok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'bit-field width that is not|enumerator value that is not|case label that is not|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 bit-field width that is not\n1 case label that is not\n1 enumerator value that is not\n" },

/* §6.7.9p4: the address of a block-scope (automatic) compound literal is not a
   constant, so it may not initialize a static-storage object. §6.4.3p2: a UCN
   with a code point < 00A0 is invalid pre-C23. File-scope/runtime compound
   literals and a valid UCN stay clean. */
{ "static_init_and_ucn", "",
  "printf 'void f(void){ static int *p=(int[]){10,20,30}; (void)p; }\\n' > {W}/sc.c && "
  "printf 'char *s=\\\"\\\\u0041\\\";\\n' > {W}/uc.c && "
  "printf 'int*p=(int[]){1,2,3}; void g(void){int*q=(int[]){4,5}; (void)q;}\\nint main(void){return 0;}\\n' > {W}/nok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/sc.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/uc.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Werror -pedantic-errors -c {W}/nok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'initializer element is not constant|not a valid universal|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 initializer element is not constant\n1 not a valid universal\n" },

/* §7.20.4.1p1: INT64_C/UINT64_C shall have type int_least64_t/uint_least64_t.
   Verified hosted and freestanding (the bundled <stdint.h> fallback). */
{ "int64_c_type", "",
  "printf '#include <stdint.h>\\n"
  "_Static_assert(_Generic(INT64_C(1), int_least64_t:1, default:0)==1, \\\"t1\\\");\\n"
  "_Static_assert(_Generic(UINT64_C(1), uint_least64_t:1, default:0)==1, \\\"t2\\\");\\n"
  "int main(void){return 0;}\\n' > {W}/i64.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/i64.c -o /dev/null 2>&1 && echo HOSTED_OK; "
  "{MCC} -B{B} -I{I} -std=c11 -ffreestanding -nostdinc -Werror -c {W}/i64.c -o /dev/null 2>&1 && echo FREESTANDING_OK; } | sort",
  "FREESTANDING_OK\nHOSTED_OK\n" },

/* §6.7.2.1p11: consecutive _Bool bit-fields pack into one byte (ABI). */
{ "bool_bitfield_packing", "",
  "printf '#include <stdio.h>\\n"
  "struct B{_Bool a:1;_Bool b:1;};\\n"
  "struct C{_Bool a:1,b:1,c:1,d:1;};\\n"
  "int main(void){ struct B v={1,1}; printf(\\\"%%zu %%zu %%d%%d\\\\n\\\",sizeof(struct B),sizeof(struct C),v.a,v.b); return 0; }\\n' > {W}/bfp.c && "
  "{MCC} -B{B} -I{I} {W}/bfp.c -o {W}/bfp && {W}/bfp",
  "1 1 11\n" },

/* §6.3.1.8: `double * float _Complex` (the x*I idiom) is computed in double, not
   narrowed to float — the imaginary part keeps double precision. */
{ "complex_real_precision", "",
  "printf '#include <complex.h>\\n#include <stdio.h>\\n"
  "int main(void){ volatile double r=0.1; double _Complex z=r*I; printf(\\\"%%.17g\\\\n\\\", cimag(z)); return 0; }\\n' > {W}/cxp.c && "
  "{MCC} -B{B} -I{I} {W}/cxp.c -lm -o {W}/cxp && {W}/cxp",
  "0.10000000000000001\n" },

/* §6.4.4.1 / §6.4.4.4 / §6.7.6.2p1: GNU/C23 extensions ISO C99/C11 do not have —
   binary integer constants (0b…), the \e escape, and a zero-size array — are
   diagnosed under -pedantic; the ISO-valid neighbours (hex, standard escapes, a
   sized array, the proper flexible array member) stay clean under -Werror. */
{ "pedantic_extension_diagnostics", "",
  "printf 'int x=0b1010;\\n' > {W}/pb.c && "
  "printf 'char *s=\"\\\\e[0m\";\\n' > {W}/pe.c && "
  "printf 'int a[0];\\n' > {W}/pa.c && "
  "printf 'int x=0x1f,y=42; const char*s=\"\\\\n\\\\t\"; int arr[3]; struct S{int n;int d[];};\\nint main(void){return 0;}\\n' > {W}/pok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/pb.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/pe.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -pedantic-errors -c {W}/pa.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Werror -pedantic-errors -c {W}/pok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'binary integer constants|non-ISO escape sequence|forbids zero-size array|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 binary integer constants\n1 forbids zero-size array\n1 non-ISO escape sequence\n" },

/* §6.4.4.4/§6.4.5/§6.7.1/§6.7.2.4: u/U/u8 literal prefixes, _Thread_local and
   _Atomic are C11 additions — diagnosed under -std=c99 -pedantic-errors; the L
   (C90) prefix and all three under -std=c11 stay clean. */
{ "c11_features_in_c99", "",
  "printf 'const void*p=u\"hi\";\\n' > {W}/cu.c && "
  "printf 'static _Thread_local int x;\\n' > {W}/ct.c && "
  "printf '_Atomic(int) a;\\n' > {W}/ca.c && "
  "printf 'const void*p=u\"hi\"; static _Thread_local int x; _Atomic(int) a; const void*q=L\"w\";\\nint main(void){return 0;}\\n' > {W}/cok.c && "
  "{ {MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/cu.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/ct.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c99 -pedantic-errors -c {W}/ca.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/cok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'character/string prefix|_Thread_local|_Atomic|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 _Atomic\n1 _Thread_local\n1 character/string prefix\n" },

/* §6.10.9p1 (_Pragma operand must be a parenthesized string literal) and §6.7p3
   (a variably-modified typedef may not be redefined, even with the same type). */
{ "pragma_vla_typedef_constraints", "",
  "printf 'void f(void){ _Pragma(foo); }\\n' > {W}/pn.c && "
  "printf 'void f(int n){ typedef int T[n]; typedef int T[n]; (void)sizeof(T[0]); }\\n' > {W}/tv.c && "
  "printf 'void g(void){ _Pragma(\"pack(1)\"); }\\ntypedef int U; typedef int U; U z;\\nint main(void){return 0;}\\n' > {W}/pvok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/pn.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/tv.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/pvok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
  "grep -oE '_Pragma takes a parenthesized|redefinition of variably modified|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 _Pragma takes a parenthesized\n1 redefinition of variably modified\n" },

/* §6.7.6.3p3: an identifier list in a function declarator that is NOT part of a
   definition shall be empty — `int f(a,b);` is rejected, while the K&R
   *definition* `int def(a,b) int a,b; {…}` and an empty `int proto();` are fine. */
{ "kr_identifier_list_declaration", "",
  "printf 'int f(a,b);\\nint main(void){return 0;}\\n' > {W}/kr_bad.c && "
  "printf 'int def(a,b) int a,b; { return a+b; }\\nint proto();\\nint main(void){return def(1,2)+proto();}\\n' > {W}/kr_ok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/kr_bad.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/kr_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'parameter names \\(without types\\)|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 parameter names (without types)\n" },

/* §6.9.1p3 / §6.9.1p7: in a function *definition* the return type must be void
   or a complete object type, and every parameter's adjusted type must be a
   complete object type — an incomplete struct return or parameter is rejected.
   The same incomplete types are fine via a pointer, a void return, or a mere
   declaration (all of ic_ok.c stays clean even under -Werror). */
{ "function_definition_complete_types", "",
  "printf 'struct S; struct S g(void){ }\\n' > {W}/ic_ret.c && "
  "printf 'struct S; int h(struct S s){ return 0; }\\n' > {W}/ic_par.c && "
  "printf 'struct C{int x;}; struct C cc(struct C a){return a;}\\nstruct S; struct S *p(void){return 0;} int q(struct S *s){return !!s;}\\nvoid v(void){}\\nint main(void){return 0;}\\n' > {W}/ic_ok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/ic_ret.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/ic_par.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/ic_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'return type is an incomplete|has incomplete type|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 has incomplete type\n1 return type is an incomplete\n" },

/* §6.7.8p3 / §6.2.3: a typedef name and an ordinary identifier share one name
   space — reusing a typedef name for an object (`int T;`) or a function
   definition (`int T(void){…}`) in the same scope is rejected, while a deeper
   block may shadow the typedef and ordinary redeclarations remain valid. */
{ "typedef_ordinary_name_space", "",
  "printf 'typedef int T; int T;\\n' > {W}/ns_obj.c && "
  "printf 'typedef int T; int T(void){return 0;}\\n' > {W}/ns_fn.c && "
  "printf 'typedef int T; void f(void){ int T; T=1; (void)T; }\\nextern int x; int x=5;\\nint main(void){return x;}\\n' > {W}/ns_ok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/ns_obj.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -c {W}/ns_fn.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/ns_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
  "grep -oE \"redeclared as different kind|redefinition of 'T'|CLEAN_OK\" | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 redeclared as different kind\n1 redefinition of 'T'\n" },

/* §6.9.1p2: a function definition's declarator must itself include a parameter
   (or K&R identifier) list — the function type may not be supplied entirely by a
   typedef name (`typedef int F(void); F f {…}`).  A typedef *return* type, a K&R
   definition, and using the function typedef for a pointer all stay valid. */
{ "function_definition_typedef_type", "",
  "printf 'typedef int F(void); F f { return 0; }\\n' > {W}/tdf_bad.c && "
  "printf 'typedef int T; T h(void){ return 0; }\\nint def(a,b) int a,b; { return a+b; }\\ntypedef int F(void); F *fp;\\nint main(void){return h()+def(1,2)+(fp!=0);}\\n' > {W}/tdf_ok.c && "
  "{ {MCC} -B{B} -I{I} -std=c11 -c {W}/tdf_bad.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -std=c11 -Werror -c {W}/tdf_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
  "grep -oE \"declared with a typedef'd function type|CLEAN_OK\" | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 declared with a typedef'd function type\n" },

/* §6.4.4 / GNU: imaginary *integer* constants (`3i`, `5j`, `0x4I`) — previously
   only the floating forms (`3.0i`) were accepted; the integer forms now build a
   complex value too (matching gcc/clang).  Compile and run, checking real/imag. */
{ "imaginary_integer_constants", "",
  "printf '#include <complex.h>\\n#include <stdio.h>\\nint main(void){ double complex z = 2 + 3i; long double complex w = 4 + 5j; printf(\"%%g %%g %%Lg %%Lg\\\\n\", creal(z), cimag(z), creall(w), cimagl(w)); return 0; }\\n' > {W}/imag_rt.c && "
  "{MCC} -B{B} -I{I} -std=c11 {W}/imag_rt.c -o {W}/imag_rt && {W}/imag_rt",
  "2 3 4 5\n" },

/* §7.25p6: the type-generic nexttoward selects its function from the FIRST
   argument only (its second argument is always long double).  sizeof is
   unevaluated, so this checks the *selected return type* (float=4, double=8,
   long double=16) without needing libm — keying on (x)+(y) wrongly gave 16 16 16. */
{ "tgmath_nexttoward_first_arg", "os!=WIN32:PE has long double==double (8), so nexttoward(long double) is 8 not 16",
  "printf '#include <tgmath.h>\\n#include <stdio.h>\\nint main(void){ float f=1; double d=1; long double l=1; printf(\"%%d %%d %%d\\\\n\", (int)sizeof(nexttoward(f,2.0L)), (int)sizeof(nexttoward(d,2.0L)), (int)sizeof(nexttoward(l,2.0L))); return 0; }\\n' > {W}/ntg.c && "
  "{MCC} -B{B} -I{I} -std=c11 {W}/ntg.c -o {W}/ntg && {W}/ntg",
  "4 8 16\n" },

/* §6.4.2.1 / Annex D.2: a combining-mark code point (e.g. U+0300) may appear in
   an identifier but NOT as its first character.  As an initial UCN it is now
   rejected; the same mark as a NON-initial character, and ordinary leading UCNs
   like U+00C0, stay valid. */
{ "ucn_identifier_initial_combining", "",
  "printf 'int \\\\u0300x;\\n' > {W}/ucn_bad.c && "
  "printf 'int a\\\\u0300b = 5;\\nint \\\\u00C0v = 7;\\nint main(void){ return a\\\\u0300b + \\\\u00C0v; }\\n' > {W}/ucn_ok.c && "
  "{ {MCC} -B{B} -I{I} -c {W}/ucn_bad.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -Werror -c {W}/ucn_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; } | "
  "grep -oE 'not valid as the first character|CLEAN_OK' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 not valid as the first character\n" },

/* §6.6/§G: a complex constant whose real part overflows to infinity must yield
   inf in BOTH a static and a LOCAL initializer.  The local form used to store 0
   (the gen_complex_op constant-fold path called init_putv on a value gen_op had
   declined to fold in a non-CONST_WANTED context); now it falls to the robust
   runtime path.  Finite locals stay exact. Compile and run, check the values. */
{ "complex_const_init_overflow", "os!=WIN32:msvcrt prints infinity as \"1.#INF\", not glibc's \"inf\"",
  "printf '#include <complex.h>\\n#include <stdio.h>\\ndouble complex gz = 4.0e38f + 0.0*I;\\nint main(void){ double complex lz = 4.0e38f + 0.0*I; double complex lf = 2.0 + 3.0*I; printf(\"%%g %%g %%g %%g\\\\n\", creal(gz), creal(lz), creal(lf), cimag(lf)); return 0; }\\n' > {W}/imc.c && "
  "{MCC} -B{B} -I{I} -std=c11 {W}/imc.c -lm -o {W}/imc && {W}/imc",
  "inf inf 2 3\n" },

/* §7.21.6.1/2 — -Wformat: a printf/scanf-family call with a string-literal
   format is checked for argument class (integer/floating/pointer) and count.
   The four mismatches warn; a fully-correct call (incl. %*.*f, %% and scanf
   pointers) is clean even under -Werror; and a default build (no -Wformat)
   stays silent so existing builds and the self-host are unaffected. */
{ "wformat_printf_scanf_checking", "",
  "printf '#include <stdio.h>\\nint main(void){ printf(\"%%d\\\\n\",\"x\"); printf(\"%%s\\\\n\",1); printf(\"%%f\\\\n\",2); printf(\"%%d %%d\\\\n\",1); return 0; }\\n' > {W}/wf_bad.c && "
  "printf '#include <stdio.h>\\nint main(void){ char b[8]; int x; printf(\"%%d %%s %%f %%c %%p %%%%\\\\n\",1,\"x\",2.0,(int)0x61,(void*)0); printf(\"%%*.*f\\\\n\",4,2,3.14); scanf(\"%%d %%7s\",&x,b); return x; }\\n' > {W}/wf_ok.c && "
  "{ {MCC} -B{B} -I{I} -Wformat -c {W}/wf_bad.c -o /dev/null 2>&1; "
  "{MCC} -B{B} -I{I} -Wformat -Werror -c {W}/wf_ok.c -o /dev/null 2>&1 && echo CLEAN_OK; "
  "{MCC} -B{B} -I{I} -c {W}/wf_bad.c -o /dev/null 2>&1 && echo SILENT_DEFAULT; } | "
  "grep -oE 'expects an integer argument|expects a pointer argument|expects a floating argument|more conversions than arguments|CLEAN_OK|SILENT_DEFAULT' | sort | uniq -c | sed 's/^ *//'",
  "1 CLEAN_OK\n1 SILENT_DEFAULT\n1 expects a floating argument\n1 expects a pointer argument\n1 expects an integer argument\n1 more conversions than arguments\n" },

};
static const int cli_cases_count = (int)(sizeof(cli_cases)/sizeof(cli_cases[0]));

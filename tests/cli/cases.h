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

{ "print_search_dirs", "",
  "{MCC} -B{B} -print-search-dirs | grep -oE '^(install|include|libraries|crt):'",
  "install:\ninclude:\nlibraries:\ncrt:\n" },

/* ---- sub-tools / driver --------------------------------------------- */
{ "ar_create_list", "",
  "{MCC} -B{B} -I{I} -c {D}/lib.c -o {W}/al.o && {MCC} -B{B} -I{I} -c {D}/sec.c -o {W}/as.o && "
  "{MCC} -ar rcs {W}/libcli.a {W}/al.o {W}/as.o && {MCC} -ar t {W}/libcli.a",
  "al.o\nas.o\n" },

{ "response_file", "",
  "printf -- '-c {D}/lib.c -o {W}/resp.o\\n' > {W}/a.rsp && {MCC} -B{B} -I{I} @{W}/a.rsp && "
  "nm {W}/resp.o | grep -oE 'exported_fn'",
  "exported_fn\n" },

};
static const int cli_cases_count = (int)(sizeof(cli_cases)/sizeof(cli_cases[0]));

if(NOT CPU STREQUAL "x86_64" OR TARGETOS STREQUAL "WIN32" OR TARGETOS STREQUAL "Darwin")
    message("asm/reloc-suffix: needs an ELF x86_64 target (cpu=${CPU} os=${TARGETOS})")
    cmake_language(EXIT 77)
endif()

set(_w "${BINDIR}/asm-reloc-suffix")
file(REMOVE_RECURSE "${_w}")
file(MAKE_DIRECTORY "${_w}")

file(WRITE "${_w}/plain.c"
     "void g(void);\nvoid f(void) { __asm__ volatile(\"call g\"); }\n")
file(WRITE "${_w}/plt.c"
     "void g(void);\nvoid f(void) { __asm__ volatile(\"call g@PLT\"); }\n")
file(WRITE "${_w}/jmpplain.c"
     "void h(void);\nvoid f(void) { __asm__ volatile(\"jmp h\"); }\n")
file(WRITE "${_w}/jmpplt.c"
     "void h(void);\nvoid f(void) { __asm__ volatile(\"jmp h@PLT\"); }\n")
file(WRITE "${_w}/gotpcrel.c"
     "extern int v;\nvoid f(void) { __asm__ volatile(\"movq v@GOTPCREL(%%rip), %%rax\" ::: \"rax\"); }\n")
file(WRITE "${_w}/bogus.c"
     "void g(void);\nvoid f(void) { __asm__ volatile(\"call g@NOSUCHRELOC\"); }\n")

function(_asm_compile _name _rcvar _outvar)
    execute_process(COMMAND "${MCC}" -c "${_w}/${_name}.c" -o "${_w}/${_name}.o"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _o ERROR_VARIABLE _e)
    set(${_rcvar} "${_rc}" PARENT_SCOPE)
    set(${_outvar} "${_o}${_e}" PARENT_SCOPE)
endfunction()

foreach(_t plain plt jmpplain jmpplt)
    _asm_compile(${_t} _rc _out)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "asm/reloc-suffix: ${_t}.c did not compile. The "
                            "@PLT suffix on an x86_64 asm operand must parse -- "
                            "musl needs it and mcc already emits R_X86_64_PLT32 "
                            "for the plain form. Output:\n${_out}")
    endif()
endforeach()

find_program(_readelf NAMES readelf llvm-readelf eu-readelf)
if(_readelf STREQUAL "_readelf-NOTFOUND")
    message("asm/reloc-suffix: no readelf on this host, the relocation half "
            "cannot be checked")
    cmake_language(EXIT 77)
endif()

function(_asm_relocs _name _outvar)
    execute_process(COMMAND "${_readelf}" -r "${_w}/${_name}.o"
                    OUTPUT_VARIABLE _o ERROR_QUIET RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "asm/reloc-suffix: readelf failed on ${_name}.o")
    endif()
    set(_keep "")
    string(REPLACE "\n" ";" _lines "${_o}")
    foreach(_l IN LISTS _lines)
        if(_l MATCHES "R_X86_64_[A-Z0-9]+")
            if(NOT _l MATCHES "eh_frame|\\.text \\+")
                string(STRIP "${_l}" _l)
                list(APPEND _keep "${_l}")
            endif()
        endif()
    endforeach()
    set(${_outvar} "${_keep}" PARENT_SCOPE)
endfunction()

_asm_relocs(plain _r_plain)
_asm_relocs(plt _r_plt)
_asm_relocs(jmpplain _r_jmpplain)
_asm_relocs(jmpplt _r_jmpplt)

if(NOT _r_plain MATCHES "R_X86_64_PLT32")
    message(FATAL_ERROR "asm/reloc-suffix: a plain `call g` no longer emits "
                        "R_X86_64_PLT32 (got: ${_r_plain}). The whole premise of "
                        "ignoring @PLT is that the plain form already carries that "
                        "relocation; if it does not, ignoring the suffix is wrong")
endif()
if(NOT _r_plain STREQUAL _r_plt)
    message(FATAL_ERROR "asm/reloc-suffix: `call g` and `call g@PLT` disagree.\n"
                        "  plain: ${_r_plain}\n  @PLT:  ${_r_plt}")
endif()
if(NOT _r_jmpplain STREQUAL _r_jmpplt)
    message(FATAL_ERROR "asm/reloc-suffix: `jmp h` and `jmp h@PLT` disagree.\n"
                        "  plain: ${_r_jmpplain}\n  @PLT:  ${_r_jmpplt}")
endif()
message("asm/reloc-suffix: call/jmp @PLT relocate identically to the plain form "
        "(${_r_plain} / ${_r_jmpplain})")

foreach(_t gotpcrel bogus)
    _asm_compile(${_t} _rc _out)
    if(_rc EQUAL 0)
        message(FATAL_ERROR "asm/reloc-suffix: ${_t}.c compiled. An unimplemented "
                            "@suffix must be diagnosed, never silently swallowed -- "
                            "swallowing @GOTPCREL would emit a direct reference "
                            "where an indirection through the GOT was written")
    endif()
    if(NOT _out MATCHES "unsupported relocation operator")
        message(FATAL_ERROR "asm/reloc-suffix: ${_t}.c failed, but not with the "
                            "relocation-operator diagnostic, so the refusal is "
                            "incidental rather than deliberate. Output:\n${_out}")
    endif()
endforeach()
message("asm/reloc-suffix: @GOTPCREL and an unknown @suffix both diagnose")

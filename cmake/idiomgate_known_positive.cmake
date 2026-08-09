execute_process(COMMAND "${GATE}" "${SRCDIR}/src" "${SRCDIR}/tools"
                        "${SRCDIR}/runtime" "${SRCDIR}/include"
                RESULT_VARIABLE _clean OUTPUT_VARIABLE _out ERROR_VARIABLE _out)
string(REGEX MATCH "idiom-gate subject:[^\n]*" _csum "${_out}")
string(REGEX MATCH "idiom-gate coverage:[^\n]*" _ccov "${_out}")
message("${_csum}")
message("${_ccov}")
if(NOT _clean EQUAL 0)
    message(FATAL_ERROR "idiom-gate-known-positive: the real tree is already "
                        "failing the invariant, so this cell cannot say "
                        "anything about whether the gate can detect "
                        "anything:\n${_out}")
endif()

execute_process(COMMAND "${GATE}" --subset "${SRCDIR}/tests/idiom/known-positive"
                RESULT_VARIABLE _bad OUTPUT_VARIABLE _bout ERROR_VARIABLE _bout)
message("${_bout}")
if(_bad EQUAL 0)
    message(FATAL_ERROR "idiom-gate-known-positive: tests/idiom/known-positive "
                        "tests a value-kind config macro with #ifdef, one with "
                        "#ifndef and no default #define, one through defined(), "
                        "and a flag-kind one as a value -- every violation shape "
                        "the gate names -- and the gate passed. It is not "
                        "reading the files it walks")
endif()
if(NOT _bout MATCHES "17 violation\\(s\\)")
    message(FATAL_ERROR "idiom-gate-known-positive: the fixture carries 17 "
                        "wrong idioms across 16 config macros and the gate did "
                        "not report 17. A fixture that stops firing on one macro "
                        "silently withdraws that macro's guarantee:\n${_bout}")
endif()
foreach(_m MCC_CONFIG_PIE MCC_CONFIG_STATIC MCC_CONFIG_MUSL
           MCC_CONFIG_TRACE MCC_CONFIG_CPUVER MCC_CONFIG_DWARF_VERSION
           MCC_CONFIG_SEMLOCK MCC_CONFIG_RUNMEM_RO MCC_CONFIG_AUTO_MCCDIR
           MCC_CONFIG_SYSROOT MCC_CONFIG_CROSSPREFIX MCC_CONFIG_CRTPREFIX
           MCC_CONFIG_LIBPATHS MCC_CONFIG_SYSINCLUDEPATHS
           MCC_CONFIG_ELFINTERP MCC_CONFIG_ELFINTERP_ARMHF)
    if(NOT _bout MATCHES "${_m}:")
        message(FATAL_ERROR "idiom-gate-known-positive: the fixture carries a "
                            "wrong idiom on ${_m} and the gate did not name it. "
                            "That macro is registered but not enforceable, which "
                            "is a coverage figure with nothing behind it")
    endif()
endforeach()

execute_process(COMMAND "${GATE}" --subset "${SRCDIR}/tests/idiom/unregistered"
                RESULT_VARIABLE _unreg OUTPUT_VARIABLE _uout ERROR_VARIABLE _uout)
message("${_uout}")
if(_unreg EQUAL 0 OR NOT _uout MATCHES "MCC_CONFIG_NOT_A_REAL_KNOB")
    message(FATAL_ERROR "idiom-gate-known-positive: tests/idiom/unregistered "
                        "tests a MCC_CONFIG_* name that has no REGISTRY row, "
                        "and the gate accepted it. A name the gate does not "
                        "know is a name it exempts, and the denominator it "
                        "prints would then not be the tree's:\n${_uout}")
endif()

execute_process(COMMAND "${GATE}" --subset "${SRCDIR}/tests/idiom/no-subject"
                RESULT_VARIABLE _nosub OUTPUT_VARIABLE _nout ERROR_VARIABLE _nout)
message("${_nout}")
if(_nosub EQUAL 0 OR NOT _nout MATCHES "MCC_CONFIG_UCLIBC:" OR
   NOT _nout MATCHES "MCC_CONFIG_JIT:")
    message(FATAL_ERROR "idiom-gate-known-positive: tests/idiom/no-subject puts "
                        "a conditional on MCC_CONFIG_UCLIBC and MCC_CONFIG_JIT, "
                        "both registered as reachable by no conditional, and the "
                        "gate accepted them. A refusal that cannot be "
                        "contradicted is not a refusal:\n${_nout}")
endif()

execute_process(COMMAND "${GATE}" "${SRCDIR}/tests/idiom/known-positive"
                RESULT_VARIABLE _floor OUTPUT_VARIABLE _fout ERROR_VARIABLE _fout)
message("${_fout}")
if(_floor EQUAL 0 OR NOT _fout MATCHES "enforced over zero conditional")
    message(FATAL_ERROR "idiom-gate-known-positive: run over a directory that "
                        "reaches 16 of the 29 checked macros, the gate did not "
                        "report the other 13 as enforced over nothing. Without "
                        "that floor a macro whose last conditional is deleted "
                        "keeps counting toward coverage:\n${_fout}")
endif()

execute_process(COMMAND "${GATE}" "${SRCDIR}/tests/idiom/empty"
                RESULT_VARIABLE _empty OUTPUT_VARIABLE _eout ERROR_VARIABLE _eout)
message("${_eout}")
if(_empty EQUAL 0)
    message(FATAL_ERROR "idiom-gate-known-positive: the gate reported OK over "
                        "an empty directory. A run that scanned no file must "
                        "not be character-for-character identical to a run "
                        "that checked the whole tree")
endif()
message("idiom-gate-known-positive: clean OK, 17 violations across 16 macros "
        "detected, unregistered name refused, no-subject registration "
        "contradicted, zero-conditional floor fired, empty walk refused")

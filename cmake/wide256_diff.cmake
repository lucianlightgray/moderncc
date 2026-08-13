cmake_minimum_required(VERSION 3.22)

if(NOT MCC OR NOT SRCDIR OR NOT BINDIR OR NOT HOSTCC)
    message(FATAL_ERROR "wide256_diff: MCC, SRCDIR, BINDIR and HOSTCC are required")
endif()

set(_dir "${BINDIR}/wide256-diff")
file(MAKE_DIRECTORY "${_dir}")

set(_gmpflags "")
if(GMP_INC)
    list(APPEND _gmpflags "-I${GMP_INC}")
endif()
if(GMP_LIBDIR)
    list(APPEND _gmpflags "-L${GMP_LIBDIR}")
endif()

set(_probe "${_dir}/probe.c")
file(WRITE "${_probe}" "#include <gmp.h>\nint main(void){mpz_t a;mpz_init(a);mpz_clear(a);return 0;}\n")
execute_process(COMMAND "${HOSTCC}" "${_probe}" -o "${_dir}/probe" ${_gmpflags} -lgmp
                RESULT_VARIABLE _probe_rc OUTPUT_QUIET ERROR_QUIET)
if(NOT _probe_rc EQUAL 0)
    message(STATUS "wide256/gmp-diff: no GMP for the host compiler '${HOSTCC}'; "
                   "the oracle cannot be built, so there is nothing to compare "
                   "mcc against; SKIP")
    cmake_language(EXIT 77)
endif()

set(_oracle "${_dir}/oracle")
execute_process(COMMAND "${HOSTCC}" -O2 "-I${SRCDIR}/tests/wide256" ${_gmpflags}
                        "${SRCDIR}/tests/wide256/oracle.c" -o "${_oracle}" -lgmp
                RESULT_VARIABLE _orc OUTPUT_VARIABLE _oout ERROR_VARIABLE _oout)
if(NOT _orc EQUAL 0)
    message(FATAL_ERROR "wide256/gmp-diff: the GMP oracle failed to build even "
                        "though the GMP probe linked:\n${_oout}")
endif()

execute_process(COMMAND "${_oracle}" OUTPUT_FILE "${_dir}/oracle.txt"
                RESULT_VARIABLE _rrc ERROR_VARIABLE _rerr)
if(NOT _rrc EQUAL 0)
    message(FATAL_ERROR "wide256/gmp-diff: the oracle exited ${_rrc}:\n${_rerr}")
endif()

file(STRINGS "${_dir}/oracle.txt" _olines)
list(LENGTH _olines _on)
if(_on LESS 9000)
    message(FATAL_ERROR "wide256/gmp-diff: the oracle produced only ${_on} lines; "
                        "a corpus this small would let an empty or truncated "
                        "subject agree with it by accident. Expected at least 9000.")
endif()

set(_mutflag "")
if(MUTATE)
    set(_mutflag "-DMCC_W256_MUTATE=1")
endif()

set(_failed "")
set(_checked 0)
foreach(_o -O0 -O1 -O2 -O3 -Os)
    set(_bin "${_dir}/subject${_o}")
    execute_process(COMMAND "${MCC}" "-B${BINDIR}" ${_o} ${_mutflag}
                            "-I${SRCDIR}/tests/wide256"
                            "${SRCDIR}/tests/wide256/subject.c" -o "${_bin}"
                    RESULT_VARIABLE _crc OUTPUT_VARIABLE _cout ERROR_VARIABLE _cout)
    if(NOT _crc EQUAL 0)
        message(FATAL_ERROR "wide256/gmp-diff: mcc failed to compile the "
                            "'__int256' subject at ${_o}:\n${_cout}")
    endif()
    execute_process(COMMAND "${_bin}" OUTPUT_FILE "${_dir}/subject${_o}.txt"
                    RESULT_VARIABLE _src ERROR_VARIABLE _serr)
    if(NOT _src EQUAL 0)
        message(FATAL_ERROR "wide256/gmp-diff: the ${_o} subject exited ${_src}:\n${_serr}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
                            "${_dir}/oracle.txt" "${_dir}/subject${_o}.txt"
                    RESULT_VARIABLE _drc OUTPUT_QUIET ERROR_QUIET)
    math(EXPR _checked "${_checked} + 1")
    if(NOT _drc EQUAL 0)
        list(APPEND _failed "${_o}")
    endif()
endforeach()

message(STATUS "wide256/gmp-diff: ${_on} oracle rows x ${_checked} optimisation levels")
if(NOT _failed STREQUAL "")
    execute_process(COMMAND diff "${_dir}/oracle.txt" "${_dir}/subject-O0.txt"
                    OUTPUT_VARIABLE _d ERROR_QUIET)
    string(SUBSTRING "${_d}" 0 4000 _d)
    message(FATAL_ERROR "wide256/gmp-diff: mcc's '__int256' disagrees with GMP at "
                        "${_failed}. The oracle is libgmp compiled by the host C "
                        "compiler and shares no code with mcc, so a disagreement "
                        "is an mcc defect -- fix the compiler or the runtime, do "
                        "not re-pin the expectation. First differences (-O0):\n${_d}")
endif()
message("wide256/gmp-diff: OK -- ${_on} rows agree with GMP at ${_checked} levels")

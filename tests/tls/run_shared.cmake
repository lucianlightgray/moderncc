cmake_minimum_required(VERSION 3.22)

if(NOT MCC OR NOT BDIR OR NOT SRC OR NOT OUT)
    message(FATAL_ERROR "run_shared: MCC, BDIR, SRC, OUT are required")
endif()

execute_process(
    COMMAND "${MCC}" "-B${BDIR}" -nostdlib -fPIC -shared "${SRC}" -o "${OUT}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "mcc -fPIC -shared failed (${_rc}):\n${_out}${_err}")
endif()
if(NOT EXISTS "${OUT}")
    message(FATAL_ERROR "mcc -fPIC -shared produced no ${OUT}")
endif()

# The three accesses must reach the thread block through the GOT, not through a
# baked tp offset: a shared object does not own the block a local-exec offset is
# relative to. Read the file rather than trusting the exit status -- the bug this
# guards emitted a .so at rc=0 with every offset resolved to zero.
file(READ "${OUT}" _hex HEX)

# Elf64_Rela.r_info for R_AARCH64_TLS_TPREL64 (1030) against symbol 0, the form a
# non-preemptible thread-local takes: type in the low word, index in the high.
string(FIND "${_hex}" "0604000000000000" _tprel)
if(_tprel LESS 0)
    message(FATAL_ERROR
        "no R_AARCH64_TLS_TPREL64 relocation in ${OUT}; the thread-local "
        "accesses did not go dynamic")
endif()

# TLSLE resolves at link time and leaves no dynamic relocation at all, so the
# check above is the one that fails when local-exec leaks back in.
message(STATUS "tls-shared: R_AARCH64_TLS_TPREL64 present, link clean")

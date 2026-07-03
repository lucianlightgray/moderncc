# Byte-exact `-S` roundtrip: `mcc -c prog.c` and `mcc -c (mcc -S prog.c)`
# must produce identical section contents.  Exercised on the fixed-width
# targets (arm64, riscv64), whose disassemblers guarantee byte identity;
# x86 targets use the behavioral dash-s-roundtrip instead (the assembler
# legally re-encodes some immediates/branches there).
#
# usage: cmake -DMCC=<compiler> -DSECCMP=<seccmp> -DSRC=<prog.c> -DWORK=<dir>
#        -P run_dash_s_bytes.cmake

foreach(v MCC SECCMP SRC WORK)
    if(NOT DEFINED ${v})
        message(FATAL_ERROR "run_dash_s_bytes: missing -D${v}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${WORK}")

function(run)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE _rc
                    OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "FAILED (${_rc}): ${ARGV}\n${_out}${_err}")
    endif()
endfunction()

run("${MCC}" -c "${SRC}" -o "${WORK}/ref.o")
run("${MCC}" -S "${SRC}" -o "${WORK}/prog.s")
run("${MCC}" -c "${WORK}/prog.s" -o "${WORK}/rt.o")

execute_process(COMMAND "${SECCMP}" "${WORK}/ref.o" "${WORK}/rt.o"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message("${_out}${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "-S roundtrip is not byte-identical")
endif()

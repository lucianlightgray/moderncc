file(WRITE "${BINDIR}/ladder_gpu_probe.c"
"int f(int a, int b) { int t = a * 3 + b; if (b & 7) t = t ^ (a >> 2); return t + (a & b); }\n"
"int g(int a, int b) { return (a * 3 + b) + (a & b); }\n"
"int h(int a, int b) { return a / (b | 1) + a % (b | 1); }\n"
"int k(int a, int b) { return (a < b) + (a == b) + (a > b); }\n"
"int main(void) { return f(1,2) + g(3,4) + h(5,6) + k(7,8); }\n")
set(_srcs "${BINDIR}/ladder_gpu_probe.c")
file(GLOB _more "${SRCDIR}/tests/exec/expressions/*.c")
list(SORT _more)
list(LENGTH _more _n)
if(_n GREATER 8)
    list(SUBLIST _more 0 8 _more)
endif()
list(APPEND _srcs ${_more})

set(_base "MCC_AST_EVAL_LADDER=1;MCC_AST_EVAL_LADDER_CENSUS=1")
set(_ndiff 0)
set(_disp 0)
foreach(_f IN LISTS _srcs)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${_base}
                            "${MCC}" -w -O2 -c "${_f}" -o "${BINDIR}/ladpar.o"
                    OUTPUT_VARIABLE _a ERROR_VARIABLE _a TIMEOUT 120)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${_base}
                            "MCC_AST_EVAL_LADDER_GPU=1"
                            "${MCC}" -w -O2 -c "${_f}" -o "${BINDIR}/ladpar.o"
                    OUTPUT_VARIABLE _b ERROR_VARIABLE _b TIMEOUT 120)
    if(_b MATCHES "available=0")
        message("ladder-gpu-parity: no usable Vulkan device, skipping")
        return()
    endif()
    if(_b MATCHES "dispatches=([0-9]+)")
        math(EXPR _disp "${_disp} + ${CMAKE_MATCH_1}")
    endif()
    string(REGEX MATCHALL "\\[ladder-cross\\] [^\n]*" _av "${_a}")
    string(REGEX MATCHALL "\\[ladder-cross\\] [^\n]*" _bv "${_b}")
    list(FILTER _av EXCLUDE REGEX "secs=")
    list(FILTER _bv EXCLUDE REGEX "secs=")
    if(NOT "${_av}" STREQUAL "${_bv}")
        math(EXPR _ndiff "${_ndiff} + 1")
        message("VERDICT DIFF in ${_f}")
        message("  cpu: ${_av}")
        message("  gpu: ${_bv}")
    endif()
endforeach()

message("ladder-gpu-parity: gpu dispatches=${_disp} differing-files=${_ndiff}")
if(_disp EQUAL 0)
    message(FATAL_ERROR "ladder-gpu-parity: zero GPU dispatches, so identical "
                        "verdicts prove nothing -- the GPU path never ran")
endif()
if(NOT _ndiff EQUAL 0)
    message(FATAL_ERROR "ladder-gpu-parity: the GPU oracle disagrees with the "
                        "CPU oracle on ${_ndiff} file(s)")
endif()
message("ladder-gpu-parity: OK")

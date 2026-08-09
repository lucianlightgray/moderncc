file(WRITE "${BINDIR}/ladder_gpu_probe.c"
"int f(int a, int b) { int t = a * 3 + b; if (b & 7) t = t ^ (a >> 2); return t + (a & b); }\n"
"int g(int a, int b) { return (a * 3 + b) + (a & b); }\n"
"int h(int a, int b) { return a / (b | 1) + a % (b | 1); }\n"
"int k(int a, int b) { return (a < b) + (a == b) + (a > b); }\n"
"unsigned long long u(unsigned long long a, unsigned long long b)\n"
"{ return (a * 0xAAAAAAAAAAAAAAABULL >> 1) + (b | 1) + (a < b); }\n"
"long long s(long long a, long long b)\n"
"{ long long d = b | 1; return ((a << 40) / d) + ((a << 37) % d) - (a * b); }\n"
"long long c(long long a, long long b)\n"
"{ long long x = a << 40, y = b << 33; return (x < y) + (x == y) + (x ^ y) + ~x + -y; }\n"
"long long v(long long a, long long b)\n"
"{ return (a << (b & 63)) ^ (a >> (b & 63)) ^ (b ? a / b : (int)a); }\n"
"int main(void) { return f(1,2) + g(3,4) + h(5,6) + k(7,8) +\n"
"                        (int)u(9,10) + (int)s(11,12) + (int)c(13,14) + (int)v(15,16); }\n")
set(_srcs "${BINDIR}/ladder_gpu_probe.c")
file(GLOB _more "${SRCDIR}/tests/exec/expressions/*.c")
list(SORT _more)
# Drop files carrying inline asm before taking the first 8. The corpus is read
# for a ladder census differential and an asm statement contributes nothing to
# an AST ladder on any host -- but it is not arch-neutral: the first file
# alphabetically, al_ax_extend.c, is x86 (`movl $0x1234ABCD, %eax`), so on
# arm64 the CPU arm refused it with "ARM64 instruction 'movl' not implemented"
# and the status check below turned that into a hard failure. That check is
# correct and stays; the corpus selection was what was host-blind.
set(_keep "")
foreach(_c IN LISTS _more)
    file(READ "${_c}" _txt)
    if(NOT _txt MATCHES "(^|[^A-Za-z_])(__asm__|asm)[ \t\n]*\\(")
        list(APPEND _keep "${_c}")
    endif()
endforeach()
set(_more "${_keep}")
list(LENGTH _more _n)
if(_n LESS 4)
    message(FATAL_ERROR
        "ladder-gpu-parity: only ${_n} asm-free files in tests/exec/expressions; "
        "the corpus filter has eaten the corpus and the differential would "
        "compare almost nothing")
endif()
if(_n GREATER 8)
    list(SUBLIST _more 0 8 _more)
endif()
list(APPEND _srcs ${_more})

set(_base "MCC_AST_EVAL_LADDER=1;MCC_AST_EVAL_LADDER_CENSUS=1")
set(_ndiff 0)
set(_disp 0)
foreach(_f IN LISTS _srcs)
    # Both exit statuses are checked. Without RESULT_VARIABLE this cell greps
    # stdout only, so the compiler could dump core on every file and the cell
    # would still report PASS -- the two arms' census lines would simply both be
    # absent and compare equal. Every other GPU driver script in cmake/ already
    # captures the status; this one did not.
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${_base}
                            "${MCC}" -w -O2 -c "${_f}" -o "${BINDIR}/ladpar.o"
                    OUTPUT_VARIABLE _a ERROR_VARIABLE _a RESULT_VARIABLE _rca
                    TIMEOUT 120)
    if(NOT _rca EQUAL 0)
        message(FATAL_ERROR "ladder-gpu-parity: the CPU arm failed on ${_f} "
                            "(rc=${_rca}); a crash here made this cell pass "
                            "vacuously before the status was checked\n${_a}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${_base}
                            "MCC_AST_EVAL_LADDER_GPU=1"
                            "${MCC}" -w -O2 -c "${_f}" -o "${BINDIR}/ladpar.o"
                    OUTPUT_VARIABLE _b ERROR_VARIABLE _b RESULT_VARIABLE _rcb
                    TIMEOUT 120)
    if(NOT _rcb EQUAL 0)
        message(FATAL_ERROR "ladder-gpu-parity: the GPU arm failed on ${_f} "
                            "(rc=${_rcb})\n${_b}")
    endif()
    if(_b MATCHES "available=0")
        if(MCC_GPU_REQUIRED)
            message(FATAL_ERROR "ladder-gpu-parity: no usable GPU device, but "
                                "MCC_GPU_REQUIRED is set -- this cell exists to "
                                "exercise a device and there is none")
        endif()
        message("ladder-gpu-parity: no usable GPU device, skipping")
        cmake_language(EXIT 77)
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

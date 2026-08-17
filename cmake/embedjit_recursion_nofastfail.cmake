if(NOT MCC OR NOT WORK)
    message(FATAL_ERROR "embedjit-recursion-nofastfail: MCC and WORK must be set")
endif()

set(src "${WORK}/embed-recursion.c")
set(out "${WORK}/embed-recursion.exe")
file(WRITE "${src}"
"#include <stdio.h>\n"
"static int fib(int n) { return n < 2 ? n : fib(n-1) + fib(n-2); }\n"
"int main(void) { printf(\"%d\\n\", fib(10)); return 0; }\n")

execute_process(
    COMMAND "${MCC}" -w -O1 --embed-jit --jit-functions fib "${src}" -o "${out}"
    RESULT_VARIABLE crc OUTPUT_VARIABLE cso ERROR_VARIABLE cse)
if(NOT crc EQUAL 0)
    message(FATAL_ERROR "embedjit-recursion-nofastfail: 'mcc --embed-jit' compile exited ${crc}, expected 0\n${cse}${cso}")
endif()

execute_process(
    COMMAND "${out}"
    RESULT_VARIABLE rrc OUTPUT_VARIABLE rso ERROR_VARIABLE rse)
if(NOT rrc EQUAL 0)
    message(FATAL_ERROR "embedjit-recursion-nofastfail: the --embed-jit binary exited ${rrc}, expected 0 -- a recursive function under embed-JIT must not CRT-mismatch fastfail (0xC0000409) in the engine-startup baseline-recompile read path; see T-win-50021 slice-2 (Win32-HANDLE fd-shim)\n${rse}${rso}")
endif()

string(STRIP "${rso}" rso_stripped)
if(NOT rso_stripped STREQUAL "55")
    message(FATAL_ERROR "embedjit-recursion-nofastfail: expected fib(10)=55, got '${rso_stripped}'")
endif()

message(STATUS "embedjit-recursion-nofastfail: recursive embed-JIT binary runs (fib(10)=55), no CRT-mismatch fastfail")

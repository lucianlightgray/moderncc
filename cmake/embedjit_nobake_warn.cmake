if(NOT MCC OR NOT WORK)
    message(FATAL_ERROR "embedjit-nobake-warn: MCC and WORK must be set")
endif()

set(src "${WORK}/embed-nobake-warn.c")
set(out "${WORK}/embed-nobake-warn.out")
file(WRITE "${src}" "int main(void){return 0;}\n")

execute_process(
    COMMAND "${MCC}" -w --embed-jit "${src}" -o "${out}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE so
    ERROR_VARIABLE se)

if(NOT rc EQUAL 0)
    message(FATAL_ERROR "embedjit-nobake-warn: 'mcc -w --embed-jit' exited ${rc}, expected 0 -- the no-bake notice must inform, not fail\n${se}${so}")
endif()

string(FIND "${se}${so}" "no functions were JIT-baked" pos)
if(pos EQUAL -1)
    message(FATAL_ERROR "embedjit-nobake-warn: '-w' suppressed the --embed-jit no-bake notice; a build passing -w gets an engine-less binary silently. Output was:\n${se}${so}")
endif()

message(STATUS "embedjit-nobake-warn: -w did not suppress the no-bake notice")

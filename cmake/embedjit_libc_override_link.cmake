if(NOT MCC OR NOT WORK)
    message(FATAL_ERROR "embedjit-libc-override: MCC and WORK must be set")
endif()

set(src "${WORK}/embed-libc-override.c")
set(out "${WORK}/embed-libc-override.exe")
file(WRITE "${src}"
"#include <stdio.h>\n"
"double strtod(const char *nptr, char **endptr);\n"
"long double strtold(const char *nptr, char **endptr) {\n"
"\treturn (long double)strtod(nptr, endptr);\n"
"}\n"
"extern long double __mingw_strtold(const char *, char **);\n"
"int main(void) {\n"
"\tchar *e;\n"
"\tlong double a = strtold(\"3.5\", &e);\n"
"\tlong double b = __mingw_strtold(\"2.5\", &e);\n"
"\tprintf(\"%d\\n\", (int)((a + b) * 10));\n"
"\treturn 0;\n"
"}\n")

execute_process(
    COMMAND "${MCC}" -w --embed-jit "${src}" -o "${out}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE so
    ERROR_VARIABLE se)

if(so MATCHES "defined twice" OR se MATCHES "defined twice")
    message(FATAL_ERROR "embedjit-libc-override: a program that redefines strtold must not collide with the mingw libmingwex strtopx.o pulled for __mingw_strtold; the program's strong definition must win\n${se}${so}")
endif()

if(NOT rc EQUAL 0)
    message(FATAL_ERROR "embedjit-libc-override: 'mcc --embed-jit' exited ${rc}, expected 0 -- the embed-jit link of a libc-overriding program failed\n${se}${so}")
endif()

message(STATUS "embedjit-libc-override: program strtold definition wins over the archive member's; embed-jit link succeeds")

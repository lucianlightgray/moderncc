# bin2c.cmake — emit a binary file as a C byte array.
# Usage: cmake -DIN=<binfile> -DOUT=<out.c> -DSYM=<symbol> -P bin2c.cmake
# Produces:  const unsigned char <SYM>[]     = { 0x.. , ... };
#            const unsigned int  <SYM>_len   = <n>u;
# Runs at build time (script mode), so IN may be a build product.

if(NOT DEFINED IN OR NOT DEFINED OUT OR NOT DEFINED SYM)
    message(FATAL_ERROR "bin2c.cmake: need -DIN= -DOUT= -DSYM=")
endif()

file(READ "${IN}" _hex HEX)
string(LENGTH "${_hex}" _hexlen)
math(EXPR _n "${_hexlen} / 2")

# "aabbcc" -> "0xaa,0xbb,0xcc," ; newline every 32 bytes keeps lines sane.
string(REGEX REPLACE "(..)" "0x\\1," _body "${_hex}")
string(REGEX REPLACE "((0x..,){16})" "\\1\n" _body "${_body}")

file(WRITE  "${OUT}" "/* Generated from ${IN} by cmake/bin2c.cmake — do not edit. */\n")
file(APPEND "${OUT}" "const unsigned char ${SYM}[] = {\n${_body}\n};\n")
file(APPEND "${OUT}" "const unsigned int ${SYM}_len = ${_n}u;\n")

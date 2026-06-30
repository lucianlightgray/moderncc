# Preprocessor differential test suite (CMake -P port of run_preprocess.sh).
#
# Runs every tests/preprocess/**/*.{c,S} through `mcc -E` and verifies it against
# the gcc==clang consensus, modelled on the diff3 suite:
#
#   * transformation tests (everything outside diagnostics/) — the normalized
#     `mcc -E` token stream must equal the normalized `gcc -E`==`clang -E`
#     output. If gcc and clang themselves disagree there is no consensus to hold
#     mcc to, so the case is SKIPPED (reported, not failed).
#
#   * diagnostics/ tests (negative cases) — `mcc -E` must emit a diagnostic
#     (error or warning). Enforced only when gcc or clang also diagnoses.
#
#   * *.S (asm) — `#` is a GAS comment and #line marker emission differs
#     cosmetically, so just smoke-test that `mcc -E` terminates cleanly.
#
# Invoke under `cmake -P`, passing the .sh's positional args as -D cache vars:
#   -DMCC=<mcc>  -DBDIR=<build(-B)>  -DIDIR=<include(-I)>  -DTDIR=<test-dir>
#   [-DGCC=<gcc>]  [-DCLANG=<clang>]  [-DV=1]
# Exit: 0 if no real divergence, 1 on any FAIL, 77 on whole-suite skip (no
# host gcc/clang) -- matching run_preprocess.sh.
cmake_minimum_required(VERSION 3.20)

foreach(_req MCC BDIR IDIR TDIR)
  if(NOT DEFINED ${_req})
    message(FATAL_ERROR "run_preprocess.cmake: -D${_req}=... is required")
  endif()
endforeach()
if(NOT DEFINED GCC)
  set(GCC gcc)
endif()
if(NOT DEFINED CLANG)
  set(CLANG clang)
endif()
if(NOT DEFINED V)
  set(V 0)
endif()

# command -v "$GCC" / "$CLANG" -- whole-suite skip if either is missing.
find_program(PP_GCC   NAMES "${GCC}")
find_program(PP_CLANG NAMES "${CLANG}")
if(NOT PP_GCC)
  message("SKIP: no gcc")
  cmake_language(EXIT 77)
endif()
if(NOT PP_CLANG)
  message("SKIP: no clang")
  cmake_language(EXIT 77)
endif()

# Normalize a preprocessed token stream: drop line markers (lines beginning
# with #) and blank lines, collapse runs of whitespace, trim ends. Mirrors the
# .sh norm():  grep -v '^#' | grep -v '^[[:space:]]*$' | sed 's/ws+/ /g; trim'.
function(pp_norm raw outvar)
  # Protect content so newline-splitting via the CMake list separator is safe.
  string(REPLACE "\\" "\\\\" s "${raw}")
  string(REPLACE ";" "\\;" s "${s}")
  string(REPLACE "\n" ";" s "${s}")
  set(acc "")
  foreach(line IN LISTS s)
    if(line MATCHES "^#")            # drop preprocessor line markers
      continue()
    endif()
    string(REGEX REPLACE "[ \t\r]+" " " line "${line}")  # collapse whitespace
    string(STRIP "${line}" line)                          # trim ends
    if(line STREQUAL "")             # drop blank / whitespace-only lines
      continue()
    endif()
    string(APPEND acc "${line}\n")
  endforeach()
  set(${outvar} "${acc}" PARENT_SCOPE)
endfunction()

# diag(): does `compiler -E file` emit error/warning on stderr? (stdout dropped)
function(pp_diag compiler file outvar)
  execute_process(
    COMMAND "${compiler}" -E "${file}"
    OUTPUT_QUIET
    ERROR_VARIABLE err
    RESULT_VARIABLE rc)
  string(TOLOWER "${err}" errl)
  if(errl MATCHES "error|warning")
    set(${outvar} TRUE PARENT_SCOPE)
  else()
    set(${outvar} FALSE PARENT_SCOPE)
  endif()
endfunction()

file(GLOB_RECURSE files "${TDIR}/*.c" "${TDIR}/*.S")
list(SORT files)

set(PASS 0)
set(SKIP 0)
set(FAIL 0)
set(FAILED "")

foreach(f IN LISTS files)
  file(RELATIVE_PATH rel "${TDIR}" "${f}")

  if(rel MATCHES "\\.S$")
    # asm: smoke-test only -- `mcc -E` must terminate cleanly, bounded by timeout.
    execute_process(
      COMMAND "${MCC}" "-B${BDIR}" "-I${IDIR}" -E "${f}"
      OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE rc TIMEOUT 10)
    if(rc EQUAL 0)
      math(EXPR PASS "${PASS}+1")
      if(V)
        message("ok   ${rel} (asm -E smoke)")
      endif()
    else()
      math(EXPR FAIL "${FAIL}+1")
      string(APPEND FAILED " ${rel}(asm-E-crash/hang)")
      message("FAIL ${rel}: mcc -E crashed or hung on asm input")
    endif()

  elseif(rel MATCHES "^diagnostics/")
    # negative case: require a diagnostic, gated on a gcc/clang consensus.
    pp_diag("${PP_GCC}"   "${f}" gdiag)
    pp_diag("${PP_CLANG}" "${f}" cdiag)
    if(gdiag OR cdiag)
      execute_process(
        COMMAND "${MCC}" "-B${BDIR}" "-I${IDIR}" -E "${f}"
        OUTPUT_QUIET ERROR_VARIABLE merr RESULT_VARIABLE rc)
      string(TOLOWER "${merr}" merrl)
      if(merrl MATCHES "error|warning")
        math(EXPR PASS "${PASS}+1")
        if(V)
          message("ok   ${rel} (diagnosed)")
        endif()
      else()
        math(EXPR FAIL "${FAIL}+1")
        string(APPEND FAILED " ${rel}(no-diagnostic)")
        message("FAIL ${rel}: mcc emits no diagnostic (gcc/clang do)")
      endif()
    else()
      math(EXPR SKIP "${SKIP}+1")
      if(V)
        message("skip ${rel} (gcc/clang do not diagnose)")
      endif()
    endif()

  else()
    # transformation: normalized mcc -E must equal the gcc==clang consensus.
    execute_process(COMMAND "${PP_GCC}"   -E "${f}"
      OUTPUT_VARIABLE gout ERROR_QUIET RESULT_VARIABLE grc)
    execute_process(COMMAND "${PP_CLANG}" -E "${f}"
      OUTPUT_VARIABLE cout ERROR_QUIET RESULT_VARIABLE crc)
    pp_norm("${gout}" g)
    pp_norm("${cout}" c)
    if(NOT g STREQUAL c)
      math(EXPR SKIP "${SKIP}+1")
      if(V)
        message("skip ${rel} (no gcc==clang consensus)")
      endif()
    else()
      execute_process(COMMAND "${MCC}" "-B${BDIR}" "-I${IDIR}" -E "${f}"
        OUTPUT_VARIABLE mout ERROR_QUIET RESULT_VARIABLE mrc)
      pp_norm("${mout}" m)
      if(m STREQUAL g)
        math(EXPR PASS "${PASS}+1")
        if(V)
          message("ok   ${rel}")
        endif()
      else()
        math(EXPR FAIL "${FAIL}+1")
        string(APPEND FAILED " ${rel}")
        message("FAIL ${rel}: mcc -E diverges from gcc==clang")
      endif()
    endif()
  endif()
endforeach()

message("preprocess-suite: PASS=${PASS} SKIP=${SKIP} FAIL=${FAIL}")
if(NOT FAIL EQUAL 0)
  message("failed:${FAILED}")
  cmake_language(EXIT 1)
endif()
cmake_language(EXIT 0)

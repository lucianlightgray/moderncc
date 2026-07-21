set(_lib "${OUT}.lib")
execute_process(
	COMMAND "${GEN}" "${_lib}" "${MACHINE}" "ADVAPI32.dll" "GetUserNameA"
	RESULT_VARIABLE _r)
if(NOT _r EQUAL 0)
	message(FATAL_ERROR "mkshortimp failed (${_r})")
endif()

execute_process(
	COMMAND "${MCC}" "-B${BDIR}" "${SRC}" "${_lib}" -o "${OUT}"
	RESULT_VARIABLE _r)
if(NOT _r EQUAL 0)
	message(FATAL_ERROR "mcc link against short-import lib failed (${_r})")
endif()

execute_process(COMMAND "${OUT}" RESULT_VARIABLE _r)
if(NOT _r EQUAL 0)
	message(FATAL_ERROR "short-import program returned ${_r} (import did not bind)")
endif()

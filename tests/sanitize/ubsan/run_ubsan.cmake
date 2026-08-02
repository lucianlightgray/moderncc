if(NOT DEFINED MODE)
	message(FATAL_ERROR "run_ubsan.cmake: MODE (trap|clean) required")
endif()

set(_levels -O0 -O1 -O2 -O3)

set(_recover_flags "")
if(MODE STREQUAL "recover")
	set(_recover_flags -fsanitize-recover=undefined)
endif()

foreach(_opt IN LISTS _levels)
	execute_process(
		COMMAND ${EMU} "${MCC}" "-B${BDIR}" -fsanitize=undefined ${_recover_flags} ${_opt} "${SRC}" -o "${OUT}"
		RESULT_VARIABLE _crc OUTPUT_VARIABLE _cout ERROR_VARIABLE _cerr)
	if(NOT _crc EQUAL 0)
		message(FATAL_ERROR "compile failed at ${_opt} (${_crc}):\n${_cout}${_cerr}")
	endif()

	execute_process(
		COMMAND ${EMU} "${OUT}"
		RESULT_VARIABLE _rrc OUTPUT_VARIABLE _rout ERROR_VARIABLE _rerr)

	if(MODE STREQUAL "recover")
		if(NOT _rrc EQUAL 0)
			message(FATAL_ERROR
				"recover program did NOT survive at ${_opt}: exit=${_rrc} "
				"(expected 0; handler should log+return, not trap)")
		endif()
		if(NOT "${_rout}" STREQUAL "${EXPECT}\n")
			message(FATAL_ERROR
				"recover program output mismatch at ${_opt}:\n"
				"  expected: [${EXPECT}]\n  actual:   [${_rout}]")
		endif()
		if(NOT "${_rerr}" MATCHES "UndefinedBehaviorSanitizer: runtime error")
			message(FATAL_ERROR
				"recover handler did NOT log a diagnostic at ${_opt}:\n  stderr=[${_rerr}]")
		endif()
	elseif(MODE STREQUAL "trap")
		if(_rrc GREATER_EQUAL 0 AND _rrc LESS 128)
			message(FATAL_ERROR
				"UB check did NOT fire at ${_opt}: exit=${_rrc}, output=[${_rout}] "
				"(expected a trap: POSIX exit >= 128 or a Windows exception code)")
		endif()
	else()
		if(NOT _rrc EQUAL 0)
			message(FATAL_ERROR
				"clean program trapped at ${_opt}: exit=${_rrc} (unexpected UB check firing)")
		endif()
		if(NOT "${_rout}" STREQUAL "${EXPECT}\n")
			message(FATAL_ERROR
				"clean program output mismatch at ${_opt}:\n"
				"  expected: [${EXPECT}]\n  actual:   [${_rout}]")
		endif()
	endif()
endforeach()

message(STATUS "ubsan ${MODE} ${SRC}: OK across -O0..-O3")

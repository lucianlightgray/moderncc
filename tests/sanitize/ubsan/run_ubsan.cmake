if(NOT DEFINED MODE)
	message(FATAL_ERROR "run_ubsan.cmake: MODE (trap|clean) required")
endif()

set(_levels -O0 -O1 -O2 -O3)

foreach(_opt IN LISTS _levels)
	execute_process(
		COMMAND "${MCC}" "-B${BDIR}" -fsanitize=undefined ${_opt} "${SRC}" -o "${OUT}"
		RESULT_VARIABLE _crc OUTPUT_VARIABLE _cout ERROR_VARIABLE _cerr)
	if(NOT _crc EQUAL 0)
		message(FATAL_ERROR "compile failed at ${_opt} (${_crc}):\n${_cout}${_cerr}")
	endif()

	execute_process(
		COMMAND "${OUT}"
		RESULT_VARIABLE _rrc OUTPUT_VARIABLE _rout ERROR_VARIABLE _rerr)

	if(MODE STREQUAL "trap")
		# A fired trap crashes the process: POSIX raises SIGILL/SIGTRAP (exit
		# 128+signo); Windows raises EXCEPTION_ILLEGAL_INSTRUCTION (0xC000001D,
		# reported by CMake as a negative / >2^31 code). Only a clean small exit
		# (0..127) means the check did NOT fire.
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

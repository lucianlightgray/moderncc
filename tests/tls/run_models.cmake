# Drive the four x86-64 TLS models through mcc's linker. For each model the
# reference compiler (CC) emits the object; mcc links it dynamically, and also
# fully static when STATIC=1. A pattern-match abort ("unexpected R_X86_64_TLSGD
# pattern" etc.) or a wrong runtime value fails the test — pinning the tight
# codegen<->linker coupling so a future change is caught.
#
# Args (via -D): MCC, CC, BDIR, IDIR, SRC, OUTDIR, STATIC.

set(_models global-dynamic local-dynamic initial-exec local-exec)
set(_expect "g=112 l=224\n")

foreach(_m IN LISTS _models)
	set(_obj "${OUTDIR}/tlsmod-${_m}.o")
	execute_process(
		COMMAND "${CC}" -fPIC "-ftls-model=${_m}" -c "${SRC}" -o "${_obj}"
		RESULT_VARIABLE _crc ERROR_VARIABLE _cerr)
	if(NOT _crc EQUAL 0)
		message(FATAL_ERROR "reference compile (${_m}) failed:\n${_cerr}")
	endif()

	set(_links dyn)
	if(STATIC)
		list(APPEND _links sta)
	endif()
	foreach(_lk IN LISTS _links)
		set(_flags "")
		if(_lk STREQUAL sta)
			set(_flags -static)
		endif()
		set(_exe "${OUTDIR}/tlsmod-${_m}-${_lk}")
		execute_process(
			COMMAND "${MCC}" "-B${BDIR}" ${_flags} "${_obj}" -o "${_exe}"
			RESULT_VARIABLE _lrc OUTPUT_VARIABLE _lout ERROR_VARIABLE _lerr)
		if(NOT _lrc EQUAL 0)
			message(FATAL_ERROR "mcc link ${_m}/${_lk} failed (${_lrc}):\n${_lout}${_lerr}")
		endif()
		execute_process(COMMAND "${_exe}" RESULT_VARIABLE _rrc OUTPUT_VARIABLE _rout)
		if(NOT _rrc EQUAL 0)
			message(FATAL_ERROR "${_m}/${_lk} crashed/exited ${_rrc}; output:\n${_rout}")
		endif()
		if(NOT "${_rout}" STREQUAL "${_expect}")
			message(FATAL_ERROR "${_m}/${_lk} output mismatch: expected '${_expect}' got '${_rout}'")
		endif()
		message(STATUS "tls ${_m}/${_lk}: OK")
	endforeach()
endforeach()

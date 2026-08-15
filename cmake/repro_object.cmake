# ci/reproducible-object (T-lin-10374): compile a fixed source with mcc twice
# and require byte-identical objects. An mcc -c object carries no
# .note.gnu.build-id (that is a link-time note), so any difference is genuine
# codegen nondeterminism. Proven determinism lets the checks that want
# reproducibility lean on it; the whole-binary story (build-id + DWARF build
# paths, both benign) is at DETAILS.md#t-lin-10374-resolved.
#   -DMCC=<mcc>  -DSRC=<subject.c>  -DWORK=<scratch dir>
file(MAKE_DIRECTORY "${WORK}")
foreach(_tag a b)
	execute_process(
		COMMAND "${MCC}" -c "${SRC}" -o "${WORK}/repro-${_tag}.o"
		RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
	if(NOT _rc EQUAL 0)
		message(FATAL_ERROR "reproducible-object: compile ${_tag} failed (rc=${_rc}):\n${_out}${_err}")
	endif()
endforeach()
file(SHA256 "${WORK}/repro-a.o" _h1)
file(SHA256 "${WORK}/repro-b.o" _h2)
if(NOT _h1 STREQUAL _h2)
	message(FATAL_ERROR
		"reproducible-object: two mcc -c of ${SRC} produced DIFFERENT objects "
		"(${_h1} != ${_h2}) -- mcc codegen is non-deterministic")
endif()
message(STATUS "reproducible-object: OK, two mcc -c byte-identical (${_h1})")

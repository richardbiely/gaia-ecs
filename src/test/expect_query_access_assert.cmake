execute_process(
	COMMAND "${EXECUTABLE}" "${SCENARIO}"
	RESULT_VARIABLE result
	OUTPUT_VARIABLE output
	ERROR_VARIABLE error)
if(NOT "${result}" STREQUAL "86")
	message(FATAL_ERROR "Expected query presence-only assertion exit 86; got ${result}.\n${output}${error}")
endif()

# Builds Tracy's profiler GUI in a source/build directory shared by all Gaia-ECS
# CMake configurations. TracyClient remains in the active configuration's build tree.

foreach(required_var
    TRACY_UPSTREAM_SOURCE_DIR
    TRACY_SHARED_SOURCE_DIR
    TRACY_PROFILER_BUILD_DIR
    TRACY_PROFILER_OUTPUT_DIR)
  if(NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
    message(FATAL_ERROR "${required_var} is required")
  endif()
endforeach()

if(NOT DEFINED TRACY_EXECUTABLE_SUFFIX)
  set(TRACY_EXECUTABLE_SUFFIX "")
endif()

if(NOT EXISTS "${TRACY_UPSTREAM_SOURCE_DIR}/profiler/CMakeLists.txt")
  message(FATAL_ERROR "Tracy profiler source not found: ${TRACY_UPSTREAM_SOURCE_DIR}")
endif()

execute_process(
  COMMAND git -C "${TRACY_UPSTREAM_SOURCE_DIR}" rev-parse HEAD
  OUTPUT_VARIABLE tracy_upstream_ref
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
if(tracy_upstream_ref STREQUAL "")
  set(tracy_upstream_ref "unknown")
endif()

set(tracy_ref_stamp "${TRACY_SHARED_SOURCE_DIR}/.gaia_tracy_source_ref")
set(tracy_source_needs_refresh ON)
if(EXISTS "${TRACY_SHARED_SOURCE_DIR}/profiler/CMakeLists.txt" AND EXISTS "${tracy_ref_stamp}")
  file(READ "${tracy_ref_stamp}" tracy_shared_ref)
  string(STRIP "${tracy_shared_ref}" tracy_shared_ref)
  if(tracy_shared_ref STREQUAL tracy_upstream_ref)
    set(tracy_source_needs_refresh OFF)
  endif()
endif()

if(tracy_source_needs_refresh)
  message(STATUS "Refreshing shared Tracy profiler source: ${tracy_upstream_ref}")
  file(REMOVE_RECURSE "${TRACY_SHARED_SOURCE_DIR}" "${TRACY_PROFILER_BUILD_DIR}")
  file(MAKE_DIRECTORY "${TRACY_SHARED_SOURCE_DIR}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${TRACY_UPSTREAM_SOURCE_DIR}" "${TRACY_SHARED_SOURCE_DIR}"
    RESULT_VARIABLE tracy_copy_result)
  if(NOT tracy_copy_result EQUAL 0)
    message(FATAL_ERROR "Failed to copy Tracy source into ${TRACY_SHARED_SOURCE_DIR}")
  endif()
  file(WRITE "${tracy_ref_stamp}" "${tracy_upstream_ref}\n")
endif()

set(tracy_profiler_source_dir "${TRACY_SHARED_SOURCE_DIR}/profiler")
set(tracy_profiler_cache "${TRACY_PROFILER_BUILD_DIR}/CMakeCache.txt")
if(EXISTS "${tracy_profiler_cache}")
  file(READ "${tracy_profiler_cache}" tracy_cache)
  string(FIND "${tracy_cache}" "CMAKE_HOME_DIRECTORY:INTERNAL=${tracy_profiler_source_dir}" tracy_home_offset)
  if(tracy_home_offset EQUAL -1)
    message(STATUS "Resetting Tracy profiler build directory for shared source path")
    file(REMOVE_RECURSE "${TRACY_PROFILER_BUILD_DIR}")
  endif()
endif()

file(MAKE_DIRECTORY "${TRACY_PROFILER_BUILD_DIR}")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${tracy_profiler_source_dir}"
    -B "${TRACY_PROFILER_BUILD_DIR}"
    -DCMAKE_BUILD_TYPE=Release
  RESULT_VARIABLE tracy_configure_result)
if(NOT tracy_configure_result EQUAL 0)
  message(FATAL_ERROR "Failed to configure shared Tracy profiler GUI")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    --build "${TRACY_PROFILER_BUILD_DIR}"
    --config Release
  RESULT_VARIABLE tracy_build_result)
if(NOT tracy_build_result EQUAL 0)
  message(FATAL_ERROR "Failed to build shared Tracy profiler GUI")
endif()

set(tracy_profiler_binary_candidates
  "${TRACY_PROFILER_BUILD_DIR}/tracy-profiler${TRACY_EXECUTABLE_SUFFIX}"
  "${TRACY_PROFILER_BUILD_DIR}/Release/tracy-profiler${TRACY_EXECUTABLE_SUFFIX}"
  "${TRACY_PROFILER_BUILD_DIR}/RelWithDebInfo/tracy-profiler${TRACY_EXECUTABLE_SUFFIX}"
)

set(tracy_profiler_binary "")
foreach(candidate IN LISTS tracy_profiler_binary_candidates)
  if(EXISTS "${candidate}")
    set(tracy_profiler_binary "${candidate}")
    break()
  endif()
endforeach()

if(tracy_profiler_binary STREQUAL "")
  message(FATAL_ERROR "Built Tracy profiler executable was not found in ${TRACY_PROFILER_BUILD_DIR}")
endif()

file(MAKE_DIRECTORY "${TRACY_PROFILER_OUTPUT_DIR}")
set(tracy_profiler_stable_binary "${TRACY_PROFILER_OUTPUT_DIR}/tracy-profiler${TRACY_EXECUTABLE_SUFFIX}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${tracy_profiler_binary}"
    "${tracy_profiler_stable_binary}"
  RESULT_VARIABLE tracy_copy_binary_result)
if(NOT tracy_copy_binary_result EQUAL 0)
  message(FATAL_ERROR "Failed to copy Tracy profiler to ${tracy_profiler_stable_binary}")
endif()

file(WRITE "${TRACY_PROFILER_OUTPUT_DIR}/tracy-profiler.path" "${tracy_profiler_stable_binary}\n")

if(DEFINED TRACY_PROFILER_STAMP AND NOT "${TRACY_PROFILER_STAMP}" STREQUAL "")
  file(WRITE "${TRACY_PROFILER_STAMP}" "${tracy_upstream_ref}\n${tracy_profiler_stable_binary}\n")
endif()

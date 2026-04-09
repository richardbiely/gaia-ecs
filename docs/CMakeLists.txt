find_package(Doxygen QUIET)

if(NOT DOXYGEN_FOUND)
  message(WARNING "Doxygen not found. Documentation target won't be generated.")
  return()
endif()

include(FetchContent)

FetchContent_Declare(
  doxygen-awesome-css
  URL https://github.com/jothepro/doxygen-awesome-css/archive/refs/tags/v2.4.2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(doxygen-awesome-css)
FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR DOXY_CSS_DIRECTORY)

set(DOXY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(DOXY_HTML_DIRECTORY ${DOXY_OUTPUT_DIRECTORY}/html)
set(DOXY_HEADER ${DOXY_OUTPUT_DIRECTORY}/header.html)
set(DOXY_VERSIONS_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/write_versions_json.cmake)

configure_file(Doxyfile ${DOXY_OUTPUT_DIRECTORY}/Doxyfile @ONLY)
configure_file(header.html.in ${DOXY_HEADER} @ONLY)

file(WRITE ${DOXY_VERSIONS_SCRIPT} [=[
if(NOT DEFINED HTML_ROOT)
  message(FATAL_ERROR "HTML_ROOT must be provided")
endif()

if(NOT EXISTS "${HTML_ROOT}")
  message(FATAL_ERROR "HTML_ROOT does not exist: ${HTML_ROOT}")
endif()

if(NOT DEFINED PROJECT_VERSION)
  set(PROJECT_VERSION "unknown")
endif()

set(_versions_json
  "{\n"
  "  \"latest\": { \"label\": \"${PROJECT_VERSION} (local build)\", \"path\": \"./\", \"version_path\": \"./\" },\n"
  "  \"versions\": []\n"
  "}\n"
)
file(WRITE "${HTML_ROOT}/versions.json" "${_versions_json}")
]=])

add_custom_target(docs
  COMMENT "Generating Doxygen documentation in ${DOXY_OUTPUT_DIRECTORY}/docs using ${DOXYGEN_EXECUTABLE}"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${DOXYGEN_EXECUTABLE} ${DOXY_OUTPUT_DIRECTORY}/Doxyfile
  COMMAND ${CMAKE_COMMAND} -DHTML_ROOT=${DOXY_HTML_DIRECTORY} -DPROJECT_VERSION=${PROJECT_VERSION} -P ${DOXY_VERSIONS_SCRIPT}
  VERBATIM
)

if(GAIA_INSTALL)
  install(
    DIRECTORY ${DOXY_OUTPUT_DIRECTORY}/html
    DESTINATION share/${PROJECT_NAME}-${PROJECT_VERSION}/
  )
endif()

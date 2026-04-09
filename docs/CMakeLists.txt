find_package(Doxygen QUIET)

if(NOT DOXYGEN_FOUND)
  message(WARNING "Doxygen not found. Documentation target won't be generated.")
  return()
endif()

include(FetchContent)

FetchContent_Declare(
  doxygen-awesome-css
  GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css
  GIT_TAG main
  GIT_SHALLOW 1
)

FetchContent_MakeAvailable(doxygen-awesome-css)

set(DOXY_CSS_DIRECTORY ${doxygen-awesome-css_SOURCE_DIR})
set(DOXY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(DOXY_HTML_DIRECTORY ${DOXY_OUTPUT_DIRECTORY}/html)
set(DOXY_SELECTOR_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/inject_version_selector.cmake)

message("DOXY_CSS_DIRECTORY: ${doxygen-awesome-css_SOURCE_DIR}")
message("DOXY_OUTPUT_DIRECTORY: ${DOXY_OUTPUT_DIRECTORY}")

configure_file(Doxyfile ${DOXY_OUTPUT_DIRECTORY}/Doxyfile @ONLY)

file(WRITE ${DOXY_SELECTOR_SCRIPT} [=[
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

set(_selector_payload
  "window.GAIA_VERSION_SELECTOR_DATA = { latest: { label: \"${PROJECT_VERSION} (local build)\", path: \"./\", version_path: \"./\" }, versions: [] };"
)

function(_gaia_rel_prefix out_var html_root html_file)
  file(RELATIVE_PATH _rel "${html_root}" "${html_file}")
  get_filename_component(_dir "${_rel}" DIRECTORY)

  if(_dir STREQUAL "" OR _dir STREQUAL ".")
    set(${out_var} "./" PARENT_SCOPE)
    return()
  endif()

  string(REPLACE "\\" "/" _dir "${_dir}")
  string(REPLACE "/" ";" _parts "${_dir}")

  set(_prefix "")
  foreach(_part IN LISTS _parts)
    if(NOT _part STREQUAL "" AND NOT _part STREQUAL ".")
      string(APPEND _prefix "../")
    endif()
  endforeach()

  if(_prefix STREQUAL "")
    set(_prefix "./")
  endif()

  set(${out_var} "${_prefix}" PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE _html_files LIST_DIRECTORIES false "${HTML_ROOT}/*.html")

foreach(_html_file IN LISTS _html_files)
  file(READ "${_html_file}" _html)

  if(_html MATCHES "gaia-version-selector\\.css" OR _html MATCHES "gaia-version-selector\\.js")
    continue()
  endif()

  string(FIND "${_html}" "</head>" _head_pos)
  if(_head_pos EQUAL -1)
    message(FATAL_ERROR "${_html_file} does not contain </head>")
  endif()

  _gaia_rel_prefix(_prefix "${HTML_ROOT}" "${_html_file}")
  string(CONCAT _assets
    "  <link rel=\"stylesheet\" href=\"${_prefix}gaia-version-selector.css\">\n"
    "  <script>${_selector_payload}</script>\n"
    "  <script defer src=\"${_prefix}gaia-version-selector.js\"></script>\n"
  )

  string(REPLACE "</head>" "${_assets}</head>" _html "${_html}")
  file(WRITE "${_html_file}" "${_html}")
endforeach()
]=])

add_custom_target(docs ALL
  COMMENT "Generating Doxygen documentation in ${DOXY_OUTPUT_DIRECTORY}/docs using ${DOXYGEN_EXECUTABLE}"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${DOXYGEN_EXECUTABLE} ${DOXY_OUTPUT_DIRECTORY}/Doxyfile
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/site/gaia-version-selector.css ${DOXY_HTML_DIRECTORY}/gaia-version-selector.css
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/site/gaia-version-selector.js ${DOXY_HTML_DIRECTORY}/gaia-version-selector.js
  COMMAND ${CMAKE_COMMAND} -DHTML_ROOT=${DOXY_HTML_DIRECTORY} -DPROJECT_VERSION=${PROJECT_VERSION} -P ${DOXY_SELECTOR_SCRIPT}
  VERBATIM
)

if(GAIA_INSTALL)
  install(
    DIRECTORY ${DOXY_OUTPUT_DIRECTORY}/html
    DESTINATION share/${PROJECT_NAME}-${PROJECT_VERSION}/
  )
endif()

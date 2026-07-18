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

set(DOXY_JQUERY_SCRIPT "")
if(DOXYGEN_VERSION VERSION_LESS "1.17.0")
  set(DOXY_JQUERY_SCRIPT "<script type=\"text/javascript\" src=\"\$relpath^jquery.js\"></script>")
endif()

# Older distro-packaged Doxygen releases do not understand a subset of the newer
# config tags and HTML template markers used by the current docs theme. When that
# happens, the warnings are noisy but harmless for most tags; the problem is that
# unsupported header markers such as PROJECT_ICON and COPY_CLIPBOARD are left
# unexpanded in header.html and Doxygen aborts the docs build.
#
# To keep the docs target working across both newer local installs and older CI
# packages, we configure the full template first and then strip only the known
# incompatible pieces for legacy Doxygen versions.
set(GAIA_DOXYGEN_COMPAT_TAGS
  PROJECT_ICON
  MARKDOWN_STRICT
  AUTOLINK_IGNORE_WORDS
  HIDE_UNDOC_NAMESPACES
  EXTERNAL_TOOL_PATH
  WARN_LAYOUT_FILE
  IMPLICIT_DIR_DOCS
  HTML_COPY_CLIPBOARD
  HTML_PROJECT_COOKIE
  PAGE_OUTLINE_PANEL
  SHOW_ENUM_VALUES
  RTF_EXTRA_FILES
  UML_MAX_EDGE_LABELS
  PLANTUMLFILE_DIRS
)

set(GAIA_DOXYGEN_LEGACY_COMPAT OFF)
if(DOXYGEN_VERSION VERSION_LESS "1.10.0")
  set(GAIA_DOXYGEN_LEGACY_COMPAT ON)
  message(STATUS "Doxygen ${DOXYGEN_VERSION} detected, enabling legacy docs compatibility sanitization.")
endif()

configure_file(Doxyfile ${DOXY_OUTPUT_DIRECTORY}/Doxyfile @ONLY)
configure_file(header.html.in ${DOXY_HEADER} @ONLY)

if(GAIA_DOXYGEN_LEGACY_COMPAT)
  file(READ ${DOXY_OUTPUT_DIRECTORY}/Doxyfile _gaia_doxyfile)
  foreach(_gaia_tag IN LISTS GAIA_DOXYGEN_COMPAT_TAGS)
    string(REGEX REPLACE "(^|[\r\n])${_gaia_tag}[ \t]*=[^\r\n]*" "\\1" _gaia_doxyfile "${_gaia_doxyfile}")
  endforeach()
  file(WRITE ${DOXY_OUTPUT_DIRECTORY}/Doxyfile "${_gaia_doxyfile}")

  file(READ ${DOXY_HEADER} _gaia_header)
  string(REPLACE "<!--BEGIN PROJECT_ICON-->
<link rel=\"icon\" href=\"$relpath^$projecticon\" type=\"image/x-icon\" />
<!--END PROJECT_ICON-->
" "" _gaia_header "${_gaia_header}")
  string(REPLACE "<!--BEGIN COPY_CLIPBOARD-->
<script type=\"text/javascript\" src=\"$relpath^clipboard.js\"></script>
<!--END COPY_CLIPBOARD-->
" "" _gaia_header "${_gaia_header}")
  file(WRITE ${DOXY_HEADER} "${_gaia_header}")
endif()

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

string(CONCAT _versions_json
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

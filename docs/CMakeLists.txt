find_package(Doxygen REQUIRED)

if(DOXYGEN_FOUND)
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

  message("DOXY_CSS_DIRECTORY: ${doxygen-awesome-css_SOURCE_DIR}")
  message("DOXY_OUTPUT_DIRECTORY: ${DOXY_OUTPUT_DIRECTORY}")

  configure_file(Doxyfile ${DOXY_OUTPUT_DIRECTORY}/Doxyfile @ONLY)

  add_custom_target(docs ALL
    COMMENT "Generating Doxygen documentation in ${DOXY_OUTPUT_DIRECTORY}/docs using ${DOXYGEN_EXECUTABLE}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXY_OUTPUT_DIRECTORY}/Doxyfile
    VERBATIM
  )

  if(GAIA_INSTALL)
    install(
      DIRECTORY ${DOXY_OUTPUT_DIRECTORY}/html
      DESTINATION share/${PROJECT_NAME}-${PROJECT_VERSION}/
    )
  endif()
else()
  message(WARNING "Doxygen not found. Documentation can't be generated")
endif()
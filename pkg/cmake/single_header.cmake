# ---------------------------------------------------------------------------
# CMakeLists.txt  –  single-header generation block
#
# Uses the shell include-walker (make_single_header.sh / .bat) which inlines
# only headers found under include/, passing everything else through verbatim.
# The output is then formatted with clang-format.
# ---------------------------------------------------------------------------

option(GAIA_GENERATE_SINGLE_HEADER "Generate the single file header automatically." OFF)

if(GAIA_GENERATE_SINGLE_HEADER)
    set(GAIA_SH_INPUT "${CMAKE_SOURCE_DIR}/include/gaia.h")
    set(GAIA_SH_OUTPUT "${CMAKE_SOURCE_DIR}/single_include/gaia.h")

    # -----------------------------------------------------------------------
    # Require clang-format
    # -----------------------------------------------------------------------
    find_program(CLANG_FORMAT_EXE
        NAMES clang-format
        clang-format-19 clang-format-18 clang-format-17
        DOC "clang-format executable used to format the single-header output"
    )

    if(NOT CLANG_FORMAT_EXE)
        message(FATAL_ERROR
            "GAIA_GENERATE_SINGLE_HEADER is ON but clang-format was not found. "
            "Install clang-format or set CLANG_FORMAT_EXE manually.")
    endif()

    message(STATUS "Single-header generator: shell include-walker")
    message(STATUS "clang-format: ${CLANG_FORMAT_EXE}")

    # -----------------------------------------------------------------------
    # Glob all headers so the command reruns when any of them change
    # -----------------------------------------------------------------------
    file(GLOB_RECURSE GAIA_ALL_HEADERS
        "${CMAKE_SOURCE_DIR}/include/*.h"
        "${CMAKE_SOURCE_DIR}/include/*.hpp"
        "${CMAKE_SOURCE_DIR}/include/*.inl"
    )

    # -----------------------------------------------------------------------
    # Select the script for the host platform
    # -----------------------------------------------------------------------
    if(CMAKE_HOST_WIN32)
        set(_script "${CMAKE_SOURCE_DIR}/make_single_header.bat")
        set(_amalgam_cmd "${_script}" "${CLANG_FORMAT_EXE}")
    else()
        set(_script "${CMAKE_SOURCE_DIR}/make_single_header.sh")
        set(_amalgam_cmd bash "${_script}" "${CLANG_FORMAT_EXE}")
    endif()

    # -----------------------------------------------------------------------
    # Custom command: amalgamate and format in one script invocation
    # clang-format is passed explicitly so the script uses the same binary
    # that CMake found, rather than whatever happens to be first on PATH.
    # -----------------------------------------------------------------------
    add_custom_command(
        OUTPUT "${GAIA_SH_OUTPUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_SOURCE_DIR}/single_include"
        COMMAND ${_amalgam_cmd}
        DEPENDS ${GAIA_ALL_HEADERS} "${_script}"
        COMMENT "Generating and formatting single_include/gaia.h"
        VERBATIM
    )

    # Explicit target: cmake --build build --target single_header
    add_custom_target(single_header ALL
        DEPENDS "${GAIA_SH_OUTPUT}"
    )

    message(STATUS "single_header target registered (output: ${GAIA_SH_OUTPUT})")
endif() # GAIA_GENERATE_SINGLE_HEADER

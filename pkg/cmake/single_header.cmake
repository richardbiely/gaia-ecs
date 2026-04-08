# ---------------------------------------------------------------------------
# CMakeLists.txt  –  single-header generation block
#
# Uses the shell include-walker (make_single_header.sh / .bat) which inlines
# only headers found under include/, passing everything else through verbatim.
# The output is formatted with clang-format when it is available.
# ---------------------------------------------------------------------------

option(GAIA_GENERATE_SINGLE_HEADER "Generate the single file header automatically." ON)

if(GAIA_GENERATE_SINGLE_HEADER)
    set(GAIA_SH_INPUT "${CMAKE_SOURCE_DIR}/include/gaia.h")
    set(GAIA_SH_OUTPUT "${CMAKE_SOURCE_DIR}/single_include/gaia.h")

    # -----------------------------------------------------------------------
    # Discover clang-format if it is available
    # -----------------------------------------------------------------------
    find_program(CLANG_FORMAT_EXE
        NAMES clang-format
        clang-format-19 clang-format-18 clang-format-17
        DOC "clang-format executable used to format the single-header output"
    )

    message(STATUS "Single-header generator: shell include-walker")
    if(CLANG_FORMAT_EXE)
        message(STATUS "clang-format: ${CLANG_FORMAT_EXE}")
    else()
        message(WARNING "clang-format was not found. single_include/gaia.h will be generated without formatting.")
    endif()

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
        set(_amalgam_cmd "${_script}")
    else()
        set(_script "${CMAKE_SOURCE_DIR}/make_single_header.sh")
        set(_amalgam_cmd bash "${_script}")
    endif()

    if(CLANG_FORMAT_EXE)
        list(APPEND _amalgam_cmd "${CLANG_FORMAT_EXE}")
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
        COMMENT "Generating single_include/gaia.h"
        VERBATIM
    )

    # Explicit target: cmake --build build --target single_header
    add_custom_target(single_header ALL
        DEPENDS "${GAIA_SH_OUTPUT}"
    )

    message(STATUS "single_header target registered (output: ${GAIA_SH_OUTPUT})")
endif() # GAIA_GENERATE_SINGLE_HEADER

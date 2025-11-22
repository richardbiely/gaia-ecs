if(!GAIA_BUILD_SRC)
  return()
endif()

# Library configuration
if(GAIA_DEVMODE)
  add_definitions(-DGAIA_DEVMODE=1)
else()
  add_definitions(-DGAIA_DEVMODE=0)
endif()

if(GAIA_ECS_CHUNK_ALLOCATOR)
  add_definitions(-DGAIA_ECS_CHUNK_ALLOCATOR=1)
else()
  add_definitions(-DGAIA_ECS_CHUNK_ALLOCATOR=0)
endif()

if(GAIA_FORCE_DEBUG)
  add_definitions(-DGAIA_FORCE_DEBUG=1)
else()
  add_definitions(-DGAIA_FORCE_DEBUG=0)
endif()

if(GAIA_DISABLE_ASSERTS)
  add_definitions(-DGAIA_DISABLE_ASSERTS=1)
else()
  add_definitions(-DGAIA_DISABLE_ASSERTS=0)
endif()

# Compiler settings
include(CheckCXXCompilerFlag)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_REQUIRED_QUIET TRUE)

message(STATUS "Compiling for C++${CMAKE_CXX_STANDARD}")

function(enable_cxx_compiler_flag_if_supported flag)
  string(FIND "${CMAKE_CXX_FLAGS}" "${flag}" flag_already_set)

  if(flag_already_set EQUAL -1)
    check_cxx_compiler_flag("${flag}" flag_supported)

    if(flag_supported)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
    endif()

    unset(flag_supported CACHE)
  endif()
endfunction()

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.29")
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND UNIX AND NOT APPLE)
    # Use mold as the default linker for clang if possible
    find_program(MOLD_LINKER mold)

    if(MOLD_LINKER)
      message(STATUS "Linker: ${MOLD_LINKER} MOLD")
      set(CMAKE_LINKER_TYPE "MOLD")
    endif()
  endif()
else()
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND UNIX AND NOT APPLE)
    # Use mold as the default linker for clang if possible
    find_program(MOLD_LINKER mold)

    if(MOLD_LINKER)
      message(STATUS "Linker: ${MOLD_LINKER} MOLD")
      add_link_options("-fuse-ld=${MOLD_LINKER}")
    else()
      # # LLD uses all system threads by default. Older vesions than 11 can't tweak this,
      # # later versions can do --threads=${HOST_PROC_COUNT}.
      # #
      # # include(ProcessorCount)
      # # ProcessorCount(HOST_PROC_COUNT)
      # # LLD>=11
      # # add_link_options("-fuse-ld=lld-${CLANG_VERSION_MAJOR};LINKER:--threads=${HOST_PROC_COUNT}")
      # # LLD<11
      # # add_link_options("-fuse-ld=lld;LINKER:--threads") (default state)
      # #
      # # Anyways, we want to use the default settings.
      # string(REPLACE "." ";" VERSION_LIST ${CMAKE_CXX_COMPILER_VERSION})
      # list(GET VERSION_LIST 0 CLANG_VERSION_MAJOR) # extract major compiler version

      # # Search for a given version of clang. E.g. lld-13 when clang 13 is used.
      # # Search for the default version of clang, too.
      # find_program(LLD_PROGRAM_MATCH_VER lld-${CLANG_VERSION_MAJOR})
      # find_program(LLD_PROGRAM lld)

      # if(LLD_PROGRAM_MATCH_VER) # lld matching compiler version
      # message(STATUS "Linker: ${LLD_PROGRAM_MATCH_VER} LLD")
      # add_link_options("-fuse-ld=lld-${CLANG_VERSION_MAJOR}")
      # elseif(LLD_PROGRAM) # default lld
      # message(STATUS "Linker: ${LLD_PROGRAM} LLD")
      # add_link_options("-fuse-ld=lld")
      # endif(LLD_PROGRAM_MATCH_VER)
    endif()

  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # Use gold as the default linker for GNU if possible
    find_program(GNU_GOLD_PROGRAM gold)

    if(GNU_GOLD_PROGRAM)
      # Find the number of cores we can use
      include(ProcessorCount)
      ProcessorCount(HOST_PROC_COUNT)

      if(NOT HOST_PROC_COUNT EQUAL 0)
        message(STATUS "Linker: ${GNU_GOLD_PROGRAM}, GOLD")
        add_link_options("-fuse-ld=gold")
      else()
        message(STATUS "Linker: ${GNU_GOLD_PROGRAM}, GOLD (threads: ${HOST_PROC_COUNT})")
        add_link_options("-fuse-ld=gold;LINKER:--threads,--thread-count=${HOST_PROC_COUNT}")
      endif()
    endif(GNU_GOLD_PROGRAM)
  endif()
endif()

if(MSVC)
  # parallel build
  enable_cxx_compiler_flag_if_supported("/MP")

  # strictness rules
  enable_cxx_compiler_flag_if_supported("/W4")
  enable_cxx_compiler_flag_if_supported("/WX")

  # instruction sets
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    enable_cxx_compiler_flag_if_supported("-msse4.1")
    enable_cxx_compiler_flag_if_supported("-mfma")
  else()
    if(NOT IsARM)
      enable_cxx_compiler_flag_if_supported("/arch:AVX2")
    endif()
  endif()

  enable_cxx_compiler_flag_if_supported("/permissive-")

  # Link MSVC projects against static runtime library to avoid issues
  # with running the programs on some platforms (e.g. virtual machines).
  foreach(flag_var
    CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
    CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_MINSIZEREL
    CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
    CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
    string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    string(REPLACE "/MDd" "/MTd" ${flag_var} "${${flag_var}}")
  endforeach()
else()
  # MacOS
  if(GAIA_MACOS_BUILD_HACK AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    # Workaround for the new linker starting with XCode 15
    # https://developer.apple.com/documentation/xcode-release-notes/xcode-15-release-notes#Linking
    # We need to use -Wl,-ld_classic for the linker for the backwards compatibility when buiding
    # outside of XCode because they messed things up.
    if(NOT CMAKE_GENERATOR MATCHES "Xcode")
      # Make sure xcodebuild is installed. Otherwise we won't be able to tell is the workaround
      # needs to be applied
      find_program(XCODEBUILD_EXECUTABLE xcodebuild)

      if(XCODEBUILD_EXECUTABLE)
        execute_process(COMMAND xcodebuild -version OUTPUT_VARIABLE XCODE_VERSION_STRING OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(REGEX MATCH "([0-9]+)\\.[0-9]+" XCODE_MAJOR_VERSION ${XCODE_VERSION_STRING})

        if(XCODE_MAJOR_VERSION GREATER_EQUAL "15")
          add_link_options("-Wl,-ld_classic")
        endif()
      endif()
    endif()
  endif()

  # strictness rules
  # Tracy profiler is not written with strictness and warnings-as-errors in mind
  # so rather than reading spam in the build output we disable any strictness
  # rules and deprecation warnings when profiling.
  if(!GAIA_PROFILER_CPU AND !GAIA_PROFILER_MEM)
    enable_cxx_compiler_flag_if_supported("-Wall")
    enable_cxx_compiler_flag_if_supported("-Wextra")
    enable_cxx_compiler_flag_if_supported("-Wextra-semi")
    enable_cxx_compiler_flag_if_supported("-Wextra-tokens")
    enable_cxx_compiler_flag_if_supported("-pedantic")
    enable_cxx_compiler_flag_if_supported("-pedantic-errors")
    enable_cxx_compiler_flag_if_supported("-Wpadded")
    enable_cxx_compiler_flag_if_supported("-Wpadded-bitfield")
    enable_cxx_compiler_flag_if_supported("-Werror")
    enable_cxx_compiler_flag_if_supported("-Wshadow")
    enable_cxx_compiler_flag_if_supported("-Wcast-align")
    enable_cxx_compiler_flag_if_supported("-Wunused")
    enable_cxx_compiler_flag_if_supported("-Wconversion")
    enable_cxx_compiler_flag_if_supported("-Wsign-conversion")
    enable_cxx_compiler_flag_if_supported("-Wnull-dereference")
    enable_cxx_compiler_flag_if_supported("-Wdouble-promotion")
    enable_cxx_compiler_flag_if_supported("-Wformat=2")
    enable_cxx_compiler_flag_if_supported("-Wimplicit-fallthrough")
    enable_cxx_compiler_flag_if_supported("-Wduplicated-cond")
    enable_cxx_compiler_flag_if_supported("-Wduplicated-branches")
    enable_cxx_compiler_flag_if_supported("-Wlogical-op")
    enable_cxx_compiler_flag_if_supported("-Wuseless-cast")
    enable_cxx_compiler_flag_if_supported("-Wunused-parameter")
    enable_cxx_compiler_flag_if_supported("-Wunused-local-typedef")
  else()
    enable_cxx_compiler_flag_if_supported("-Wno-deprecated-declarations")
  endif()

  # instruction sets
  enable_cxx_compiler_flag_if_supported("-ftree-vectorize")

  if(NOT IsARM)
    enable_cxx_compiler_flag_if_supported("-msse4.1")
    enable_cxx_compiler_flag_if_supported("-mfma")
  endif()
endif()

# Turn off RTTI and exception handling
# Ideally we would want something like following:
# set(CMAKE_CXX_EXCEPTIONS OFF)
# set(CMAKE_CXX_RTTI OFF)
# https://gitlab.kitware.com/cmake/cmake/-/merge_requests/4634
# However, it does not look like it is ever going to happen so let us do things manually.
if(MSVC)
  # Turn off RTTI
  string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

  # Turn off exceptions (including in STL)
  string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  add_definitions(-D_HAS_EXCEPTIONS=0)
else()
  # Turn off RTTI
  enable_cxx_compiler_flag_if_supported("-fno-rtti")

  # Turn off exceptions (including in STL)
  enable_cxx_compiler_flag_if_supported("-fno-exceptions")
endif()

if(NOT EMSCRIPTEN)
  set(IS_FETCH_AVAILABLE OFF)

  if(GAIA_BUILD_BENCHMARK OR GAIA_BUILD_UNITTEST OR GAIA_PROFILER_CPU OR GAIA_PROFILER_MEM OR GAIA_MAKE_SINGLE_HEADER)
    find_package(Git REQUIRED)
    Include(FetchContent OPTIONAL RESULT_VARIABLE FetchContent_FOUND)

    if(NOT FetchContent_FOUND)
      message(SEND_ERROR "FetchContent might be necessary but it is not available")
    else()
      set(IS_FETCH_AVAILABLE ON)
    endif()
  endif()

  if(GAIA_PROFILER_CPU OR GAIA_PROFILER_MEM OR GAIA_PROFILER_BUILD)
    if(IS_FETCH_AVAILABLE)
      find_package(tracy CONFIG QUIET 0.13.0)

      if(NOT tracy_FOUND)
        FetchContent_Declare(tracy
          GIT_REPOSITORY https://github.com/wolfpld/tracy.git
          GIT_TAG v0.13.0
          GIT_SHALLOW TRUE
        )

        set(TRACY_ENABLE ON CACHE BOOL "TRACY_ENABLE override")
        set(TRACY_ON_DEMAND ON CACHE BOOL "TRACY_ON_DEMAND override")

        FetchContent_MakeAvailable(tracy)
      endif()
    else()
      find_package(tracy CONFIG REQUIRED 0.13.0)
    endif()

    if(GAIA_PROFILER_CPU)
      add_compile_definitions(GAIA_PROFILER_CPU=1)
    endif()

    if(GAIA_PROFILER_MEM)
      add_compile_definitions(GAIA_PROFILER_MEM=1)
    endif()

    if(GAIA_PROFILER_CPU OR GAIA_PROFILER_MEM)
      link_libraries(TracyClient)
    endif()

    if(GAIA_PROFILER_BUILD)
      # Add a custom target to build the Tracy profiler GUI
      if(UNIX)
        set(tracy_profiler_build_dir "build/unix")
        message(STATUS "Building profiler for UNIX platform in: " ${tracy_profiler_build_dir})
      elseif(WIN32)
        set(tracy_profiler_build_dir "build/win32")
        message(STATUS "Building profiler for Windows platform in: " ${tracy_profiler_build_dir})
      else()
        message(SEND_ERROR "Unsupported platform. Building profiler won't be possible")
      endif()

      if(DEFINED tracy_profiler_build_dir)
        add_custom_target(build_tracy_profiler_gui ALL
          COMMAND ${CMAKE_COMMAND} -E make_directory ${tracy_profiler_build_dir}
          COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Release -B ${tracy_profiler_build_dir} -S .
          COMMAND ${CMAKE_COMMAND} --build ${tracy_profiler_build_dir} --config Release
          WORKING_DIRECTORY ${tracy_SOURCE_DIR}/profiler
        )
      endif()
    endif()
  endif()

  # Amalgamate currently does not build for Win+ARM properly because it requires the __cpuid intrinsic
  # if(GAIA_MAKE_SINGLE_HEADER AND NOT (WIN32 AND IsARM))
  if(GAIA_MAKE_SINGLE_HEADER)
    if(IS_FETCH_AVAILABLE)
      find_package(amalgamate CONFIG QUIET 0.99.0)

      if(NOT amalgamate_FOUND)
        FetchContent_Declare(
          amalgamate
          GIT_REPOSITORY https://github.com/rindeal/Amalgamate.git
          GIT_TAG v0.99.0
          GIT_SHALLOW TRUE
          GIT_SUBMODULES_RECURSE TRUE
        )

        FetchContent_MakeAvailable(amalgamate)
      endif()
    else()
      find_package(amalgamate CONFIG REQUIRED 0.99.0)
    endif()

    # Add a custom target to build amalgamate
    if(UNIX)
      message(STATUS "Building amalgamate for Unix platform in: " ${amalgamate_SOURCE_DIR})

      add_custom_target(build_amalgamate ALL
        WORKING_DIRECTORY ${amalgamate_SOURCE_DIR}
        COMMAND make
      )
    elseif(WIN32)
      message(STATUS "Building amalgamate for Windows platform in: " ${amalgamate_SOURCE_DIR})
      add_custom_target(build_amalgamate ALL
        WORKING_DIRECTORY ${amalgamate_SOURCE_DIR}
        COMMAND build.bat
      )

    else()
      message(SEND_ERROR "Unsupported platform. Building amalgamate won't be possible")
    endif()

    add_custom_target(generate_single_header ALL)

    if(WIN32)
      set(generate_single_header_path "make_single_header.bat")
    else()
      set(generate_single_header_path "./make_single_header.sh")
    endif()

    add_custom_command(
      TARGET generate_single_header POST_BUILD
      COMMAND ${generate_single_header_path} ${amalgamate_SOURCE_DIR}
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      COMMENT "Merging files into a single header"
    )
  endif()

  if(GAIA_GENERATE_CC)
    add_custom_target(generate_compile_commands ALL)
    add_custom_command(
      COMMENT "Generating compile_commands.json in: ${CMAKE_BINARY_DIR}/ninja"
      TARGET generate_compile_commands POST_BUILD
      COMMAND ${CMAKE_COMMAND}
      -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -G "Ninja" -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_BINARY_DIR}/ninja
      -DGAIA_USE_SANITIZER=${GAIA_USE_SANITIZER}
      -DGAIA_PROFILER_CPU=${GAIA_PROFILER_CPU}
      -DGAIA_PROFILER_MEM=${GAIA_PROFILER_MEM}
      -DGAIA_DEVMODE=${GAIA_DEVMODE}
      -DGAIA_ECS_CHUNK_ALLOCATOR=${GAIA_ECS_CHUNK_ALLOCATOR}
      -DGAIA_FORCE_DEBUG=${GAIA_FORCE_DEBUG}
      -DGAIA_DISABLE_ASSERTS=${GAIA_DISABLE_ASSERTS}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/ninja
      COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/ninja/compile_commands.json ${CMAKE_SOURCE_DIR}/ninja/compile_commands.json
    )
  endif()

  if(GAIA_BUILD_BENCHMARK)
    if(IS_FETCH_AVAILABLE)
      find_package(picobench CONFIG QUIET 2.08)

      if(NOT picobench_FOUND)
        FetchContent_Declare(
          picobench
          GIT_REPOSITORY https://github.com/iboB/picobench.git
          GIT_TAG v2.08
        )

        FetchContent_MakeAvailable(picobench)
      endif()
    endif()
  endif()

  if(GAIA_BUILD_UNITTEST)
    if(IS_FETCH_AVAILABLE)
      if(NOT doctest_FOUND)
        FetchContent_Declare(
          doctest
          GIT_REPOSITORY "https://github.com/onqtam/doctest.git"
          GIT_TAG v2.4.12
        )

        FetchContent_MakeAvailable(doctest)
      endif()
    else()
      find_package(doctest CONFIG REQUIRED 2.4.12)
    endif()
  endif()

  # Unit test
  if(GAIA_BUILD_UNITTEST)
    add_subdirectory(test)
  endif()

  # Benchmark
  if(GAIA_BUILD_BENCHMARK)
    add_subdirectory(perf)
  endif()
endif()

# Examples
if(GAIA_BUILD_EXAMPLES)
  if(EMSCRIPTEN)
    add_subdirectory(examples/example_wasm)
  else()
    add_subdirectory(examples/example_dll)
    add_subdirectory(examples/example_app)
    add_subdirectory(examples/example1)
    add_subdirectory(examples/example2)
    add_subdirectory(examples/example3)
    add_subdirectory(examples/example_roguelike)
  endif()
endif()

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

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_REQUIRED_QUIET TRUE)

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
  if(${CMAKE_CXX_COMPILER_ID} MATCHES Clang AND UNIX AND NOT APPLE)
    # Use mold as the default linker for clang if possible
    find_program(MOLD_LINKER mold)

    if(MOLD_LINKER)
      message(STATUS "Linker: ${MOLD_LINKER} MOLD")
      set(CMAKE_LINKER_TYPE "MOLD")
    endif()
  endif()
else()
  if(${CMAKE_CXX_COMPILER_ID} MATCHES Clang AND UNIX AND NOT APPLE)
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
      #   message(STATUS "Linker: ${LLD_PROGRAM_MATCH_VER} LLD")
      #   add_link_options("-fuse-ld=lld-${CLANG_VERSION_MAJOR}")
      # elseif(LLD_PROGRAM) # default lld
      #   message(STATUS "Linker: ${LLD_PROGRAM} LLD")
      #   add_link_options("-fuse-ld=lld")
      # endif(LLD_PROGRAM_MATCH_VER)
    endif()

  elseif(${CMAKE_CXX_COMPILER_ID} MATCHES GNU)
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

  # fast-math
  enable_cxx_compiler_flag_if_supported("/fp:fast")

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
else()
  # MacOS
  if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    # Workaround for the new linker starting with XCode 15
    # https://developer.apple.com/documentation/xcode-release-notes/xcode-15-release-notes#Linking
    # We need to use -Wl,-ld_classic for the linker for the backwards compatibility when buiding
    # outside of XCode because they messed things up.
    if(NOT CMAKE_GENERATOR MATCHES "Xcode")
      # Make sure xcodebuild is installed. Otherwise we won't be able to tell is the workaround
      # needs to be applied
      find_program(XCODEBUILD_EXECUTABLE NAMES xcodebuild)

      if(XCODEBUILD_EXECUTABLE)
        execute_process(COMMAND xcodebuild -version OUTPUT_VARIABLE XCODE_VERSION_STRING OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(REGEX MATCH "([0-9]+)\\.[0-9]+" XCODE_MAJOR_VERSION ${XCODE_VERSION_STRING})

        if(XCODE_MAJOR_VERSION GREATER_EQUAL "15")
          add_link_options("-Wl,-ld_classic")
        endif()
      endif()
    endif()
  endif()

  enable_cxx_compiler_flag_if_supported("-fno-exceptions")

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
    enable_cxx_compiler_flag_if_supported("-Wuplicated-cond")
    enable_cxx_compiler_flag_if_supported("-Wduplicated-branches")
    enable_cxx_compiler_flag_if_supported("-Wlogical-op")
    enable_cxx_compiler_flag_if_supported("-Wuseless-cast")
    enable_cxx_compiler_flag_if_supported("-Wunused-parameter")
    enable_cxx_compiler_flag_if_supported("-Wunused-local-typedef")
  else()
    enable_cxx_compiler_flag_if_supported("-Wno-deprecated-declarations")
  endif()

  # fast-math
  enable_cxx_compiler_flag_if_supported("-ffast-math")

  # instruction sets
  enable_cxx_compiler_flag_if_supported("-ftree-vectorize")

  if(NOT IsARM)
    enable_cxx_compiler_flag_if_supported("-msse4.1")
    enable_cxx_compiler_flag_if_supported("-mfma")
  endif()
endif()

if(MSVC)
  # Turn off RTTI
  string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

  # Turn off exceptions (including in STL)
  string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  add_definitions(-D_HAS_EXCEPTIONS=0)

  # TODO: Enable this once it lands
  # https://gitlab.kitware.com/cmake/cmake/-/merge_requests/4634
  # set(CMAKE_CXX_EXCEPTIONS OFF)
  # set(CMAKE_CXX_RTTI OFF)
endif()

set(IS_FETCH_AVAILABLE OFF)

if(GAIA_BUILD_BENCHMARK OR GAIA_BUILD_UNITTEST OR GAIA_PROFILER_CPU OR GAIA_PROFILER_MEM OR GAIA_MAKE_SINGLE_HEADER)
  find_package(Git REQUIRED)
  Include(FetchContent OPTIONAL RESULT_VARIABLE FetchContent_FOUND)

  if(NOT FetchContent_FOUND)
    message(WARNING "FetchContent might be necessary but it is not available")
  else()
    set(IS_FETCH_AVAILABLE ON)
  endif()
endif()

if(GAIA_MAKE_SINGLE_HEADER)
  if(IS_FETCH_AVAILABLE)
    find_package(quom CONFIG QUIET 4.0.1)

    if(NOT quom_FOUND)
      FetchContent_Declare(
        quom
        GIT_REPOSITORY https://github.com/Viatorus/quom.git
        GIT_TAG 4.0.1
      )

      FetchContent_MakeAvailable(quom)
    endif()
  else()
    find_package(quom CONFIG REQUIRED 4.0.1)
  endif()

  if(NOT UNIX)
    message(WARNING "Merging files into a single header is only supported on Unix systems")
  else()
    add_custom_target(generate_single_header ALL)
    add_custom_command(
      TARGET generate_single_header POST_BUILD
      COMMAND ./make_single_header.sh ${quom_SOURCE_DIR}
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      COMMENT "Merging files into a single header"
    )
  endif()
endif()

if(GAIA_GENERATE_CC)
  add_custom_target(generate_compile_commands ALL)
  add_custom_command(
    TARGET generate_compile_commands POST_BUILD
    COMMAND ${CMAKE_COMMAND} -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -G "Ninja" -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_SOURCE_DIR}/ninja -DGAIA_DEVMODE=${GAIA_DEVMODE} -DGAIA_PROFILER_CPU=${GAIA_PROFILER_CPU} -DGAIA_PROFILER_MEM=${GAIA_PROFILER_MEM}
    COMMENT "Generating compile_commands.json"
  )
endif()

if(GAIA_BUILD_BENCHMARK)
  if(IS_FETCH_AVAILABLE)
    find_package(picobench CONFIG QUIET 2.07)

    if(NOT picobench_FOUND)
      FetchContent_Declare(
        picobench
        GIT_REPOSITORY https://github.com/iboB/picobench.git
        GIT_TAG v2.07
      )

      FetchContent_MakeAvailable(picobench)
    endif()
  endif()
endif()

if(GAIA_BUILD_UNITTEST)
  if(IS_FETCH_AVAILABLE)
    find_package(Catch2 CONFIG QUIET 3.6.0)

    if(NOT Catch2_FOUND)
      FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.6.0
      )

      set(CATCH_CONFIG_DISABLE_STRINGIFICATION ON CACHE BOOL "CATCH_CONFIG_DISABLE_STRINGIFICATION override")
      set(CATCH_CONFIG_FAST_COMPILE ON CACHE BOOL "CATCH_CONFIG_FAST_COMPILE override")

      FetchContent_MakeAvailable(Catch2)
      list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/contrib)
    endif()
  else()
    find_package(Catch2 CONFIG REQUIRED 3.6.0)
  endif()
endif()

if(GAIA_PROFILER_CPU OR GAIA_PROFILER_MEM)
  if(IS_FETCH_AVAILABLE)
    find_package(tracy CONFIG QUIET 0.10)

    if(NOT tracy_FOUND)
      FetchContent_Declare(tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG v0.10
        GIT_SHALLOW TRUE
      )

      set(TRACY_ENABLE ON CACHE BOOL "TRACY_ENABLE override")
      set(TRACY_ON_DEMAND ON CACHE BOOL "TRACY_ON_DEMAND override")

      FetchContent_MakeAvailable(tracy)
    endif()
  else()
    find_package(tracy CONFIG REQUIRED 0.10)
  endif()

  if(GAIA_PROFILER_BUILD)
    # Add a custom target to build the Tracy profiler GUI
    if(UNIX)
      set(tracy_profiler_build_dir ${tracy_SOURCE_DIR}/profiler/build/unix)
    elseif(WIN32)
      set(tracy_profiler_build_dir ${tracy_SOURCE_DIR}/profiler/build/win32)
    else()
      message(WARNING "Unsupported platform. Building profiler won't be possible")
    endif()

    if(DEFINED tracy_profiler_build_dir)
      add_custom_target(build_tracy_profiler_gui ALL
        COMMAND make -j4 release
        WORKING_DIRECTORY ${tracy_profiler_build_dir}
      )
    endif()
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

# Examples
if(GAIA_BUILD_EXAMPLES)
  add_subdirectory(dummy)
  add_subdirectory(examples/example1)
  add_subdirectory(examples/example2)
  add_subdirectory(examples/example_roguelike)
endif()

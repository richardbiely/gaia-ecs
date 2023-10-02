if(!GAIA_BUILD_SRC)
  return()
endif()

# Compiler settings
include(CheckCXXCompilerFlag)

set(CMAKE_C_STANDARD 17)
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
  enable_cxx_compiler_flag_if_supported("-fno-exceptions")

  # strictness rules
  # Tracy profiler is not written with strictness and warnings-as-errors in mind
  # so rather than reading spam in the build output we disable any strictness
  # rules and deprecation warnings when profiling.
  if(!GAIA_PROFILER_CPU AND !GAIA_PROFILER_MEM)
    enable_cxx_compiler_flag_if_supported("-Wall")
    enable_cxx_compiler_flag_if_supported("-Wextra")
    enable_cxx_compiler_flag_if_supported("-pedantic")
    enable_cxx_compiler_flag_if_supported("-pedantic-errors")
    enable_cxx_compiler_flag_if_supported("-Werror")
    enable_cxx_compiler_flag_if_supported("-Wshadow")
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

if(GAIA_BUILD_UNITTEST)
  if(IS_FETCH_AVAILABLE)
    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG v3.4.0
    )

    set(CATCH_CONFIG_FAST_COMPILE ON)

    FetchContent_GetProperties(Catch2)

    if(NOT catch2_POPULATED)
      FetchContent_Populate(Catch2)
      add_subdirectory(${catch2_SOURCE_DIR} ${catch2_BINARY_DIR})
    endif()

    FetchContent_MakeAvailable(Catch2)
  else()
    find_package(Catch2 CONFIG REQUIRED v3.4.0)
  endif()
endif()

if(GAIA_PROFILER_CPU OR GAIA_PROFILER_MEM)
  if(IS_FETCH_AVAILABLE)
    FetchContent_Declare(tracy
      GIT_REPOSITORY https://github.com/wolfpld/tracy.git
      GIT_TAG v0.9.1
      GIT_SHALLOW TRUE
      GIT_PROGRESS TRUE
    )

    set(TRACY_ENABLE ON CACHE BOOL "TRACY_ENABLE override")
    set(TRACY_ON_DEMAND ON CACHE BOOL "TRACY_ON_DEMAND override")

    FetchContent_MakeAvailable(tracy)
  else()
    find_package(tracy CONFIG REQUIRED 0.9.1)
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

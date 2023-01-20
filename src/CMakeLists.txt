if(GAIA_BUILD_SRC)
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
      enable_cxx_compiler_flag_if_supported("/arch:AVX2")
    endif()
  else()
    enable_cxx_compiler_flag_if_supported("-fno-exceptions")

    # strictness rules
    enable_cxx_compiler_flag_if_supported("-Wall")
    enable_cxx_compiler_flag_if_supported("-Wextra")
    enable_cxx_compiler_flag_if_supported("-pedantic")
    enable_cxx_compiler_flag_if_supported("-Werror")

    # fast-math
    enable_cxx_compiler_flag_if_supported("-ffast-math")
    enable_cxx_compiler_flag_if_supported("-fno-math-errno")

    # instruction sets
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
      enable_cxx_compiler_flag_if_supported("-mfpu=neon")
    else(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
      enable_cxx_compiler_flag_if_supported("-msse4.1")
      enable_cxx_compiler_flag_if_supported("-mfma")
    endif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
  endif()

  if(MSVC)
    # TODO: Enable this once it lands
    # https://gitlab.kitware.com/cmake/cmake/-/merge_requests/4634
    # set(CMAKE_CXX_EXCEPTIONS OFF)
    # set(CMAKE_CXX_RTTI OFF)

    # Turn off RTTI
    string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    # Turn off exceptions (including in STL)
    string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_definitions(-D_HAS_EXCEPTIONS=0)
  endif()

  # Unit test
  if(GAIA_BUILD_UNITTEST)
    option(GAIA_FIND_CATCH2_PACKAGE "Enable finding Catch2 package." ON)
    add_subdirectory(test)
  endif()

  # Benchmark
  if(GAIA_BUILD_BENCHMARK)
    add_subdirectory(perf)
  endif()

  # Examples
  if(GAIA_BUILD_EXAMPLES)
    add_subdirectory(examples/example1)
    add_subdirectory(examples/example2)
    add_subdirectory(examples/example_rougelike)
  endif()
endif()

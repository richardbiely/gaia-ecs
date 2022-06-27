enable_testing()

if(GAIA_FIND_CATCH2_PACKAGE)
  Include(FetchContent)

  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v2.13.7
  )

  FetchContent_GetProperties(Catch2)

  if(NOT catch2_POPULATED)
    FetchContent_Populate(Catch2)
    add_subdirectory(${catch2_SOURCE_DIR} ${catch2_BINARY_DIR})
  endif()

  # Shortcut for the above commands but only available since 3.14
  # FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/contrib)
else()
  find_package(Catch2 CONFIG REQUIRED 2)
endif()

add_executable(gaia_test src/main.cpp)
set_property(TARGET gaia_test PROPERTY C_STANDARD 17)
set_property(TARGET gaia_test PROPERTY CXX_STANDARD 17)
set_property(TARGET gaia_test PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET gaia_test PROPERTY CXX_EXTENSIONS OFF)

set(CMAKE_REQUIRED_QUIET TRUE)

# Compiler settings
include(CheckCXXCompilerFlag)

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
else()
  # strictness rules
  enable_cxx_compiler_flag_if_supported("-Wall")
  enable_cxx_compiler_flag_if_supported("-Wextra")
  enable_cxx_compiler_flag_if_supported("-pedantic")
  enable_cxx_compiler_flag_if_supported("-Werror")
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

# Unit test framework
target_link_libraries(gaia_test PRIVATE Catch2::Catch2)
target_include_directories(gaia_test PRIVATE ${PROJECT_SOURCE_DIR}/include)

include(CTest)
include(Catch)
catch_discover_tests(gaia_test)

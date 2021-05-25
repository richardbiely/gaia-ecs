enable_testing()

find_package(Catch2 CONFIG REQUIRED 2)
add_executable(gaia_test test/test.cpp)

set_property(TARGET gaia_test PROPERTY C_STANDARD 17)
set_property(TARGET gaia_test PROPERTY CXX_STANDARD 17)
set_property(TARGET gaia_test PROPERTY CXX_STANDARD_REQUIRED ON) 
set_property(TARGET gaia_test PROPERTY CXX_EXTENSIONS OFF)

# Enforce strict rules
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

if (MSVC)
  enable_cxx_compiler_flag_if_supported("/W4")
  enable_cxx_compiler_flag_if_supported("/WX")
else()
  enable_cxx_compiler_flag_if_supported("-Wall")
  enable_cxx_compiler_flag_if_supported("-Wextra")
  enable_cxx_compiler_flag_if_supported("-pedantic")
  enable_cxx_compiler_flag_if_supported("-Werror")
endif()

# Unit test framework
target_link_libraries(gaia_test PRIVATE Catch2::Catch2)
target_include_directories(gaia_test PRIVATE ${PROJECT_SOURCE_DIR}/include)

include(CTest)
include(Catch)
catch_discover_tests(gaia_test)
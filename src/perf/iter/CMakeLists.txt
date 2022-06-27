add_executable(gaia_perf_iter src/main.cpp)

set_property(TARGET gaia_perf_iter PROPERTY C_STANDARD 17)
set_property(TARGET gaia_perf_iter PROPERTY CXX_STANDARD 17)
set_property(TARGET gaia_perf_iter PROPERTY CXX_STANDARD_REQUIRED ON) 
set_property(TARGET gaia_perf_iter PROPERTY CXX_EXTENSIONS OFF)

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

if (MSVC)
  # parallel build
  enable_cxx_compiler_flag_if_supported("/MP")
  # strictness rules
  enable_cxx_compiler_flag_if_supported("/W4")
  enable_cxx_compiler_flag_if_supported("/WX")
  # fast-math
  enable_cxx_compiler_flag_if_supported("/fp:fast")
  # necessary because VS might complain about too many sections used in debug builds
  enable_cxx_compiler_flag_if_supported("/bigobj")
else()
  # strictness rules
  enable_cxx_compiler_flag_if_supported("-Wall")
  enable_cxx_compiler_flag_if_supported("-Wextra")
  enable_cxx_compiler_flag_if_supported("-pedantic")
  enable_cxx_compiler_flag_if_supported("-Werror")
  # fast-math
  enable_cxx_compiler_flag_if_supported("-ffast-math")
  enable_cxx_compiler_flag_if_supported("-fno-math-errno")
endif()

if (MSVC)
	# TODO: Enable this once it lands
	# https://gitlab.kitware.com/cmake/cmake/-/merge_requests/4634
	#set(CMAKE_CXX_EXCEPTIONS OFF)
	#set(CMAKE_CXX_RTTI OFF)

	# Turn off RTTI
	string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  # Turn off exceptions (including in STL)
	string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	add_definitions(-D_HAS_EXCEPTIONS=0)
endif()

# Benchmark framework
target_include_directories(gaia_perf_iter PRIVATE ${PROJECT_SOURCE_DIR}/include)

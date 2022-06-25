find_package(benchmark CONFIG REQUIRED)
add_executable(gaia_perf_duel src/main.cpp)

set_property(TARGET gaia_perf_duel PROPERTY C_STANDARD 17)
set_property(TARGET gaia_perf_duel PROPERTY CXX_STANDARD 17)
set_property(TARGET gaia_perf_duel PROPERTY CXX_STANDARD_REQUIRED ON) 
set_property(TARGET gaia_perf_duel PROPERTY CXX_EXTENSIONS OFF)

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
  # dynamic runtime (because of google bemchmark)
  # TODO: handle google benchmark
  enable_cxx_compiler_flag_if_supported("/MD")
  # strictness rules
  enable_cxx_compiler_flag_if_supported("/W4")
  enable_cxx_compiler_flag_if_supported("/WX")
  # fast-math
  enable_cxx_compiler_flag_if_supported("/fp:fast") 
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    enable_cxx_compiler_flag_if_supported("-msse4.1")
    enable_cxx_compiler_flag_if_supported("-mfma")
  endif()
else()
  #enable_cxx_compiler_flag_if_supported("-Rpass-analysis=loop-vectorize")
  #enable_cxx_compiler_flag_if_supported("-fsave-optimization-record")
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
target_link_libraries(gaia_perf_duel PRIVATE benchmark::benchmark)
if (MSVC)
  target_link_libraries(gaia_perf_duel PRIVATE Shlwapi)
else()
  target_link_libraries(gaia_perf_duel PRIVATE pthread)
endif()
target_include_directories(gaia_perf_duel PRIVATE ${PROJECT_SOURCE_DIR}/include)

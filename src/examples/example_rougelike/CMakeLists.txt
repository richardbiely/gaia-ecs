add_executable(gaia_example_rougelike src/main.cpp)

set_property(TARGET gaia_example_rougelike PROPERTY C_STANDARD 17)
set_property(TARGET gaia_example_rougelike PROPERTY CXX_STANDARD 17)
set_property(TARGET gaia_example_rougelike PROPERTY CXX_STANDARD_REQUIRED ON) 
set_property(TARGET gaia_example_rougelike PROPERTY CXX_EXTENSIONS OFF)

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
  # strictness rules
  enable_cxx_compiler_flag_if_supported("/W4")
  enable_cxx_compiler_flag_if_supported("/WX")
  # fast-math
  enable_cxx_compiler_flag_if_supported("/fp:fast") 
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

target_include_directories(gaia_example_rougelike PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Example of how to link an external project with gaia-ecs library
# previously installed somewhere

cmake_minimum_required(VERSION 3.12)
project(cli LANGUAGES CXX)

find_package(gaia CONFIG REQUIRED)
message("gaia_DIR: ${gaia_DIR}")
message("gaia_INCLUDE_DIR: ${gaia_INCLUDE_DIR}")
include_directories(${gaia_INCLUDE_DIR})

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} gaia::gaia)

add_custom_target(run
    COMMAND cli
    DEPENDS cli
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
)

set_property(TARGET run PROPERTY C_STANDARD 17)
set_property(TARGET run PROPERTY CXX_STANDARD 17)
set_property(TARGET run PROPERTY CXX_STANDARD_REQUIRED ON) 
set_property(TARGET run PROPERTY CXX_EXTENSIONS OFF)

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

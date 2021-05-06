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

set_property(TARGET run PROPERTY C_STANDARD 99)
set_property(TARGET run PROPERTY CXX_STANDARD 20)
set_property(TARGET run PROPERTY CXX_STANDARD_REQUIRED ON) 
set_property(TARGET run PROPERTY CXX_EXTENSIONS OFF)
# Example of how to link an external project with gaia-ecs library
# previously installed somewhere

cmake_minimum_required(VERSION 3.12)

set(PROJ_NAME "cli")
project(${PROJ_NAME} LANGUAGES CXX)

find_package(gaia CONFIG REQUIRED)
message("gaia_DIR: ${gaia_DIR}")
message("gaia_INCLUDE_DIR: ${gaia_INCLUDE_DIR}")
include_directories(${gaia_INCLUDE_DIR})

add_executable(${PROJ_NAME} src/main.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJ_NAME} gaia::gaia)
target_link_libraries(${PROJ_NAME} PRIVATE Threads::Threads)

set(PROJ_TARGET "run")
add_custom_target(${PROJ_TARGET}
    COMMAND ${PROJ_NAME}
    DEPENDS ${PROJ_NAME}
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
)

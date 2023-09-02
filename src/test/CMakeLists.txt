enable_testing()

set(PROJ_NAME "gaia_test")
add_executable(${PROJ_NAME} src/main.cpp)

# System-specific threading library
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJ_NAME} PRIVATE Threads::Threads)

# Unit test framework
target_link_libraries(${PROJ_NAME} PRIVATE Catch2::Catch2WithMain)

# Project files
target_include_directories(${PROJ_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

include(CTest)
include(Catch)
catch_discover_tests(${PROJ_NAME})

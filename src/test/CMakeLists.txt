enable_testing()

set(PROJ_NAME "gaia_test")
add_executable(${PROJ_NAME} src/main.cpp)

# Unit test framework
target_link_libraries(${PROJ_NAME} PRIVATE Catch2::Catch2WithMain)

# Project files
target_include_directories(${PROJ_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

include(CTest)
include(Catch)
catch_discover_tests(${PROJ_NAME})

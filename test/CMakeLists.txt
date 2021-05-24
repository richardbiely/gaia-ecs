enable_testing()

find_package(Catch2 CONFIG REQUIRED 2)
add_executable(gaia_test test/test.cpp)

set_property(TARGET gaia_test PROPERTY C_STANDARD 17)
set_property(TARGET gaia_test PROPERTY CXX_STANDARD 17)
set_property(TARGET gaia_test PROPERTY CXX_STANDARD_REQUIRED ON) 
set_property(TARGET gaia_test PROPERTY CXX_EXTENSIONS OFF)

target_link_libraries(gaia_test PRIVATE Catch2::Catch2)
target_include_directories(gaia_test PRIVATE ${PROJECT_SOURCE_DIR}/include)

include(CTest)
include(Catch)
catch_discover_tests(gaia_test)
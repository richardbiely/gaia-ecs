find_package(benchmark CONFIG REQUIRED)
add_executable(gaia_perf perf/perf.cpp)

set_property(TARGET gaia_perf PROPERTY C_STANDARD 17)
set_property(TARGET gaia_perf PROPERTY CXX_STANDARD 17)
set_property(TARGET gaia_perf PROPERTY CXX_STANDARD_REQUIRED ON) 
set_property(TARGET gaia_perf PROPERTY CXX_EXTENSIONS OFF)

target_link_libraries(gaia_perf PRIVATE benchmark::benchmark)
target_include_directories(gaia_perf PRIVATE ${PROJECT_SOURCE_DIR}/include)

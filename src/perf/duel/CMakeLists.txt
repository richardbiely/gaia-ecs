set(PROJ_NAME "gaia_perf_duel")
add_executable(${PROJ_NAME} src/main.cpp)

# Benchmark framework
target_include_directories(${PROJ_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

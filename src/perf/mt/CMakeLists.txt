set(PROJ_NAME "gaia_perf_mt")
add_executable(${PROJ_NAME} src/main.cpp)

target_include_directories(${PROJ_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

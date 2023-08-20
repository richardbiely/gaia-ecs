set(PROJ_NAME "gaia_perf_duel")
add_executable(${PROJ_NAME} src/main.cpp)

# System-specific threading library
find_package(Threads REQUIRED)
target_link_libraries(${PROJ_NAME} PRIVATE Threads::Threads)

target_include_directories(${PROJ_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

set(PROJ_NAME "gaia_perf_iter")
add_executable(${PROJ_NAME} src/main.cpp)

if(MSVC)
  # Necessary because VS might complain about too many sections used in debug builds
  target_compile_options(${PROJ_NAME} PRIVATE /bigobj)
endif()

target_include_directories(${PROJ_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(${PROJ_NAME} PRIVATE ${picobench_SOURCE_DIR}/include)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJ_NAME} PRIVATE Threads::Threads)
target_link_libraries(${PROJ_NAME} PRIVATE picobench)
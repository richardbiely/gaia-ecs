set(PROJ_NAME "gaia_perf_iter")
add_executable(${PROJ_NAME} src/main.cpp)

if(MSVC)
  # necessary because VS might complain about too many sections used in debug builds
  target_compile_options(${PROJ_NAME} PRIVATE /bigobj)
endif()

# Benchmark framework
target_include_directories(${PROJ_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

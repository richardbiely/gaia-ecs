set(PROJ_NAME "gaia_perf_duel")
add_executable(${PROJ_NAME} src/main.cpp)

# Preprocessor
target_compile_definitions(${PROJ_NAME} PRIVATE GAIA_DEBUG=0)

# Benchmark framework
target_include_directories(${PROJ_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Profiler
if(GAIA_PROFILER_CPU OR GAIA_PROFILER_MEM)
  target_link_libraries(${PROJ_NAME} PRIVATE TracyClient)
endif()

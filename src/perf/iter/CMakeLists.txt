set(PROJ_NAME "gaia_perf_iter")
add_executable(${PROJ_NAME} src/main.cpp)

# Preprocessor
target_compile_definitions(${PROJ_NAME} PRIVATE GAIA_DEBUG=0)

if(MSVC)
  # Necessary because VS might complain about too many sections used in debug builds
  target_compile_options(${PROJ_NAME} PRIVATE /bigobj)
endif()

# Benchmark framework
target_include_directories(${PROJ_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Profiler
if(GAIA_PROFILER_CPU OR GAIA_PROFILER_MEM)
  if(GAIA_PROFILER_CPU)
    target_compile_definitions(${PROJ_NAME} PRIVATE GAIA_PROFILER_CPU=1)
  endif()

  if(GAIA_PROFILER_MEM)
    target_compile_definitions(${PROJ_NAME} PRIVATE GAIA_PROFILER_MEM=1)
  endif()

  target_link_libraries(${PROJ_NAME} PRIVATE TracyClient)
endif()

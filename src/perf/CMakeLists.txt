add_subdirectory(iter)
add_subdirectory(duel)
add_subdirectory(entity)

# Preprocessor
add_definitions(-DGAIA_DEBUG=0)

# Profiler
if(GAIA_PROFILER_CPU OR GAIA_PROFILER_MEM)
  if(GAIA_PROFILER_CPU)
    compile_definitions(PRIVATE GAIA_PROFILER_CPU=1)
  endif()

  if(GAIA_PROFILER_MEM)
    compile_definitions(PRIVATE GAIA_PROFILER_MEM=1)
  endif()

  link_libraries(PRIVATE TracyClient)
endif()

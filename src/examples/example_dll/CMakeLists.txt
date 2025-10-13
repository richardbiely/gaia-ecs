set(PROJ_NAME "gaia_example_dll")

add_library(${PROJ_NAME} SHARED
  src/dll_api.cpp
)

if(GAIA_PROFILER_CPU OR GAIA_PROFILER_MEM)
  set_property(TARGET TracyClient PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

target_include_directories(${PROJ_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_SOURCE_DIR}/include
)

target_compile_definitions(${PROJ_NAME} PRIVATE GAIA_GEN_DLL)
set(PROJ_NAME "gaia_example_dll")

add_library(${PROJ_NAME} SHARED
  src/dll_api.cpp
)

target_include_directories(${PROJ_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
  PRIVATE ${PROJECT_SOURCE_DIR}/include
)

target_compile_definitions(${PROJ_NAME} PRIVATE BUILDING_DLL)
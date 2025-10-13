set(PROJ_NAME "gaia_example_app")
add_executable(${PROJ_NAME} src/main.cpp)

target_include_directories(${PROJ_NAME}
  PRIVATE ${PROJECT_SOURCE_DIR}/include
  PUBLIC ${PROJECT_CURRENT_SOURCE_DIR}/../../example_dll/include)

target_link_libraries(${PROJ_NAME} PRIVATE gaia_example_dll)

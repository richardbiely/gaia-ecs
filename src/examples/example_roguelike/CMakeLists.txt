set(PROJ_NAME "gaia_example_roguelike")

file(GLOB_RECURSE ROGUELIKE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
foreach(SOURCE_FILE IN LISTS ROGUELIKE_SOURCES)
  if(NOT SOURCE_FILE MATCHES "/gaia_ecs\\.h$")
    file(READ "${SOURCE_FILE}" SOURCE_TEXT)
    string(FIND "${SOURCE_TEXT}" "#include <gaia.h>" DIRECT_GAIA_INCLUDE)
    if(NOT DIRECT_GAIA_INCLUDE EQUAL -1)
      message(FATAL_ERROR "${SOURCE_FILE} must include gaia_ecs.h instead of <gaia.h>")
    endif()
  endif()
endforeach()

add_executable(
  ${PROJ_NAME}
  src/main.cpp
  src/roguelike_dungeon.cpp
  src/roguelike_game.cpp
  src/roguelike_path.cpp
  src/roguelike_render.cpp
  src/roguelike_systems.cpp
)

target_include_directories(${PROJ_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJ_NAME} PRIVATE Threads::Threads)

if(MSVC)
  enable_cxx_compiler_flag_if_supported("/bigobj")
endif()

if(BUILD_TESTING)
  add_test(NAME ${PROJ_NAME}_smoke COMMAND $<TARGET_FILE:${PROJ_NAME}> --smoke)
  set_tests_properties(${PROJ_NAME}_smoke PROPERTIES TIMEOUT 30)
endif()

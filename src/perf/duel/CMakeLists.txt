set(PROJ_NAME "gaia_duel")
add_executable(${PROJ_NAME} src/main.cpp)

if(MSVC)
  enable_cxx_compiler_flag_if_supported("/bigobj")
endif()

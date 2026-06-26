set(PROJ_NAME "gaia_duel")
add_executable(${PROJ_NAME} src/main.cpp)

if(MSVC)
	target_compile_options(${PROJ_NAME} PRIVATE /bigobj)
endif()

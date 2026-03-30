set(GAIA_TEST_SOURCES
	src/main.cpp
	src/test_core.cpp
	src/test_ecs_entity.cpp
	src/test_query_core.cpp
	src/test_query_prefab.cpp
	src/test_mutation_runtime.cpp
	src/test_system.cpp
	src/test_observer.cpp
	src/test_misc.cpp
)

function(gaia_configure_test_target target_name)
	add_executable(${target_name} ${GAIA_TEST_SOURCES})

	# System-specific threading library
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package(Threads REQUIRED)
	target_link_libraries(${target_name} PRIVATE Threads::Threads)

	# Unit test framework
	target_link_libraries(${target_name} PRIVATE doctest::doctest)

	# Project files
	target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/include)
endfunction()

set(PROJ_NAME "gaia_test")
gaia_configure_test_target(${PROJ_NAME})

set(PROJ_NAME_NO_AUTOREG "gaia_test_no_autoreg")
gaia_configure_test_target(${PROJ_NAME_NO_AUTOREG})
target_compile_definitions(${PROJ_NAME_NO_AUTOREG} PRIVATE GAIA_ECS_AUTO_COMPONENT_REGISTRATION=0)

if(MSVC)
	enable_cxx_compiler_flag_if_supported("/bigobj")
endif()

include(CTest)
enable_testing()
add_test(NAME ${PROJ_NAME} COMMAND ${PROJ_NAME})
add_test(
	NAME ${PROJ_NAME_NO_AUTOREG}
	COMMAND ${PROJ_NAME_NO_AUTOREG} --test-case=*component registration*)

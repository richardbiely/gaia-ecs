set(GAIA_TEST_SOURCES
	src/main.cpp
	src/test_core.cpp
	src/test_ecs_entity.cpp
	src/test_query_core.cpp
	src/test_query_prefab_lifecycle.cpp
	src/test_query_prefab_usage.cpp
	src/test_query_prefab_inherit.cpp
	src/test_query_prefab_cache.cpp
	src/test_mutation_runtime.cpp
	src/test_system.cpp
	src/test_observer_basic.cpp
	src/test_observer_trav.cpp
	src/test_observer_prefab.cpp
	src/test_misc.cpp
	src/test_mt.cpp
	src/test_serialization.cpp
	src/test_parent.cpp
	src/test_storage.cpp
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
	target_compile_definitions(${target_name} PRIVATE GAIA_ECS_TEST_HOOKS=1)

	# Compiler-specific options
	if(MSVC)
		target_compile_options(${target_name} PRIVATE /bigobj)
	endif()
endfunction()

set(PROJ_NAME "gaia_test")
gaia_configure_test_target(${PROJ_NAME})

set(PROJ_NAME_NO_AUTOREG "gaia_test_no_autoreg")
gaia_configure_test_target(${PROJ_NAME_NO_AUTOREG})
target_compile_definitions(${PROJ_NAME_NO_AUTOREG} PRIVATE GAIA_ECS_AUTO_COMPONENT_REGISTRATION=0)

add_test(NAME ${PROJ_NAME} COMMAND $<TARGET_FILE:${PROJ_NAME}>)
add_test(
	NAME ${PROJ_NAME_NO_AUTOREG}
	COMMAND $<TARGET_FILE:${PROJ_NAME_NO_AUTOREG}> "--test-case=*component registration*")

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

set(PROJ_NAME_NO_OBSERVERS "gaia_test_no_observers")
add_executable(${PROJ_NAME_NO_OBSERVERS} src/test_no_observers.cpp)
target_link_libraries(${PROJ_NAME_NO_OBSERVERS} PRIVATE Threads::Threads)
target_include_directories(${PROJ_NAME_NO_OBSERVERS} PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(${PROJ_NAME_NO_OBSERVERS} PRIVATE GAIA_OBSERVERS_ENABLED=0 GAIA_USE_SAFE_ENTITY=1)

set(PROJ_NAME_NO_OBSERVERS_SINGLE_HEADER "gaia_test_no_observers_single_header")
add_executable(${PROJ_NAME_NO_OBSERVERS_SINGLE_HEADER} src/test_no_observers.cpp)
target_link_libraries(${PROJ_NAME_NO_OBSERVERS_SINGLE_HEADER} PRIVATE Threads::Threads)
target_include_directories(${PROJ_NAME_NO_OBSERVERS_SINGLE_HEADER} PRIVATE ${PROJECT_SOURCE_DIR}/single_include)
target_compile_definitions(
	${PROJ_NAME_NO_OBSERVERS_SINGLE_HEADER} PRIVATE GAIA_OBSERVERS_ENABLED=0 GAIA_USE_SAFE_ENTITY=1)

add_test(NAME ${PROJ_NAME} COMMAND $<TARGET_FILE:${PROJ_NAME}>)
add_test(
	NAME ${PROJ_NAME_NO_AUTOREG}
	COMMAND $<TARGET_FILE:${PROJ_NAME_NO_AUTOREG}> "--test-case=*component registration*")
add_test(NAME ${PROJ_NAME_NO_OBSERVERS} COMMAND $<TARGET_FILE:${PROJ_NAME_NO_OBSERVERS}>)
add_test(
	NAME ${PROJ_NAME_NO_OBSERVERS_SINGLE_HEADER} COMMAND $<TARGET_FILE:${PROJ_NAME_NO_OBSERVERS_SINGLE_HEADER}>)

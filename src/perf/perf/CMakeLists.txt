set(PROJ_NAME "gaia_perf")
add_executable(${PROJ_NAME}
	src/main.cpp
	src/entity_lifecycle.cpp
	src/structural_changes.cpp
	src/component_access.cpp
	src/query_hot_path.cpp
	src/fragmented.cpp
	src/observers.cpp
	src/observers_bench.cpp
	src/systems.cpp
	src/mixed.cpp
	src/parent.cpp
	src/sparse.cpp
	src/legacy_entity.cpp
	src/legacy_iter.cpp
	src/containers.cpp
	src/allocators.cpp
	src/funcs.cpp
)

target_include_directories(${PROJ_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
	target_compile_options(${PROJ_NAME} PRIVATE /bigobj)
endif()

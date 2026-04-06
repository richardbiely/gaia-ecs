set(PROJ_NAME "gaia_perf")
add_executable(${PROJ_NAME}
	src/main.cpp
	src/entity_lifecycle.cpp
	src/structural_changes.cpp
	src/query_hot_path.cpp
	src/fragmented.cpp
	src/observers.cpp
	src/systems.cpp
	src/mixed.cpp
	src/parent.cpp
	src/sparse.cpp
	src/legacy_entity.cpp
	src/legacy_iter.cpp
	src/containers.cpp
	src/allocators.cpp
)

target_include_directories(${PROJ_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
  enable_cxx_compiler_flag_if_supported("/bigobj")
endif()

set(PROJ_NAME "gaia_perf")
add_executable(${PROJ_NAME}
	src/main.cpp
	src/perf_entity_lifecycle.cpp
	src/perf_structural_changes.cpp
	src/perf_query_hot_path.cpp
	src/perf_fragmented.cpp
	src/perf_observers.cpp
	src/perf_systems.cpp
	src/perf_mixed.cpp
	src/perf_parent.cpp
	src/perf_sparse.cpp
	src/perf_legacy_entity.cpp
	src/perf_legacy_iter.cpp
)

target_include_directories(${PROJ_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
  enable_cxx_compiler_flag_if_supported("/bigobj")
endif()

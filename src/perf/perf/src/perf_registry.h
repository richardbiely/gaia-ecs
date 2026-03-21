#pragma once

enum class PerfRunMode {
	Normal,
	Profiling,
	Sanitizer,
};

void register_perf_matrix_entity_lifecycle(PerfRunMode mode);
void register_perf_matrix_structural_changes(PerfRunMode mode);
void register_perf_matrix_query_hot_path(PerfRunMode mode);
void register_perf_matrix_fragmented(PerfRunMode mode);
void register_perf_matrix_observers(PerfRunMode mode);
void register_perf_matrix_systems(PerfRunMode mode);
void register_perf_matrix_mixed(PerfRunMode mode);
void register_perf_matrix_parent(PerfRunMode mode);
void register_perf_matrix_sparse(PerfRunMode mode);
void register_perf_entity_legacy(PerfRunMode mode);
void register_perf_iter_legacy(PerfRunMode mode);

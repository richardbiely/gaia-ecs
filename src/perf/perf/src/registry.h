#pragma once

enum class PerfRunMode {
	Normal,
	Profiling,
	Sanitizer,
};

void register_entity_lifecycle(PerfRunMode mode);
void register_structural_changes(PerfRunMode mode);
void register_query_hot_path(PerfRunMode mode);
void register_fragmented(PerfRunMode mode);
void register_observers(PerfRunMode mode);
void register_systems(PerfRunMode mode);
void register_mixed(PerfRunMode mode);
void register_parent(PerfRunMode mode);
void register_sparse(PerfRunMode mode);
void register_legacy_entity(PerfRunMode mode);
void register_legacy_iter(PerfRunMode mode);
void register_containers(PerfRunMode mode);

#define PICOBENCH_IMPLEMENT
#include <gaia.h>
#include <picobench/picobench.hpp>
#include <string_view>

#include "registry.h"

int main(int argc, char* argv[]) {
	bool profilingMode = false;
	bool sanitizerMode = false;

	gaia::cnt::darray<const char*> picobenchArgs;
	picobenchArgs.reserve((uint32_t)argc);
	picobenchArgs.push_back(argv[0]);

	for (int i = 1; i < argc; ++i) {
		const std::string_view arg(argv[i]);
		if (arg == "-p") {
			profilingMode = true;
			continue;
		}

		if (arg == "-s") {
			sanitizerMode = true;
			continue;
		}

		picobenchArgs.push_back(argv[i]);
	}

	GAIA_LOG_N("Profiling mode = %s", profilingMode ? "ON" : "OFF");
	GAIA_LOG_N("Sanitizer mode = %s", sanitizerMode ? "ON" : "OFF");

	const auto mode =
			profilingMode ? PerfRunMode::Profiling : (sanitizerMode ? PerfRunMode::Sanitizer : PerfRunMode::Normal);

	register_entity_lifecycle(mode);
	register_structural_changes(mode);
	register_query_hot_path(mode);
	register_fragmented(mode);
	register_observers(mode);
	register_systems(mode);
	register_mixed(mode);
	register_parent(mode);
	register_sparse(mode);
	register_legacy_entity(mode);
	register_legacy_iter(mode);
	register_containers(mode);

	picobench::runner r;
	r.parse_cmd_line((int)picobenchArgs.size(), picobenchArgs.data());

	if (r.error() == picobench::error_unknown_cmd_line_argument)
		r.set_error(picobench::no_error);

	return r.run(0);
}

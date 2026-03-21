#define DOCTEST_CONFIG_IMPLEMENT
#include "test_common.h"

int main(int argc, char** argv) {
	// Use custom logging. Just for code coverage.
	util::set_log_func(util::detail::log_cached);
	util::set_log_flush_func(util::detail::log_flush_cached);

	doctest::Context ctx(argc, argv);
	ctx.setOption("success", false); // suppress successful checks
	return ctx.run();
}

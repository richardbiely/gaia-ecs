#include <gaia.h>

GAIA_INIT

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

TEST_CASE("CreateEntity") {
  gaia::ecs::World w;
  auto e = w.CreateEntity();
  REQUIRE(e.IsValid());
}
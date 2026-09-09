#include <cstdio>
#include <cstdlib>
#include <cstring>

#define GAIA_ASSERT_ENABLED 1
#if GAIA_ASSERT_ENABLED
static bool assertionExpected = false;

//! Distinguishes the intended misuse assertion from setup failures and unrelated crashes.
[[noreturn]] static void query_access_assert(const char* expression) {
	std::fprintf(stderr, "Query presence-only assertion: %s\n", expression);
	std::exit(assertionExpected ? 86 : 87);
}
	#define GAIA_ASSERT(condition)                                                                                       \
		{                                                                                                                  \
			if (!(condition))                                                                                                \
				query_access_assert(#condition);                                                                               \
		}
	#define GAIA_ASSERT2(condition, message) GAIA_ASSERT(condition)
#endif
#define GAIA_SYSTEMS_ENABLED 1
#include <gaia.h>

using namespace gaia;

struct Payload {
	int value;
};

struct Marker {};

struct SparsePayload {
	GAIA_STORAGE(Sparse);
	int value;
};

struct SoAPayload {
	GAIA_LAYOUT(SoA);
	int x, y;
};

int main(int argc, char** argv) {
	if (argc != 2)
		return 2;
	const auto is = [&](const char* mode) {
		return std::strcmp(argv[1], mode) == 0;
	};

	ecs::World world;
	const auto payload = world.add<Payload>().entity;
	const auto entity = world.add();
	world.add<Payload>(entity, {1});
	world.add<SparsePayload>(entity, {2});
	world.add<SoAPayload>(entity, {3, 4});
	const auto target = world.add();
	const auto pair = ecs::Pair(payload, target);
	world.add(entity, pair);

	if (is("auto-tag-indexed") || is("auto-tag-raw") || is("auto-entity-raw") || is("auto-pair-indexed") ||
			is("auto-pair-raw") || is("valid")) {
		const auto marker = world.add<Marker>().entity;
		const auto emptyPair = ecs::Pair(marker, target);
		world.add<Marker>(entity);
		world.add(entity, target);
		world.add(entity, emptyPair);
		auto presenceQuery = world.query();
		if (is("valid")) {
			presenceQuery.all<Marker>().all(target).all(emptyPair);
			if (presenceQuery.count() != 1 || presenceQuery.fetch().ctx().data.readWriteMask != 0)
				return 3;
			for (const auto& term: presenceQuery.fetch().ctx().data.terms_view()) {
				if (term.access != ecs::QueryAccess::Match || presenceQuery.access(term.id) != ecs::QueryAccess::None)
					return 3;
			}
			unsigned count = 0;
			presenceQuery.each([&](ecs::Iter& it) {
				if (it.has<Marker>() && it.has(target) && it.has(emptyPair))
					count += it.size();
			});
			if (count != 1)
				return 3;
		} else {
			const auto id = is("auto-entity-raw")													 ? target
											: is("auto-tag-indexed") || is("auto-tag-raw") ? marker
																																		 : emptyPair;
			if (is("auto-tag-indexed"))
				presenceQuery.all<Marker>();
			else
				presenceQuery.add({ecs::QueryOpKind::All, ecs::QueryAccess::None, id});
			if (presenceQuery.count() != 1 || presenceQuery.fetch().ctx().data.terms[0].access != ecs::QueryAccess::Match)
				return 3;
			assertionExpected = true;
			presenceQuery.each([&](ecs::Iter& it) {
				if (is("auto-tag-indexed") || is("auto-pair-indexed"))
					(void)it.view_raw(0);
				else
					(void)it.view_raw_any(id);
			});
			assertionExpected = false;
			return 0;
		}
	}

	if (is("modifier-missing") || is("modifier-metadata") || is("modifier-not") || is("modifier-after-fetch")) {
		auto query = world.query();
		if (is("modifier-after-fetch")) {
			query.all<Payload>();
			(void)query.fetch();
		} else if (is("modifier-metadata"))
			query.match_prefab();
		else if (is("modifier-not"))
			query.no<Payload>();
		assertionExpected = true;
		(void)query.no_access().count();
		assertionExpected = false;
		return 0;
	}

	if (is("not-term")) {
		assertionExpected = true;
		(void)world.query().no<Payload>(ecs::QueryTermOptions{}.no_access()).count();
		assertionExpected = false;
		return 0;
	}

	auto query = is("valid") ? world.query().all<Payload&>() : world.query().all<Payload>().no_access();
	if (query.count() != 1)
		return 3;

	if (is("valid")) {
		query.each([](Payload& value) {
			++value.value;
		});
		query.each([](ecs::Iter& it) {
			(void)it.view<Payload>();
			(void)it.view_mut<Payload>(0);
			(void)it.view_raw(0);
		});
		return world.get<Payload>(entity).value == 2 ? 0 : 4;
	}

	if (is("empty-typed") || is("empty-array") || is("empty-parallel")) {
		world.del<Payload>(entity);
		if (query.count() != 0)
			return 3;
	}
	if (is("explicit-read"))
		query.reads<Payload>();
	if (is("explicit-write"))
		query.writes<Payload>();

	if (is("optional-typed") || is("optional-view") || is("optional-indexed") || is("optional-raw")) {
		auto optionalQuery = world.query().all<SparsePayload>().any<Payload>().no_access();
		if (optionalQuery.count() != 1)
			return 3;
		assertionExpected = true;
		if (is("optional-typed"))
			optionalQuery.each([](const Payload&) {});
		else {
			optionalQuery.each([&](ecs::Iter& it) {
				if (is("optional-view"))
					(void)it.view<Payload>();
				else if (is("optional-indexed"))
					(void)it.view<Payload>(1);
				else
					(void)it.view_raw(1);
			});
		}
	} else if (is("sparse") || is("sparse-indexed")) {
		auto sparseQuery = world.query().all<SparsePayload>().no_access();
		if (sparseQuery.count() != 1)
			return 3;
		assertionExpected = true;
		sparseQuery.each([&](ecs::Iter& it) {
			if (is("sparse"))
				(void)it.view_any<SparsePayload>();
			else
				(void)it.view_any<SparsePayload>(0);
		});
	} else if (is("soa-field")) {
		auto soaQuery = world.query().all<SoAPayload>().no_access();
		if (soaQuery.count() != 1)
			return 3;
		assertionExpected = true;
		soaQuery.each([](ecs::Iter& it) {
			(void)it.view_raw_field(0, 0);
		});
	} else if (is("pair") || is("wildcard-pair")) {
		auto pairQuery = world.query().all(is("pair") ? pair : ecs::Pair(payload, ecs::All)).no_access();
		if (pairQuery.count() != 1)
			return 3;
		assertionExpected = true;
		pairQuery.each([&](ecs::Iter& it) {
			(void)it.view_raw_any(pair);
		});
	} else if (is("inherited-index")) {
		world.del<Payload>(entity);
		const auto prefab = world.prefab();
		world.add<Payload>(prefab, {5});
		world.add(payload, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
		const auto instance = world.instantiate(prefab);
		auto inheritedQuery = world.query().all<Payload>().no_access();
		if (inheritedQuery.count() != 1 || world.has_direct(instance, payload))
			return 3;
		assertionExpected = true;
		inheritedQuery.each([](ecs::Iter& it) {
			(void)it.view_any<Payload>(1);
		});
	} else if (is("source-typed")) {
		auto sourceQuery = world.query().all<SparsePayload>().all<Payload>(ecs::QueryTermOptions{}.src(entity).no_access());
		if (sourceQuery.count() != 1)
			return 3;
		assertionExpected = true;
		sourceQuery.each([](const Payload&) {});
	} else if (is("sort")) {
		assertionExpected = true;
		auto sortedQuery = world.query().all<Payload>().no_access().sort_by<Payload>(
				[](const ecs::World&, const void* lhs, const void* rhs) {
					return static_cast<const Payload*>(lhs)->value - static_cast<const Payload*>(rhs)->value;
				});
		sortedQuery.each([](ecs::Entity) {});
	} else if (is("system-typed")) {
		auto system = world.system().all<Payload>().no_access();
		assertionExpected = true;
		system.on_each([](const Payload&) {}).exec();
	} else {
		assertionExpected = true;
		if (is("typed-read") || is("empty-typed"))
			query.each([](const Payload&) {});
		else if (is("typed-write"))
			query.each([](Payload&) {});
		else if (is("empty-parallel")) {
			auto job = query.job([](const Payload&) {}, ecs::QueryExecType::Parallel);
			job.submit();
			job.wait();
		} else if (is("array") || is("empty-array")) {
			cnt::darr<Payload> values;
			query.arr(values);
		} else {
			query.each([&](ecs::Iter& it) {
				if (is("each-iter"))
					query.each_iter(it, [](const Payload&) {});
				else if (is("view-read") || is("explicit-read") || is("explicit-write"))
					(void)it.view<Payload>();
				else if (is("view-write"))
					(void)it.view_mut<Payload>();
				else if (is("view-indexed"))
					(void)it.view<Payload>(0);
				else if (is("view-auto"))
					(void)it.view_auto<Payload>();
				else if (is("sview"))
					(void)it.sview_mut<Payload>(0);
				else if (is("raw-indexed"))
					(void)it.view_raw(0);
				else if (is("raw-entity"))
					(void)it.view_raw_any(payload);
				else if (is("raw-mutable"))
					(void)it.sview_raw_any_mut(payload);
				else if (is("modify"))
					it.modify<Payload, true>();
			});
		}
	}
	assertionExpected = false;
	return 0;
}

#include "test_common.h"

namespace {
	template <uint32_t Idx>
	struct SparseTouchProbe {
		uint32_t value = 0;
	};

	template <uint32_t Idx, uint32_t Count>
	void add_sparse_touch_probes(ecs::World& world, ecs::Entity entity) {
		using Probe = SparseTouchProbe<Idx>;
		const auto& item = world.add<Probe>();
		world.add(item.entity, ecs::Sparse);
		world.add(item.entity, ecs::DontFragment);
		world.add<Probe>(entity);
		if constexpr (Idx + 1 < Count)
			add_sparse_touch_probes<Idx + 1, Count>(world, entity);
	}

	template <uint32_t Idx, uint32_t Count>
	void write_sparse_touch_probes(ecs::Iter& it) {
		using Probe = SparseTouchProbe<Idx>;
		auto view = it.view_any_mut<Probe>();
		if constexpr (Idx < ecs::ChunkHeader::MAX_COMPONENTS) {
			CHECK(view.size() == 1);
			view[0].value = Idx + 1;
		} else {
			CHECK(view.size() == 0);
			if (view.size() != 0)
				view[0].value = Idx + 1;
		}

		if constexpr (Idx + 1 < Count)
			write_sparse_touch_probes<Idx + 1, Count>(it);
	}
} // namespace

TEST_CASE("Sparse DontFragment component and non-fragmenting relation storage") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);
	wld.del(compItem.entity, ecs::DontFragment);
	CHECK(wld.has(compItem.entity, ecs::DontFragment));

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add<PositionSparse>(e);
	CHECK(wld.has<PositionSparse>(e));
	CHECK(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);

	{
		auto pos = wld.set<PositionSparse>(e);
		pos = {1.0f, 2.0f, 3.0f};
	}

	const auto& posConst = wld.get<PositionSparse>(e);
	CHECK(posConst.x == doctest::Approx(1.0f));
	CHECK(posConst.y == doctest::Approx(2.0f));
	CHECK(posConst.z == doctest::Approx(3.0f));

	wld.del<PositionSparse>(e);
	CHECK_FALSE(wld.has<PositionSparse>(e));
	CHECK_FALSE(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Typed sparse mutable views reject touched-term capacity exhaustion") {
	TestWorld twld;

	constexpr uint32_t ProbeCount = ecs::ChunkHeader::MAX_COMPONENTS + 1;
	const auto entity = wld.add();
	wld.add<Position>(entity);
	add_sparse_touch_probes<0, ProbeCount>(wld, entity);

	wld.query().all<Position>().each([&](ecs::Iter& it) {
		write_sparse_touch_probes<0, ProbeCount>(it);
	});

	CHECK(
			wld.get<SparseTouchProbe<ecs::ChunkHeader::MAX_COMPONENTS - 1>>(entity).value ==
			ecs::ChunkHeader::MAX_COMPONENTS);
	CHECK(wld.get<SparseTouchProbe<ecs::ChunkHeader::MAX_COMPONENTS>>(entity).value == 0);
}

TEST_CASE("Builder handles sticky traits and non-fragmenting relation ids") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.build(compItem.entity).add(ecs::DontFragment).del(ecs::DontFragment).add(ecs::Sparse).del(ecs::Sparse);
	CHECK(wld.has(compItem.entity, ecs::DontFragment));
	CHECK(wld.has(compItem.entity, ecs::Sparse));
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Sparse);

	const auto parent = wld.add();
	const auto child = wld.add();
	const auto* pArchetypeBefore = wld.fetch(child).pArchetype;

	wld.build(child).add(ecs::Pair(ecs::Parent, parent));
	CHECK(wld.has(ecs::Pair(ecs::Parent, parent)));
	CHECK(wld.target(child, ecs::Parent) == parent);
	CHECK(wld.fetch(child).pArchetype == pArchetypeBefore);

	wld.build(child).add(ecs::Pair(ecs::Parent, parent));
	CHECK(wld.has(ecs::Pair(ecs::Parent, parent)));
	CHECK(wld.target(child, ecs::Parent) == parent);
	CHECK(wld.fetch(child).pArchetype == pArchetypeBefore);

	wld.build(child).del(ecs::Pair(ecs::Parent, parent));
	CHECK_FALSE(wld.has(child, ecs::Pair(ecs::Parent, parent)));
	CHECK(wld.fetch(child).pArchetype == pArchetypeBefore);
}

TEST_CASE("DontFragment table component uses sparse storage") {
	TestWorld twld;

	const auto& compItem = wld.add<Position>();
	wld.add(compItem.entity, ecs::DontFragment);
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Sparse);
	CHECK(wld.component_uses_sparse_storage(compItem.entity));

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add<Position>(e);
	CHECK(wld.has<Position>(e));
	CHECK(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
	CHECK_FALSE(wld.fetch(e).pArchetype->has(compItem.entity));

	{
		auto pos = wld.set<Position>(e);
		pos = {1.0f, 2.0f, 3.0f};
	}

	const auto& posConst = wld.get<Position>(e);
	CHECK(posConst.x == doctest::Approx(1.0f));
	CHECK(posConst.y == doctest::Approx(2.0f));
	CHECK(posConst.z == doctest::Approx(3.0f));

	wld.del<Position>(e);
	CHECK_FALSE(wld.has<Position>(e));
	CHECK_FALSE(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Sparse DontFragment component runtime object add with value") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);

	const auto e = wld.add();
	wld.add(e, compItem.entity, PositionSparse{4.0f, 5.0f, 6.0f});

	CHECK(wld.has<PositionSparse>(e));
	CHECK(wld.has(e, compItem.entity));

	const auto& pos = wld.get<PositionSparse>(e);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));
}

TEST_CASE("Sparse DontFragment component direct query terms are evaluated as entity filters") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);

	const auto eA = wld.add();
	const auto eB = wld.add();
	const auto eC = wld.add();

	wld.add<Position>(eA);
	wld.add<Position>(eB);
	wld.add<Position>(eC);
	wld.add<Scale>(eC);

	wld.add<PositionSparse>(eA);

	auto qAll = wld.query().all<Position>().all<PositionSparse>();
	CHECK(qAll.count() == 1);
	expect_exact_entities(qAll, {eA});

	auto qNo = wld.query().all<Position>().no<PositionSparse>();
	CHECK(qNo.count() == 2);
	expect_exact_entities(qNo, {eB, eC});

	auto qOr = wld.query().or_<PositionSparse>().or_<Scale>();
	CHECK(qOr.count() == 2);
	expect_exact_entities(qOr, {eA, eC});
}

TEST_CASE("DontFragment table component direct query terms are evaluated as entity filters") {
	TestWorld twld;

	const auto& compItem = wld.add<Position>();
	wld.add(compItem.entity, ecs::DontFragment);

	const auto eA = wld.add();
	const auto eB = wld.add();
	const auto eC = wld.add();

	wld.add<Rotation>(eA);
	wld.add<Rotation>(eB);
	wld.add<Rotation>(eC);
	wld.add<Scale>(eC);

	wld.add<Position>(eA);

	auto qAll = wld.query().all<Rotation>().all<Position>();
	CHECK(qAll.count() == 1);
	expect_exact_entities(qAll, {eA});

	auto qNo = wld.query().all<Rotation>().no<Position>();
	CHECK(qNo.count() == 2);
	expect_exact_entities(qNo, {eB, eC});

	auto qOr = wld.query().or_<Position>().or_<Scale>();
	CHECK(qOr.count() == 2);
	expect_exact_entities(qOr, {eA, eC});
}

TEST_CASE("Sparse DontFragment component can change directly during serial query iteration") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	wld.add(compItem.entity, ecs::DontFragment);

	const auto eA = wld.add();
	const auto eB = wld.add();
	const auto eC = wld.add();
	wld.add<Position>(eA);
	wld.add<Position>(eB);
	wld.add<Position>(eC);
	wld.add<PositionSparse>(eC, {9.0f, 10.0f, 11.0f});

	const auto* pArchetypeA = wld.fetch(eA).pArchetype;
	const auto* pArchetypeB = wld.fetch(eB).pArchetype;
	const auto* pArchetypeC = wld.fetch(eC).pArchetype;

	uint32_t hits = 0;
	wld.query().all<Position>().each([&](ecs::Iter& it) {
		const auto entities = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			const auto entity = entities[i];
			if (entity == eA) {
				wld.add<PositionSparse>(entity);
			} else if (entity == eB) {
				wld.add<PositionSparse>(entity, {4.0f, 5.0f, 6.0f});
			} else if (entity == eC) {
				wld.del<PositionSparse>(entity);
			}
			++hits;
		}
	});

	CHECK(hits == 3);
	CHECK(wld.has<PositionSparse>(eA));
	CHECK(wld.has<PositionSparse>(eB));
	CHECK_FALSE(wld.has<PositionSparse>(eC));
	CHECK(wld.fetch(eA).pArchetype == pArchetypeA);
	CHECK(wld.fetch(eB).pArchetype == pArchetypeB);
	CHECK(wld.fetch(eC).pArchetype == pArchetypeC);

	const auto& posB = wld.get<PositionSparse>(eB);
	CHECK(posB.x == doctest::Approx(4.0f));
	CHECK(posB.y == doctest::Approx(5.0f));
	CHECK(posB.z == doctest::Approx(6.0f));
}

TEST_CASE("Compile-time sparse component uses sparse storage and still fragments") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	CHECK(wld.has(compItem.entity, ecs::Sparse));
	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add<PositionSparse>(e, {1.0f, 2.0f, 3.0f});
	CHECK(wld.has<PositionSparse>(e));
	CHECK(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype != pArchetypeBefore);
	CHECK(wld.fetch(e).pArchetype->has(compItem.entity));

	auto q = wld.query().all<PositionSparse&>();
	uint32_t hits = 0;
	q.each([&](PositionSparse& pos) {
		pos.x += 1.0f;
		++hits;
	});
	CHECK(hits == 1);
	CHECK(wld.get<PositionSparse>(e).x == doctest::Approx(2.0f));

	wld.del<PositionSparse>(e);
	CHECK_FALSE(wld.has<PositionSparse>(e));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Sparse component default add still makes direct storage") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add<PositionSparse>(e);
	CHECK(wld.has<PositionSparse>(e));
	CHECK(wld.has(e, compItem.entity));
	CHECK(wld.fetch(e).pArchetype != pArchetypeBefore);
	CHECK(wld.fetch(e).pArchetype->has(compItem.entity));

	{
		auto pos = wld.set<PositionSparse>(e);
		pos = {2.0f, 3.0f, 4.0f};
	}

	const auto& pos = wld.get<PositionSparse>(e);
	CHECK(pos.x == doctest::Approx(2.0f));
	CHECK(pos.y == doctest::Approx(3.0f));
	CHECK(pos.z == doctest::Approx(4.0f));
}

TEST_CASE("Table component can opt into sparse storage via trait") {
	TestWorld twld;

	const auto& compItem = wld.add<Position>();
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Table);
	CHECK_FALSE(wld.has(compItem.entity, ecs::Sparse));

	wld.add(compItem.entity, ecs::Sparse);
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Sparse);
	CHECK(wld.has(compItem.entity, ecs::Sparse));
	CHECK(wld.component_uses_sparse_storage(compItem.entity));
	wld.del(compItem.entity, ecs::Sparse);
	CHECK(wld.has(compItem.entity, ecs::Sparse));
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Sparse);

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add<Position>(e, {1.0f, 2.0f, 3.0f});
	CHECK(wld.has<Position>(e));
	CHECK(wld.fetch(e).pArchetype != pArchetypeBefore);
	CHECK(wld.fetch(e).pArchetype->has(compItem.entity));

	const auto& pos = wld.get<Position>(e);
	CHECK(pos.x == doctest::Approx(1.0f));
	CHECK(pos.y == doctest::Approx(2.0f));
	CHECK(pos.z == doctest::Approx(3.0f));
}

TEST_CASE("Sparse prefab instantiate copies sparse payload") {
	SparseTestWorld twld;

	const auto& compItem = wld.add<PositionSparse>();
	const auto prefab = wld.prefab();
	wld.add<PositionSparse>(prefab, {4.0f, 5.0f, 6.0f});

	const auto instance = wld.instantiate(prefab);

	CHECK(wld.has<PositionSparse>(instance));
	CHECK(wld.fetch(instance).pArchetype->has(compItem.entity));

	const auto& pos = wld.get<PositionSparse>(instance);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));
}

TEST_CASE("Sparse prefab sync keeps copied sparse payload after prefab delete") {
	SparseTestWorld twld;

	const auto prefab = wld.prefab();
	wld.add<PositionSparse>(prefab, {4.0f, 5.0f, 6.0f});
	const auto instance = wld.instantiate(prefab);

	wld.del<PositionSparse>(prefab);
	CHECK(wld.sync(prefab) == 0);

	CHECK(wld.has<PositionSparse>(instance));
	const auto& pos = wld.get<PositionSparse>(instance);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));
}

TEST_CASE("Chunk-backed query view keeps contiguous data access") {
	TestWorld twld;

	const auto e0 = wld.add();
	const auto e1 = wld.add();
	wld.add<Position>(e0, {1.0f, 2.0f, 3.0f});
	wld.add<Position>(e1, {4.0f, 5.0f, 6.0f});

	auto q = wld.query().all<Position>();
	uint32_t hits = 0;
	q.each([&](ecs::Iter& it) {
		auto posView = it.view<Position>();
		CHECK(posView.data() != nullptr);
		CHECK(posView.size() == 2);
		CHECK(posView.data()[0].x == doctest::Approx(1.0f));
		CHECK(posView.data()[1].x == doctest::Approx(4.0f));
		++hits;
	});
	CHECK(hits == 1);
}

TEST_CASE("Out-of-line query view does not expose contiguous data") {
	SparseTestWorld twld;

	const auto e = wld.add();
	wld.add<PositionSparse>(e, {1.0f, 2.0f, 3.0f});

	auto q = wld.query().all<PositionSparse>();
	uint32_t hits = 0;
	q.each([&](ecs::Iter& it) {
		auto posView = it.view_any<PositionSparse>();
		CHECK(posView.data() == nullptr);
		CHECK(posView.size() == 1);
		CHECK(posView[0].x == doctest::Approx(1.0f));
		CHECK(posView[0].y == doctest::Approx(2.0f));
		CHECK(posView[0].z == doctest::Approx(3.0f));
		++hits;
	});
	CHECK(hits == 1);
}

TEST_CASE("Chunk-backed sview_mut keeps contiguous data access") {
	TestWorld twld;

	const auto e0 = wld.add();
	const auto e1 = wld.add();
	wld.add<Position>(e0, {1.0f, 2.0f, 3.0f});
	wld.add<Position>(e1, {4.0f, 5.0f, 6.0f});

	auto q = wld.query().all<Position&>();
	uint32_t hits = 0;
	q.each([&](ecs::Iter& it) {
		auto posView = it.sview_mut<Position>();
		auto posTermView = it.sview_mut<Position>(0);
		CHECK(posView.data() != nullptr);
		CHECK(posTermView.data() != nullptr);
		CHECK(posView.data() == posTermView.data());
		CHECK(posView.size() == 2);
		CHECK(posTermView.size() == 2);

		posView[0].x += 10.0f;
		posTermView[1].x += 20.0f;
		++hits;
	});

	CHECK(hits == 1);
	CHECK(wld.get<Position>(e0).x == doctest::Approx(11.0f));
	CHECK(wld.get<Position>(e1).x == doctest::Approx(24.0f));
}

TEST_CASE("Sparse DontFragment runtime-registered component typed object access") {
	TestWorld twld;

	const auto& runtimeComp = add_runtime_component(
			wld, "Runtime_Sparse_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(runtimeComp.entity, ecs::DontFragment);

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add(e, runtimeComp.entity, Position{7.0f, 8.0f, 9.0f});
	CHECK(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);

	{
		auto posMut = wld.set<Position>(e, runtimeComp.entity);
		posMut = {10.0f, 11.0f, 12.0f};
	}

	const auto& pos = wld.get<Position>(e, runtimeComp.entity);
	CHECK(pos.x == doctest::Approx(10.0f));
	CHECK(pos.y == doctest::Approx(11.0f));
	CHECK(pos.z == doctest::Approx(12.0f));

	wld.del(e, runtimeComp.entity);
	CHECK_FALSE(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Sparse runtime-registered component uses sparse storage and still fragments") {
	TestWorld twld;

	const auto& runtimeComp = add_runtime_component(
			wld, "Runtime_Sparse_Fragmenting_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	CHECK(wld.has(runtimeComp.entity, ecs::Sparse));

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add(e, runtimeComp.entity, Position{7.0f, 8.0f, 9.0f});
	CHECK(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype != pArchetypeBefore);
	CHECK(wld.fetch(e).pArchetype->has(runtimeComp.entity));

	{
		auto posMut = wld.set<Position>(e, runtimeComp.entity);
		posMut = {10.0f, 11.0f, 12.0f};
	}

	const auto& pos = wld.get<Position>(e, runtimeComp.entity);
	CHECK(pos.x == doctest::Approx(10.0f));
	CHECK(pos.y == doctest::Approx(11.0f));
	CHECK(pos.z == doctest::Approx(12.0f));

	wld.del(e, runtimeComp.entity);
	CHECK_FALSE(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Runtime sparse mode drives add/has/get behavior") {
	TestWorld twld;

	const auto& fragComp = add_runtime_component(
			wld, "Runtime_Sparse_Mode_Frag", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	const auto& nonFragComp = add_runtime_component(
			wld, "Runtime_Sparse_Mode_NonFrag", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(nonFragComp.entity, ecs::DontFragment);

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add(e, fragComp.entity, Position{1.0f, 2.0f, 3.0f});
	wld.add(e, nonFragComp.entity, Position{4.0f, 5.0f, 6.0f});

	CHECK(wld.has(e, fragComp.entity));
	CHECK(wld.has(e, nonFragComp.entity));
	CHECK(wld.fetch(e).pArchetype != pArchetypeBefore);
	CHECK(wld.fetch(e).pArchetype->has(fragComp.entity));
	CHECK_FALSE(wld.fetch(e).pArchetype->has(nonFragComp.entity));

	const auto& fragPos = wld.get<Position>(e, fragComp.entity);
	CHECK(fragPos.x == doctest::Approx(1.0f));
	CHECK(fragPos.y == doctest::Approx(2.0f));
	CHECK(fragPos.z == doctest::Approx(3.0f));

	const auto& nonFragPos = wld.get<Position>(e, nonFragComp.entity);
	CHECK(nonFragPos.x == doctest::Approx(4.0f));
	CHECK(nonFragPos.y == doctest::Approx(5.0f));
	CHECK(nonFragPos.z == doctest::Approx(6.0f));
}

TEST_CASE("Sparse copy keeps frag and non-frag payloads") {
	TestWorld twld;

	const auto& fragComp = add_runtime_component(
			wld, "Runtime_Sparse_Copy_Frag", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	const auto& nonFragComp = add_runtime_component(
			wld, "Runtime_Sparse_Copy_NonFrag", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(nonFragComp.entity, ecs::DontFragment);

	const auto src = wld.add();
	wld.add(src, fragComp.entity, Position{1.0f, 2.0f, 3.0f});
	wld.add(src, nonFragComp.entity, Position{4.0f, 5.0f, 6.0f});

	const auto dst = wld.copy(src);
	CHECK(wld.has(dst, fragComp.entity));
	CHECK(wld.has(dst, nonFragComp.entity));
	CHECK(wld.fetch(dst).pArchetype->has(fragComp.entity));
	CHECK_FALSE(wld.fetch(dst).pArchetype->has(nonFragComp.entity));

	{
		auto pos = wld.set<Position>(src, fragComp.entity);
		pos = {10.0f, 11.0f, 12.0f};
	}
	{
		auto pos = wld.set<Position>(src, nonFragComp.entity);
		pos = {13.0f, 14.0f, 15.0f};
	}

	const auto& fragPos = wld.get<Position>(dst, fragComp.entity);
	CHECK(fragPos.x == doctest::Approx(1.0f));
	CHECK(fragPos.y == doctest::Approx(2.0f));
	CHECK(fragPos.z == doctest::Approx(3.0f));

	const auto& nonFragPos = wld.get<Position>(dst, nonFragComp.entity);
	CHECK(nonFragPos.x == doctest::Approx(4.0f));
	CHECK(nonFragPos.y == doctest::Approx(5.0f));
	CHECK(nonFragPos.z == doctest::Approx(6.0f));
}

TEST_CASE("Runtime-registered table component can opt into sparse storage via trait") {
	TestWorld twld;

	const auto& runtimeComp = add_runtime_component(
			wld, "Runtime_Table_Position_Becomes_Sparse", (uint32_t)sizeof(Position), ecs::DataStorageType::Table,
			(uint32_t)alignof(Position));
	CHECK(runtimeComp.comp.storage_type() == ecs::DataStorageType::Table);
	CHECK_FALSE(wld.has(runtimeComp.entity, ecs::Sparse));

	wld.add(runtimeComp.entity, ecs::Sparse);
	CHECK(runtimeComp.comp.storage_type() == ecs::DataStorageType::Sparse);
	CHECK(wld.has(runtimeComp.entity, ecs::Sparse));
	CHECK(wld.component_uses_sparse_storage(runtimeComp.entity));

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add(e, runtimeComp.entity, Position{7.0f, 8.0f, 9.0f});
	CHECK(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype != pArchetypeBefore);
	CHECK(wld.fetch(e).pArchetype->has(runtimeComp.entity));

	const auto& pos = wld.get<Position>(e, runtimeComp.entity);
	CHECK(pos.x == doctest::Approx(7.0f));
	CHECK(pos.y == doctest::Approx(8.0f));
	CHECK(pos.z == doctest::Approx(9.0f));
}

TEST_CASE("DontFragment runtime-registered table component typed object access") {
	TestWorld twld;

	const auto& runtimeComp = add_runtime_component(
			wld, "Runtime_Table_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Table,
			(uint32_t)alignof(Position));
	wld.add(runtimeComp.entity, ecs::DontFragment);
	CHECK(runtimeComp.comp.storage_type() == ecs::DataStorageType::Sparse);
	CHECK(wld.component_uses_sparse_storage(runtimeComp.entity));

	const auto e = wld.add();
	const auto* pArchetypeBefore = wld.fetch(e).pArchetype;

	wld.add(e, runtimeComp.entity, Position{7.0f, 8.0f, 9.0f});
	CHECK(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
	CHECK_FALSE(wld.fetch(e).pArchetype->has(runtimeComp.entity));

	{
		auto posMut = wld.set<Position>(e, runtimeComp.entity);
		posMut = {10.0f, 11.0f, 12.0f};
	}

	const auto& pos = wld.get<Position>(e, runtimeComp.entity);
	CHECK(pos.x == doctest::Approx(10.0f));
	CHECK(pos.y == doctest::Approx(11.0f));
	CHECK(pos.z == doctest::Approx(12.0f));

	wld.del(e, runtimeComp.entity);
	CHECK_FALSE(wld.has(e, runtimeComp.entity));
	CHECK(wld.fetch(e).pArchetype == pArchetypeBefore);
}

TEST_CASE("Runtime-registered component accessor object setter paths") {
	TestWorld twld;

	const auto& runtimeTableComp = add_runtime_component(
			wld, "Runtime_Table_Position_Setter", (uint32_t)sizeof(Position), ecs::DataStorageType::Table,
			(uint32_t)alignof(Position));
	const auto& runtimeSparseComp = add_runtime_component(
			wld, "Runtime_Sparse_Position_Setter", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(runtimeSparseComp.entity, ecs::DontFragment);

	const auto e = wld.add();
	wld.add(e, runtimeTableComp.entity, Position{1.0f, 2.0f, 3.0f});
	wld.add(e, runtimeSparseComp.entity, Position{4.0f, 5.0f, 6.0f});

	auto setter = wld.acc_mut(e);
	setter.set<Position>(runtimeTableComp.entity, Position{7.0f, 8.0f, 9.0f});
	setter.sset<Position>(runtimeTableComp.entity, Position{10.0f, 11.0f, 12.0f});
	setter.set<Position>(runtimeSparseComp.entity, Position{13.0f, 14.0f, 15.0f});
	setter.sset<Position>(runtimeSparseComp.entity, Position{16.0f, 17.0f, 18.0f});

	const auto getter = wld.acc(e);
	const auto& tablePos = getter.get<Position>(runtimeTableComp.entity);
	CHECK(tablePos.x == doctest::Approx(10.0f));
	CHECK(tablePos.y == doctest::Approx(11.0f));
	CHECK(tablePos.z == doctest::Approx(12.0f));

	const auto& sparsePos = getter.get<Position>(runtimeSparseComp.entity);
	CHECK(sparsePos.x == doctest::Approx(16.0f));
	CHECK(sparsePos.y == doctest::Approx(17.0f));
	CHECK(sparsePos.z == doctest::Approx(18.0f));
}

TEST_CASE("Sparse DontFragment runtime-registered component is removed on entity delete") {
	TestWorld twld;

	const auto& runtimeComp = add_runtime_component(
			wld, "Runtime_Sparse_Position_Delete", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(runtimeComp.entity, ecs::DontFragment);

	const auto e = wld.add();
	wld.add(e, runtimeComp.entity, Position{1.0f, 2.0f, 3.0f});
	CHECK(wld.has(e, runtimeComp.entity));

	wld.del(e);
	wld.update();

	CHECK_FALSE(wld.has(e));
}

TEST_CASE("Clear removes table unique and sparse component state") {
	SparseTestWorld twld;

	const auto e = wld.add();
	wld.add<ecs::uni<Position>>(e, {1.0f, 2.0f, 3.0f});
	wld.add<PositionSparse>(e, {4.0f, 5.0f, 6.0f});
	wld.add<Rotation>(e, {7.0f, 8.0f, 9.0f, 10.0f});

	CHECK(wld.has<ecs::uni<Position>>(e));
	CHECK(wld.has<PositionSparse>(e));
	CHECK(wld.has<Rotation>(e));

	wld.clear(e);

	CHECK(wld.has(e));
	CHECK_FALSE(wld.has<ecs::uni<Position>>(e));
	CHECK_FALSE(wld.has<PositionSparse>(e));
	CHECK_FALSE(wld.has<Rotation>(e));

	wld.add<ecs::uni<Position>>(e);
	wld.add<PositionSparse>(e);

	auto& posUni = wld.acc_mut(e).mut<ecs::uni<Position>>();
	posUni = {11.0f, 12.0f, 13.0f};
	{
		auto posSparse = wld.set<PositionSparse>(e);
		posSparse = {14.0f, 15.0f, 16.0f};
	}

	CHECK(wld.get<ecs::uni<Position>>(e).x == doctest::Approx(11.0f));
	CHECK(wld.get<PositionSparse>(e).x == doctest::Approx(14.0f));
}

TEST_CASE("EntityContainer cached entity slot across row swap and archetype move") {
	TestWorld twld;

	const auto targetA = wld.add();
	const auto targetB = wld.add();
	const auto sourceA = wld.add();
	const auto sourceB = wld.add();

	wld.child(sourceA, targetA);
	wld.child(sourceB, targetB);

	const auto* pTargetBBefore = wld.fetch(targetB).pEntity;
	CHECK(pTargetBBefore != nullptr);
	if (pTargetBBefore != nullptr)
		CHECK(*pTargetBBefore == targetB);
	CHECK(wld.target(sourceB, ecs::ChildOf) == targetB);

	wld.del(targetA);

	const auto& targetBAfterDelete = wld.fetch(targetB);
	CHECK(targetBAfterDelete.pEntity != nullptr);
	if (targetBAfterDelete.pEntity != nullptr)
		CHECK(*targetBAfterDelete.pEntity == targetB);
	CHECK(wld.target(sourceB, ecs::ChildOf) == targetB);

	wld.add<Position>(targetB, {1.0f, 2.0f, 3.0f});

	const auto& targetBAfterMove = wld.fetch(targetB);
	CHECK(targetBAfterMove.pEntity != nullptr);
	if (targetBAfterMove.pEntity != nullptr)
		CHECK(*targetBAfterMove.pEntity == targetB);
	CHECK(wld.target(sourceB, ecs::ChildOf) == targetB);
}

TEST_CASE("World set writes back when the proxy finishes") {
	SparseTestWorld twld;

	SUBCASE("chunk-backed component") {
		const auto e = wld.add();
		wld.add<Position>(e, {1.0f, 2.0f, 3.0f});

		{
			auto pos = wld.set<Position>(e);
			pos.x = 10.0f;
			pos.y = 11.0f;
			pos.z = 12.0f;

			const auto& current = wld.get<Position>(e);
			CHECK(current.x == doctest::Approx(1.0f));
			CHECK(current.y == doctest::Approx(2.0f));
			CHECK(current.z == doctest::Approx(3.0f));
		}

		const auto& updated = wld.get<Position>(e);
		CHECK(updated.x == doctest::Approx(10.0f));
		CHECK(updated.y == doctest::Approx(11.0f));
		CHECK(updated.z == doctest::Approx(12.0f));
	}

	SUBCASE("sparse component") {
		const auto e = wld.add();
		wld.add<PositionSparse>(e, {4.0f, 5.0f, 6.0f});

		{
			auto pos = wld.set<PositionSparse>(e);
			pos.x = 14.0f;
			pos.y = 15.0f;
			pos.z = 16.0f;

			const auto& current = wld.get<PositionSparse>(e);
			CHECK(current.x == doctest::Approx(4.0f));
			CHECK(current.y == doctest::Approx(5.0f));
			CHECK(current.z == doctest::Approx(6.0f));
		}

		const auto& updated = wld.get<PositionSparse>(e);
		CHECK(updated.x == doctest::Approx(14.0f));
		CHECK(updated.y == doctest::Approx(15.0f));
		CHECK(updated.z == doctest::Approx(16.0f));
	}
}

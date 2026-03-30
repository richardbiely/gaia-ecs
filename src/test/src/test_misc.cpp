#include "test_common.h"

struct AutoRegQueryProbe {
	int value = 0;
};

struct AutoRegSystemProbe {
	int value = 0;
};

struct AutoRegObserverProbe {
	int value = 0;
};

TEST_CASE("DataLayout SoA - ECS") {
	TestDataLayoutSoA_ECS<PositionSoA>();
	TestDataLayoutSoA_ECS<RotationSoA>();
}

TEST_CASE("DataLayout SoA8 - ECS") {
	TestDataLayoutSoA_ECS<PositionSoA8>();
	TestDataLayoutSoA_ECS<RotationSoA8>();
}

TEST_CASE("DataLayout SoA16 - ECS") {
	TestDataLayoutSoA_ECS<PositionSoA16>();
	TestDataLayoutSoA_ECS<RotationSoA16>();
}

//------------------------------------------------------------------------------
// Component cache
//------------------------------------------------------------------------------

template <typename T>
void comp_cache_verify(ecs::World& w, const ecs::ComponentCacheItem& other) {
	const auto& d = w.add<const Position>();
	CHECK(other.entity == d.entity);
	CHECK(other.comp == d.comp);
}

TEST_CASE("Component cache") {
	{
		TestWorld twld;
		const auto& item = wld.add<Position>();
		auto ent = item.entity;
		CHECK_FALSE(ent.entity());

		comp_cache_verify<const Position>(wld, item);
		comp_cache_verify<Position&>(wld, item);
		comp_cache_verify<const Position&>(wld, item);
		comp_cache_verify<Position*>(wld, item);
		comp_cache_verify<const Position*>(wld, item);
	}
	{
		TestWorld twld;
		const auto& item = wld.add<const Position>();
		auto ent = item.entity;
		CHECK_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, item);
		comp_cache_verify<Position&>(wld, item);
		comp_cache_verify<const Position&>(wld, item);
		comp_cache_verify<Position*>(wld, item);
		comp_cache_verify<const Position*>(wld, item);
	}
	{
		TestWorld twld;
		const auto& item = wld.add<Position&>();
		auto ent = item.entity;
		CHECK_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, item);
		comp_cache_verify<const Position>(wld, item);
		comp_cache_verify<const Position&>(wld, item);
		comp_cache_verify<Position*>(wld, item);
		comp_cache_verify<const Position*>(wld, item);
	}
	{
		TestWorld twld;
		const auto& item = wld.add<const Position&>();
		auto ent = item.entity;
		CHECK_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, item);
		comp_cache_verify<const Position>(wld, item);
		comp_cache_verify<Position&>(wld, item);
		comp_cache_verify<Position*>(wld, item);
		comp_cache_verify<const Position*>(wld, item);
	}
	{
		TestWorld twld;
		const auto& item = wld.add<Position*>();
		auto ent = item.entity;
		CHECK_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, item);
		comp_cache_verify<const Position>(wld, item);
		comp_cache_verify<Position&>(wld, item);
		comp_cache_verify<const Position&>(wld, item);
		comp_cache_verify<const Position*>(wld, item);
	}
	{
		TestWorld twld;
		const auto& item = wld.add<const Position*>();
		auto ent = item.entity;
		CHECK_FALSE(ent.entity());

		comp_cache_verify<Position>(wld, item);
		comp_cache_verify<const Position>(wld, item);
		comp_cache_verify<Position&>(wld, item);
		comp_cache_verify<const Position&>(wld, item);
		comp_cache_verify<Position*>(wld, item);
	}

	{
		SparseTestWorld twld;
		const auto& dense = wld.add<Position>();
		const auto& sparse = wld.add<PositionSparse>();
		CHECK(dense.comp.storage_type() == ecs::DataStorageType::Table);
		CHECK(sparse.comp.storage_type() == ecs::DataStorageType::Sparse);
	}
}

TEST_CASE("Component cache - runtime registration") {
	SUBCASE("basic registration populates runtime metadata and lookups") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		constexpr const char* RuntimeCompName = "Runtime_Component_Basic";
		const auto entity = wld.add();
		const auto& item = cc.add(
				entity, RuntimeCompName, 0, //
				(uint32_t)sizeof(Position), //
				ecs::DataStorageType::Table, //
				(uint32_t)alignof(Position) //
		);

		const auto nameLen = (uint32_t)GAIA_STRLEN(RuntimeCompName, ecs::ComponentCacheItem::MaxNameLength);
		CHECK(item.entity == entity);
		CHECK(item.comp.id() >= 0x80000000u);
		CHECK(item.comp.size() == (uint32_t)sizeof(Position));
		CHECK(item.comp.alig() == (uint32_t)alignof(Position));
		CHECK(item.comp.soa() == 0);
		CHECK(item.name.len() == nameLen);
		CHECK(strcmp(item.name.str(), RuntimeCompName) == 0);
		CHECK(item.hashLookup.hash == core::calculate_hash64(RuntimeCompName, nameLen));

		CHECK(cc.find(item.comp.id()) == &item);
		CHECK(cc.find(item.entity) == &item);
		CHECK(wld.symbol(RuntimeCompName) == item.entity);
		CHECK(wld.symbol(RuntimeCompName, nameLen) == item.entity);
		CHECK(wld.symbol(RuntimeCompName, nameLen - 1) == ecs::EntityBad);
		CHECK(wld.get(RuntimeCompName) == item.entity);
	}

	SUBCASE("duplicate runtime registration is idempotent") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		constexpr const char* RuntimeCompName = "Runtime_Component_Duplicate";
		const auto entityA = wld.add();
		const auto& original = cc.add(entityA, RuntimeCompName, 0, 16, ecs::DataStorageType::Table, 4);
		const auto originalHash = original.hashLookup.hash;

		constexpr uint8_t SoaSizes[] = {1, 2, 4};
		const ecs::ComponentLookupHash customHash{0x123456789abcdef0ull};
		const auto entityB = wld.add(ecs::EntityKind::EK_Uni);
		const auto& duplicate =
				cc.add(entityB, RuntimeCompName, 0, 64, ecs::DataStorageType::Table, 8, 3, SoaSizes, customHash);

		CHECK(&duplicate == &original);
		CHECK(duplicate.entity == original.entity);
		CHECK(duplicate.entity == entityA);
		CHECK(duplicate.entity != entityB);
		CHECK(duplicate.comp.id() == original.comp.id());
		CHECK(duplicate.comp.size() == 16);
		CHECK(duplicate.comp.alig() == 4);
		CHECK(duplicate.comp.soa() == 0);
		CHECK(duplicate.hashLookup.hash == originalHash);
		CHECK(duplicate.entity.kind() == ecs::EntityKind::EK_Gen);
	}

	SUBCASE("custom hash and soa metadata are preserved") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		constexpr const char* RuntimeCompName = "Runtime_Component_SoA";
		constexpr uint8_t SoaSizes[] = {4, 1, 2};
		const ecs::ComponentLookupHash customHash{0x00f00d00baadf00dull};
		const auto entity = wld.add(ecs::EntityKind::EK_Uni);

		const auto& item = cc.add(entity, RuntimeCompName, 0, 32, ecs::DataStorageType::Table, 8, 3, SoaSizes, customHash);

		CHECK(item.entity == entity);
		CHECK(item.entity.kind() == ecs::EntityKind::EK_Uni);
		CHECK(item.comp.soa() == 3);
		CHECK(item.comp.size() == 32);
		CHECK(item.comp.alig() == 8);
		CHECK(item.hashLookup.hash == customHash.hash);
		CHECK(item.soaSizes[0] == SoaSizes[0]);
		CHECK(item.soaSizes[1] == SoaSizes[1]);
		CHECK(item.soaSizes[2] == SoaSizes[2]);

		CHECK(item.func_ctor == nullptr);
		CHECK(item.func_move_ctor == nullptr);
		CHECK(item.func_copy_ctor == nullptr);
		CHECK(item.func_dtor == nullptr);
		CHECK(item.func_copy == nullptr);
		CHECK(item.func_move == nullptr);
		CHECK(item.func_swap == nullptr);
		CHECK(item.func_cmp == nullptr);
		CHECK(item.func_save == nullptr);
		CHECK(item.func_load == nullptr);

		const auto nameLen = (uint32_t)GAIA_STRLEN(RuntimeCompName, ecs::ComponentCacheItem::MaxNameLength);
		CHECK(wld.symbol(RuntimeCompName, nameLen) == item.entity);
		CHECK(cc.get(item.comp.id()).entity == item.entity);
	}

	SUBCASE("symbol path alias and display name each have explicit behavior") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		auto entityA = wld.add();
		(void)cc.add(entityA, "Gameplay::Device", 0, 0, ecs::DataStorageType::Table, 1);
		auto& itemA = cc.get(entityA);

		auto entityB = wld.add();
		(void)cc.add(entityB, "Debug::Device", 0, 0, ecs::DataStorageType::Table, 1);
		auto& itemB = cc.get(entityB);

		CHECK(wld.symbol(itemA.entity) == "Gameplay::Device");
		CHECK(wld.path(itemA.entity) == "Gameplay.Device");
		CHECK(wld.alias(itemA.entity).empty());
		CHECK(wld.symbol("Gameplay::Device") == itemA.entity);
		CHECK(wld.path("Gameplay.Device") == itemA.entity);
		CHECK(wld.alias("Device") == ecs::EntityBad);
		CHECK(wld.get("Gameplay::Device") == itemA.entity);
		CHECK(wld.get("Gameplay.Device") == itemA.entity);
		CHECK(wld.get("Device") == ecs::EntityBad);
		CHECK(wld.display_name(itemA.entity) == "Gameplay.Device");
		CHECK(wld.display_name(itemB.entity) == "Debug.Device");

		CHECK(wld.alias(itemA.entity, "GameplayDevice"));
		CHECK(wld.alias("GameplayDevice") == itemA.entity);
		CHECK(wld.get("GameplayDevice") == itemA.entity);
		CHECK(wld.display_name(itemA.entity) == "GameplayDevice");

		CHECK(wld.path(itemA.entity, "gameplay.render.Device"));
		CHECK(wld.path("gameplay.render.Device") == itemA.entity);
		CHECK(wld.get("gameplay.render.Device") == itemA.entity);

		CHECK(wld.alias(itemA.entity, nullptr));
		CHECK(wld.display_name(itemA.entity) == "gameplay.render.Device");

		CHECK(wld.path(itemA.entity, nullptr));
		CHECK(wld.display_name(itemA.entity) == "Gameplay::Device");
	}

	SUBCASE("world resolve collects ambiguous matches for diagnostics") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		const auto entityA = wld.add();
		(void)cc.add(entityA, "Gameplay::Device", 0, 0, ecs::DataStorageType::Table, 1);

		const auto entityB = wld.add();
		(void)cc.add(entityB, "Debug::Device", 0, 0, ecs::DataStorageType::Table, 1);

		cnt::darray<ecs::Entity> out;
		wld.resolve(out, "Device");
		CHECK(out.empty());

		wld.resolve(out, "Gameplay.Device");
		CHECK(out.size() == 1);
		CHECK(out[0] == entityA);

		wld.resolve(out, "Gameplay::Device");
		CHECK(out.size() == 1);
		CHECK(out[0] == entityA);
	}

	SUBCASE("schema field registration supports add update and clear") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		const auto entity = wld.add();
		(void)cc.add(entity, "Runtime_Component_Schema", 0, 24, ecs::DataStorageType::Table, 8);
		auto& item = cc.get(entity);

		CHECK_FALSE(item.has_fields());
		CHECK(item.schema.size() == 0);

		CHECK(item.set_field("x", 0, ser::serialization_type_id::f32, 0, 4));
		CHECK(item.has_fields());
		CHECK(item.schema.size() == 1);
		CHECK(strcmp(item.schema[0].name, "x") == 0);
		CHECK(item.schema[0].type == ser::serialization_type_id::f32);
		CHECK(item.schema[0].offset == 0);
		CHECK(item.schema[0].size == 4);

		// Re-registering an existing field updates metadata instead of adding a duplicate.
		CHECK(item.set_field("x", 0, ser::serialization_type_id::u32, 4, 8));
		CHECK(item.schema.size() == 1);
		CHECK(strcmp(item.schema[0].name, "x") == 0);
		CHECK(item.schema[0].type == ser::serialization_type_id::u32);
		CHECK(item.schema[0].offset == 4);
		CHECK(item.schema[0].size == 8);

		// Explicit length should be honored and copied as the canonical field name.
		CHECK(item.set_field("velocity_data", 8, ser::serialization_type_id::f64, 12, 8));
		CHECK(item.schema.size() == 2);
		CHECK(strcmp(item.schema[1].name, "velocity") == 0);
		CHECK(item.schema[1].type == ser::serialization_type_id::f64);
		CHECK(item.schema[1].offset == 12);
		CHECK(item.schema[1].size == 8);

		const auto schemaSizeBeforeInvalid = item.schema.size();
		CHECK_FALSE(item.set_field(nullptr, 0, ser::serialization_type_id::u8, 0, 1));
		CHECK_FALSE(item.set_field("", 0, ser::serialization_type_id::u8, 0, 1));

		char oversizedFieldName[ecs::ComponentCacheItem::MaxNameLength + 1] = {};
		GAIA_FOR(ecs::ComponentCacheItem::MaxNameLength) oversizedFieldName[i] = 'a';
		oversizedFieldName[ecs::ComponentCacheItem::MaxNameLength] = 0;
		CHECK_FALSE(item.set_field(
				oversizedFieldName, ecs::ComponentCacheItem::MaxNameLength, ser::serialization_type_id::u8, 0, 1));

		CHECK(item.schema.size() == schemaSizeBeforeInvalid);

		item.clear_fields();
		CHECK_FALSE(item.has_fields());
		CHECK(item.schema.size() == 0);
	}

	SUBCASE("runtime descriptor ids are monotonic") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		const auto& a = cc.add(wld.add(), "Runtime_Component_A", 0, 1, ecs::DataStorageType::Table, 1);
		const auto& b = cc.add(wld.add(), "Runtime_Component_B", 0, 2, ecs::DataStorageType::Table, 1);
		const auto& c = cc.add(wld.add(), "Runtime_Component_C", 0, 3, ecs::DataStorageType::Table, 1);

		CHECK(a.comp.id() >= 0x80000000u);
		CHECK(b.comp.id() == a.comp.id() + 1);
		CHECK(c.comp.id() == b.comp.id() + 1);
	}
}

#if GAIA_ECS_AUTO_COMPONENT_REGISTRATION
TEST_CASE("Typed APIs - auto component registration is enabled") {
#else
TEST_CASE("Typed APIs - explicit component registration is required") {
#endif
	TestWorld twld;
	auto& cc = wld.comp_cache();

#if GAIA_ECS_AUTO_COMPONENT_REGISTRATION
	CHECK(cc.template find<AutoRegQueryProbe>() == nullptr);
	auto q = wld.query().all<AutoRegQueryProbe>();
	CHECK(cc.template find<AutoRegQueryProbe>() != nullptr);
	CHECK(q.count() == 0);

	CHECK(cc.template find<AutoRegSystemProbe>() == nullptr);
	uint32_t systemHits = 0;
	auto sys = wld.system().all<AutoRegSystemProbe>().on_each([&](const AutoRegSystemProbe&) {
		++systemHits;
	});
	CHECK(cc.template find<AutoRegSystemProbe>() != nullptr);

	CHECK(cc.template find<AutoRegObserverProbe>() == nullptr);
	uint32_t observerHits = 0;
	auto obs = wld.observer().event(ecs::ObserverEvent::OnAdd).all<AutoRegObserverProbe>().on_each([&](ecs::Entity) {
		++observerHits;
	});
	CHECK(cc.template find<AutoRegObserverProbe>() != nullptr);
#else
	const auto& queryComp = wld.add<AutoRegQueryProbe>();
	auto q = wld.query().all<AutoRegQueryProbe>();
	CHECK(cc.template find<AutoRegQueryProbe>() == &queryComp);
	CHECK(q.count() == 0);

	const auto& systemComp = wld.add<AutoRegSystemProbe>();
	uint32_t systemHits = 0;
	auto sys = wld.system().all<AutoRegSystemProbe>().on_each([&](const AutoRegSystemProbe&) {
		++systemHits;
	});
	CHECK(cc.template find<AutoRegSystemProbe>() == &systemComp);

	const auto& observerComp = wld.add<AutoRegObserverProbe>();
	uint32_t observerHits = 0;
	auto obs = wld.observer().event(ecs::ObserverEvent::OnAdd).all<AutoRegObserverProbe>().on_each([&](ecs::Entity) {
		++observerHits;
	});
	CHECK(cc.template find<AutoRegObserverProbe>() == &observerComp);
#endif

	const auto e = wld.add();
	wld.add<AutoRegSystemProbe>(e, {1});
	wld.update();
	CHECK(systemHits == 1);
	(void)sys;

	const auto eObs = wld.add();
	wld.add<AutoRegObserverProbe>(eObs, {1});
	CHECK(observerHits == 1);
	(void)obs;
}

//------------------------------------------------------------------------------
// Hooks
//------------------------------------------------------------------------------

#if GAIA_ENABLE_HOOKS

static thread_local uint32_t hook_trigger_cnt = 0;

TEST_CASE("Hooks") {
	#if GAIA_ENABLE_ADD_DEL_HOOKS
	SUBCASE("add") {
		TestWorld twld;
		const auto& pitem = wld.add<Position>();
		hook_trigger_cnt = 0;

		auto e = wld.add();
		wld.add<Position>(e);
		CHECK(hook_trigger_cnt == 0);
		wld.add<Rotation>(e);
		CHECK(hook_trigger_cnt == 0);

		// Set up a hook for adding a Position component
		// ecs::ComponentCache::hooks(pitem).func_add = position_hook;
		ecs::ComponentCache::hooks(pitem).func_add = //
				[](const ecs::World& w, const ecs::ComponentCacheItem& cci, ecs::Entity src) {
					(void)w;
					(void)cci;
					(void)src;
					++hook_trigger_cnt;
				};

		// Components were added already so we don't expect the hook to trigger yet
		wld.add<Position>(e);
		CHECK(hook_trigger_cnt == 0);
		wld.add<Rotation>(e);
		CHECK(hook_trigger_cnt == 0);

		// The component has been removed, and added again. Triggering the hook is expected
		wld.del<Position>(e);
		wld.add<Position>(e);
		CHECK(hook_trigger_cnt == 1);

		// No trigger on Rotation expected because no such trigger has been registered
		wld.del<Rotation>(e);
		wld.add<Rotation>(e);
		CHECK(hook_trigger_cnt == 1);

		// Trigger for new additions
		wld.add<Position>(wld.add());
		wld.add<Position>(wld.add());
		CHECK(hook_trigger_cnt == 3);
	}

	SUBCASE("del") {
		TestWorld twld;
		const auto& pitem = wld.add<Position>();
		hook_trigger_cnt = 0;

		auto e = wld.add();

		// Set up a hook for deleting a Position component
		ecs::ComponentCache::hooks(pitem).func_del = //
				[](const ecs::World& w, const ecs::ComponentCacheItem& cci, ecs::Entity src) {
					(void)w;
					(void)cci;
					(void)src;
					++hook_trigger_cnt;
				};

		#if !GAIA_ASSERT_ENABLED
		// No components were added yet so we don't expect the hook to trigger
		wld.del<Position>(e);
		CHECK(hook_trigger_cnt == 0);
		wld.del<Rotation>(e);
		CHECK(hook_trigger_cnt == 0);
		#endif

		// The component has been added. No triggering yet
		wld.add<Rotation>(e);
		wld.add<Position>(e);
		CHECK(hook_trigger_cnt == 0);

		// Don't trigger for components without a hook
		wld.del<Rotation>(e);
		CHECK(hook_trigger_cnt == 0);

		// Trigger now
		wld.del<Position>(e);
		CHECK(hook_trigger_cnt == 1);

		#if !GAIA_ASSERT_ENABLED
		// Don't trigger again
		wld.del<Position>(e);
		CHECK(hook_trigger_cnt == 1);
		#endif

		// Component added and removed. Trigger again.
		wld.add<Position>(e);
		wld.del<Position>(e);
		CHECK(hook_trigger_cnt == 2);

		#if !GAIA_ASSERT_ENABLED
		// Don't trigger again
		wld.del<Position>(e);
		CHECK(hook_trigger_cnt == 2);
		#endif
	}
	#endif

	#if GAIA_ENABLE_SET_HOOKS
	SUBCASE("set") {
		TestWorld twld;
		const auto& pitem = wld.add<Position>();
		hook_trigger_cnt = 0;

		auto e = wld.add();
		wld.add<Position>(e);
		CHECK(hook_trigger_cnt == 0);
		wld.add<Rotation>(e);
		CHECK(hook_trigger_cnt == 0);

		// Set up a hook for setting a Position component
		ecs::ComponentCache::hooks(pitem).func_set = //
				[](const ecs::World& w, const ecs::ComponentRecord& rec, ecs::Chunk& chunk) {
					(void)w;
					(void)rec;
					(void)chunk;
					++hook_trigger_cnt;
				};

		// Adding shouldn't trigger the set trigger
		{
			hook_trigger_cnt = 0;
			wld.add<Position>(e);
			CHECK(hook_trigger_cnt == 0);
			wld.add<Rotation>(e);
			CHECK(hook_trigger_cnt == 0);

			wld.del<Position>(e);
			wld.add<Position>(e);
			CHECK(hook_trigger_cnt == 0);

			wld.del<Rotation>(e);
			wld.add<Rotation>(e);
			CHECK(hook_trigger_cnt == 0);
		}

		// Don't trigger when setting a different component
		{
			hook_trigger_cnt = 0;
			wld.set<Rotation>(e) = {};
			CHECK(hook_trigger_cnt == 0);
			(void)wld.acc(e).get<Rotation>();
			CHECK(hook_trigger_cnt == 0);
			(void)wld.acc_mut(e).get<Rotation>();
			CHECK(hook_trigger_cnt == 0);
			wld.acc_mut(e).set<Rotation>({});
			CHECK(hook_trigger_cnt == 0);
			wld.acc_mut(e).sset<Rotation>({});
			CHECK(hook_trigger_cnt == 0);
			// Modify + no hooks doesn't trigger
			wld.modify<Rotation, false>(e);
			CHECK(hook_trigger_cnt == 0);
			// Modify + hooks would trigger but we don't have any trigger set for Rotation
			wld.modify<Rotation, true>(e);
			CHECK(hook_trigger_cnt == 0);
		}

		// Trigger for mutable access
		{
			hook_trigger_cnt = 0;
			wld.set<Position>(e) = {};
			CHECK(hook_trigger_cnt == 1);
			(void)wld.acc(e).get<Position>();
			CHECK(hook_trigger_cnt == 1);
			(void)wld.acc_mut(e).get<Position>();
			CHECK(hook_trigger_cnt == 1);
			wld.acc_mut(e).set<Position>({});
			CHECK(hook_trigger_cnt == 2);
			// Silent set doesn't trigger
			wld.acc_mut(e).sset<Position>({});
			CHECK(hook_trigger_cnt == 2);
			// Modify + no hooks doesn't trigger
			wld.modify<Position, false>(e);
			CHECK(hook_trigger_cnt == 2);
			// Modify + hooks does trigger
			wld.modify<Position, true>(e);
			CHECK(hook_trigger_cnt == 3);
		}
	}
	#endif
}

#endif

//------------------------------------------------------------------------------
// Multiple worlds
//------------------------------------------------------------------------------

TEST_CASE("Multiple worlds") {
	ecs::World w1, w2;
	{
		auto e = w1.add();
		w1.add<Position>(e, {1, 1, 1});
		(void)w1.copy(e);
		(void)w1.copy(e);
	}
	{
		auto e = w2.add();
		w2.add<Scale>(e, {2, 0, 0});
		w2.add<Position>(e, {2, 2, 2});
	}

	uint32_t c = 0;
	auto q1 = w1.query().all<Position>();
	q1.each([&c](const Position& p) {
		CHECK(p.x == 1.f);
		CHECK(p.y == 1.f);
		CHECK(p.z == 1.f);
		++c;
	});
	CHECK(c == 3);

	c = 0;
	auto q2 = w2.query().all<Position>();
	q2.each([&c](const Position& p) {
		CHECK(p.x == 2.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 2.f);
		++c;
	});
	CHECK(c == 1);
}

//------------------------------------------------------------------------------
// Serialization
//------------------------------------------------------------------------------

template <typename T>
bool CompareSerializableType(const T& a, const T& b) {
	if constexpr (
			!std::is_trivially_copyable_v<T> || core::has_func_equals<T>::value || core::has_ffunc_equals<T>::value) {
		return a == b;
	} else {
		return !std::memcmp((const void*)&a, (const void*)&b, sizeof(a));
	}
}

template <typename T>
bool CompareSerializableTypes(const T& a, const T& b) {
	if constexpr (std::is_fundamental<T>::value || core::has_func_equals<T>::value || core::has_ffunc_equals<T>::value) {
		return a == b;
	} else {
		// Convert inputs into tuples where each struct member is an element of the tuple
		auto ta = meta::struct_to_tuple(a);
		auto tb = meta::struct_to_tuple(b);

		// Do element-wise comparison
		bool ret = true;
		core::each<std::tuple_size<decltype(ta)>::value>([&](auto i) {
			const auto& aa = std::get<i>(ta);
			const auto& bb = std::get<i>(tb);
			ret = ret && CompareSerializableType(aa, bb);
		});
		return ret;
	}
}

struct FooNonTrivial {
	uint32_t a = 0;

	explicit FooNonTrivial(uint32_t value): a(value) {}
	FooNonTrivial() noexcept = default;
	~FooNonTrivial() = default;
	FooNonTrivial(const FooNonTrivial&) = default;
	FooNonTrivial(FooNonTrivial&&) noexcept = default;
	FooNonTrivial& operator=(const FooNonTrivial&) = default;
	FooNonTrivial& operator=(FooNonTrivial&&) noexcept = default;

	bool operator==(const FooNonTrivial& other) const {
		return a == other.a;
	}
};

struct SerializeStruct1 {
	int a1;
	int a2;
	bool b;
	float c;
};

struct SerializeStruct2 {
	FooNonTrivial d;
	int a1;
	int a2;
	bool b;
	float c;
};

struct CustomStruct {
	char* ptr;
	uint32_t size;
};

bool operator==(const CustomStruct& a, const CustomStruct& b) {
	return a.size == b.size && 0 == memcmp(a.ptr, b.ptr, a.size);
}
namespace gaia::ser {
	template <typename Serializer>
	void tag_invoke(save_tag, Serializer& s, const CustomStruct& data) {
		s.save(data.size);
		s.save((uintptr_t)data.ptr); // expect that the memory is alive somewhere
	}

	template <typename Serializer>
	void tag_invoke(load_tag, Serializer& s, CustomStruct& data) {
		s.load(data.size);
		uintptr_t mem;
		s.load(mem);
		data.ptr = (char*)mem;
	}
} // namespace gaia::ser

struct CustomStructInternal {
	char* ptr;
	uint32_t size;

	bool operator==(const CustomStructInternal& other) const {
		return size == other.size && 0 == memcmp(ptr, other.ptr, other.size);
	}

	template <typename Serializer>
	void save(Serializer& s) const {
		s.save(size);
		s.save((uintptr_t)ptr); // expect that the memory is alive somewhere
	}

	template <typename Serializer>
	void load(Serializer& s) {
		s.load(size);
		uintptr_t mem;
		s.load(mem);
		ptr = (char*)mem;
	}
};

TEST_CASE("Serialization - custom") {
	SUBCASE("external") {
		CustomStruct in, out;
		in.ptr = new char[5];
		in.ptr[0] = 'g';
		in.ptr[1] = 'a';
		in.ptr[2] = 'i';
		in.ptr[3] = 'a';
		in.ptr[4] = '\0';
		in.size = 5;

		ser::ser_buffer_binary s;

		static_assert(!ser::has_func_save<CustomStruct, ser::ser_buffer_binary&>::value);
		static_assert(ser::has_tag_save<ser::ser_buffer_binary, CustomStruct>::value);

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
		delete[] in.ptr;
	}
	SUBCASE("internal") {
		CustomStructInternal in, out;
		in.ptr = new char[5];
		in.ptr[0] = 'g';
		in.ptr[1] = 'a';
		in.ptr[2] = 'i';
		in.ptr[3] = 'a';
		in.ptr[4] = '\0';
		in.size = 5;

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
		delete[] in.ptr;
	}
}

TEST_CASE("Serialization - bytes") {
	{
		SerializeStruct2 in{FooNonTrivial(111), 1, 2, true, 3.12345f};

		ser::ser_buffer_binary s;
		ser::save(s, in);

		const auto expectedBytes = ser::bytes(in);
		CHECK(expectedBytes == s.bytes());
	}
	{
		cnt::darr<uint32_t> in{};
		in.resize(10);
		GAIA_EACH(in) in[i] = i;

		ser::ser_buffer_binary s;
		ser::save(s, in);

		const auto expectedBytes = ser::bytes(in);
		CHECK(expectedBytes == s.bytes());
	}
	{
		CustomStructInternal in{};
		char data[] = {'g', 'a', 'i', 'a', '\0'};
		in.ptr = data;
		in.size = 5;

		ser::ser_buffer_binary s;
		ser::save(s, in);

		const auto expectedBytes = ser::bytes(in);
		CHECK(expectedBytes == s.bytes());
	}
}

TEST_CASE("Serialization - ser_json") {
	SUBCASE("supports nested objects and arrays") {
		ser::ser_json writer;
		writer.begin_object();
		writer.key("meta");
		writer.begin_object();
		writer.key("name");
		writer.value_string("gaia");
		writer.key("version");
		writer.value_int(1);
		writer.key("tags");
		writer.begin_array();
		writer.value_string("ecs");
		writer.value_string("runtime");
		writer.end_array();
		writer.end_object();
		writer.key("values");
		writer.begin_array();
		writer.value_int(3);
		writer.value_int(5);
		writer.value_int(8);
		writer.end_array();
		writer.end_object();

		CHECK(
				writer.str() ==
				"{\"meta\":{\"name\":\"gaia\",\"version\":1,\"tags\":[\"ecs\",\"runtime\"]},\"values\":[3,5,8]}");
	}

	SUBCASE("escapes string control characters") {
		ser::ser_json writer;
		writer.begin_object();
		writer.key("text");
		writer.value_string("a\"b\\c\n\r\t");
		writer.end_object();

		CHECK(writer.str() == "{\"text\":\"a\\\"b\\\\c\\n\\r\\t\"}");
	}

	SUBCASE("parses values and skips nested payloads") {
		ser::ser_json reader(
				"{\"name\":\"gaia\",\"payload\":[1,{\"x\":true}],\"enabled\":false,\"count\":12.5,\"nothing\":null}");
		ser::json_str key;
		ser::json_str text;
		double number = 0.0;
		bool b = true;

		CHECK(reader.expect('{'));
		CHECK(reader.parse_string(key));
		CHECK(key == "name");
		CHECK(reader.expect(':'));
		CHECK(reader.parse_string(text));
		CHECK(text == "gaia");
		CHECK(reader.consume(','));

		CHECK(reader.parse_string(key));
		CHECK(key == "payload");
		CHECK(reader.expect(':'));
		CHECK(reader.skip_value());
		CHECK(reader.consume(','));

		CHECK(reader.parse_string(key));
		CHECK(key == "enabled");
		CHECK(reader.expect(':'));
		CHECK(reader.parse_bool(b));
		CHECK(b == false);
		CHECK(reader.consume(','));

		CHECK(reader.parse_string(key));
		CHECK(key == "count");
		CHECK(reader.expect(':'));
		CHECK(reader.parse_number(number));
		CHECK(number == 12.5);
		CHECK(reader.consume(','));

		CHECK(reader.parse_string(key));
		CHECK(key == "nothing");
		CHECK(reader.expect(':'));
		CHECK(reader.parse_null());
		CHECK(reader.expect('}'));
		reader.ws();
		CHECK(reader.eof());
	}
}

TEST_CASE("Serialization - json runtime schema") {
	struct JsonRuntimeComp {
		int32_t a;
		float b;
		bool c;
		char name[8];
	};

	SUBCASE("schema fields are emitted as json object") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json", 0, (uint32_t)sizeof(JsonRuntimeComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRuntimeComp));
		auto& item = cc.get(entity);

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		const auto offB = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.b) - pBase);
		const auto offC = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.c) - pBase);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);

		CHECK(item.set_field("a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));
		CHECK(item.set_field("b", 0, ser::serialization_type_id::f32, offB, (uint32_t)sizeof(layout.b)));
		CHECK(item.set_field("c", 0, ser::serialization_type_id::b, offC, (uint32_t)sizeof(layout.c)));
		CHECK(item.set_field("name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));

		JsonRuntimeComp value{};
		value.a = 42;
		value.b = 1.5f;
		value.c = true;
		GAIA_STRCPY(value.name, sizeof(value.name), "gaia");

		ser::ser_json writer;
		const bool ok = ecs::component_to_json(item, &value, writer);
		CHECK(ok);
		CHECK(writer.str() == "{\"a\":42,\"b\":1.5,\"c\":true,\"name\":\"gaia\"}");

		bool okStr = false;
		const auto json = ecs::component_to_json(item, &value, okStr);
		CHECK(okStr);
		CHECK(json == writer.str());
	}

	SUBCASE("nested struct fields are emitted using schema names and offsets") {
		struct Vec3 {
			float x, y, z;
		};
		struct TransformLike {
			int32_t id;
			Vec3 position;
			Vec3 velocity;
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Nested", 0, (uint32_t)sizeof(TransformLike), ecs::DataStorageType::Table,
				(uint32_t)alignof(TransformLike));
		auto& item = cc.get(entity);

		TransformLike layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offId = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.id) - pBase);
		const auto offPosX = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.position.x) - pBase);
		const auto offPosY = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.position.y) - pBase);
		const auto offPosZ = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.position.z) - pBase);
		const auto offVelX = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.velocity.x) - pBase);
		const auto offVelY = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.velocity.y) - pBase);
		const auto offVelZ = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.velocity.z) - pBase);

		CHECK(item.set_field("id", 0, ser::serialization_type_id::s32, offId, (uint32_t)sizeof(layout.id)));
		CHECK(
				item.set_field("position.x", 0, ser::serialization_type_id::f32, offPosX, (uint32_t)sizeof(layout.position.x)));
		CHECK(
				item.set_field("position.y", 0, ser::serialization_type_id::f32, offPosY, (uint32_t)sizeof(layout.position.y)));
		CHECK(
				item.set_field("position.z", 0, ser::serialization_type_id::f32, offPosZ, (uint32_t)sizeof(layout.position.z)));
		CHECK(
				item.set_field("velocity.x", 0, ser::serialization_type_id::f32, offVelX, (uint32_t)sizeof(layout.velocity.x)));
		CHECK(
				item.set_field("velocity.y", 0, ser::serialization_type_id::f32, offVelY, (uint32_t)sizeof(layout.velocity.y)));
		CHECK(
				item.set_field("velocity.z", 0, ser::serialization_type_id::f32, offVelZ, (uint32_t)sizeof(layout.velocity.z)));

		TransformLike value{};
		value.id = 7;
		value.position = {1.25f, -2.5f, 0.0f};
		value.velocity = {10.0f, 11.5f, -12.0f};

		ser::ser_json writer;
		const bool ok = ecs::component_to_json(item, &value, writer);
		CHECK(ok);
		CHECK(
				writer.str() == "{\"id\":7,\"position.x\":1.25,\"position.y\":-2.5,\"position.z\":0,\"velocity.x\":10,"
												"\"velocity.y\":11.5,\"velocity.z\":-12}");
	}

	SUBCASE("array and array-of-struct fields are emitted using schema names and offsets") {
		struct InventoryEntry {
			uint16_t id;
			uint8_t count;
		};
		struct RuntimeArraysComp {
			float weights[3];
			InventoryEntry inventory[2];
			bool active[2];
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Arrays", 0, (uint32_t)sizeof(RuntimeArraysComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(RuntimeArraysComp));
		auto& item = cc.get(entity);

		RuntimeArraysComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offW0 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.weights[0]) - pBase);
		const auto offW1 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.weights[1]) - pBase);
		const auto offW2 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.weights[2]) - pBase);
		const auto offInv0Id = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.inventory[0].id) - pBase);
		const auto offInv0Count = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.inventory[0].count) - pBase);
		const auto offInv1Id = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.inventory[1].id) - pBase);
		const auto offInv1Count = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.inventory[1].count) - pBase);
		const auto offActive0 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.active[0]) - pBase);
		const auto offActive1 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.active[1]) - pBase);

		CHECK(item.set_field("weights[0]", 0, ser::serialization_type_id::f32, offW0, (uint32_t)sizeof(layout.weights[0])));
		CHECK(item.set_field("weights[1]", 0, ser::serialization_type_id::f32, offW1, (uint32_t)sizeof(layout.weights[1])));
		CHECK(item.set_field("weights[2]", 0, ser::serialization_type_id::f32, offW2, (uint32_t)sizeof(layout.weights[2])));
		CHECK(item.set_field(
				"inventory[0].id", 0, ser::serialization_type_id::u16, offInv0Id, (uint32_t)sizeof(layout.inventory[0].id)));
		CHECK(item.set_field(
				"inventory[0].count", 0, ser::serialization_type_id::u8, offInv0Count,
				(uint32_t)sizeof(layout.inventory[0].count)));
		CHECK(item.set_field(
				"inventory[1].id", 0, ser::serialization_type_id::u16, offInv1Id, (uint32_t)sizeof(layout.inventory[1].id)));
		CHECK(item.set_field(
				"inventory[1].count", 0, ser::serialization_type_id::u8, offInv1Count,
				(uint32_t)sizeof(layout.inventory[1].count)));
		CHECK(
				item.set_field("active[0]", 0, ser::serialization_type_id::b, offActive0, (uint32_t)sizeof(layout.active[0])));
		CHECK(
				item.set_field("active[1]", 0, ser::serialization_type_id::b, offActive1, (uint32_t)sizeof(layout.active[1])));

		RuntimeArraysComp value{};
		value.weights[0] = 0.25f;
		value.weights[1] = 0.5f;
		value.weights[2] = 1.0f;
		value.inventory[0].id = 101;
		value.inventory[0].count = 3;
		value.inventory[1].id = 202;
		value.inventory[1].count = 9;
		value.active[0] = true;
		value.active[1] = false;

		ser::ser_json writer;
		const bool ok = ecs::component_to_json(item, &value, writer);
		CHECK(ok);
		CHECK(
				writer.str() ==
				"{\"weights[0]\":0.25,\"weights[1]\":0.5,\"weights[2]\":1,\"inventory[0].id\":101,\"inventory[0].count\":3,"
				"\"inventory[1].id\":202,\"inventory[1].count\":9,\"active[0]\":true,\"active[1]\":false}");
	}

	SUBCASE("unsupported field types are emitted as null and reported") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(entity, "Runtime_Component_Json_Unsupported", 0, 4, ecs::DataStorageType::Table, 4);
		auto& item = cc.get(entity);

		CHECK(item.set_field("blob", 0, ser::serialization_type_id::trivial_wrapper, 0, 4));
		uint32_t value = 123;

		ser::ser_json writer;
		const bool ok = ecs::component_to_json(item, &value, writer);
		CHECK_FALSE(ok);
		CHECK(writer.str() == "{\"blob\":null}");
	}

	SUBCASE("out of bounds schema fields are emitted as null and reported") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(entity, "Runtime_Component_Json_Oob", 0, 4, ecs::DataStorageType::Table, 4);
		auto& item = cc.get(entity);

		CHECK(item.set_field("too_far", 0, ser::serialization_type_id::u32, 8, 4));
		uint32_t value = 123;

		ser::ser_json writer;
		const bool ok = ecs::component_to_json(item, &value, writer);
		CHECK_FALSE(ok);
		CHECK(writer.str() == "{\"too_far\":null}");
	}

	SUBCASE("json_to_component loads schema payload") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Read", 0, (uint32_t)sizeof(JsonRuntimeComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRuntimeComp));
		auto& item = cc.get(entity);

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		const auto offB = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.b) - pBase);
		const auto offC = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.c) - pBase);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);
		CHECK(item.set_field("a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));
		CHECK(item.set_field("b", 0, ser::serialization_type_id::f32, offB, (uint32_t)sizeof(layout.b)));
		CHECK(item.set_field("c", 0, ser::serialization_type_id::b, offC, (uint32_t)sizeof(layout.c)));
		CHECK(item.set_field("name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));

		JsonRuntimeComp in{};
		in.a = -123;
		in.b = 9.25f;
		in.c = true;
		GAIA_STRCPY(in.name, sizeof(in.name), "reader");

		bool okWrite = false;
		const auto json = ecs::component_to_json(item, &in, okWrite);
		CHECK(okWrite);

		JsonRuntimeComp out{};
		ser::ser_json reader(json.data(), (uint32_t)json.size());
		bool okRead = true;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK(okRead);
		reader.ws();
		CHECK(reader.eof());

		CHECK(out.a == in.a);
		CHECK(out.b == in.b);
		CHECK(out.c == in.c);
		CHECK(strcmp(out.name, in.name) == 0);
	}

	SUBCASE("json_to_component loads raw payload") {
		struct JsonRawComp {
			uint32_t a;
			float b;
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Raw_Read", 0, (uint32_t)sizeof(JsonRawComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRawComp));
		auto& item = cc.get(entity);
		item.clear_fields();

		JsonRawComp in{77u, 1.75f};

		ser::ser_buffer_binary raw;
		{
			auto s = ser::make_serializer(raw);
			item.save(s, &in, 0, 1, 1);
		}

		ser::ser_json writer;
		writer.begin_object();
		writer.key("$raw");
		writer.begin_array();
		{
			const auto* pData = raw.data();
			GAIA_FOR(raw.bytes()) writer.value_int(pData[i]);
		}
		writer.end_array();
		writer.end_object();

		JsonRawComp out{};
		ser::ser_json reader(writer.str().data(), (uint32_t)writer.str().size());
		bool okRead = true;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK(okRead);
		reader.ws();
		CHECK(reader.eof());

		CHECK(out.a == in.a);
		CHECK(out.b == in.b);
	}

	SUBCASE("json_to_component marks unknown fields as best-effort failures but keeps parsing") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Unknown_Fields", 0, (uint32_t)sizeof(JsonRuntimeComp),
				ecs::DataStorageType::Table, (uint32_t)alignof(JsonRuntimeComp));
		auto& item = cc.get(entity);

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		const auto offB = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.b) - pBase);
		const auto offC = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.c) - pBase);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);
		CHECK(item.set_field("a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));
		CHECK(item.set_field("b", 0, ser::serialization_type_id::f32, offB, (uint32_t)sizeof(layout.b)));
		CHECK(item.set_field("c", 0, ser::serialization_type_id::b, offC, (uint32_t)sizeof(layout.c)));
		CHECK(item.set_field("name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));

		JsonRuntimeComp out{};
		ser::ser_json reader("{\"a\":11,\"unknown\":{\"nested\":[1,true,\"x\"]},\"b\":2.25,\"c\":false,\"name\":\"ok\"}");
		bool okRead = true;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK_FALSE(okRead);
		reader.ws();
		CHECK(reader.eof());

		CHECK(out.a == 11);
		CHECK(out.b == 2.25f);
		CHECK(out.c == false);
		CHECK(strcmp(out.name, "ok") == 0);
	}

	SUBCASE("json_to_component fails on malformed schema field payload type") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Bad_Type", 0, (uint32_t)sizeof(JsonRuntimeComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRuntimeComp));
		auto& item = cc.get(entity);

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		CHECK(item.set_field("a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));

		JsonRuntimeComp out{};
		ser::ser_json reader("{\"a\":\"bad\"}");
		bool okRead = true;
		CHECK_FALSE(ecs::json_to_component(item, &out, reader, okRead));
	}

	SUBCASE("json_to_component treats null field values as best-effort failures") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Null_Field", 0, (uint32_t)sizeof(JsonRuntimeComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRuntimeComp));
		auto& item = cc.get(entity);

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		CHECK(item.set_field("a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));

		JsonRuntimeComp out{};
		out.a = 123;
		ser::ser_json reader("{\"a\":null}");
		bool okRead = true;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK_FALSE(okRead);
		CHECK(out.a == 123);
	}

	SUBCASE("json_to_component clamps out-of-range integers and marks best-effort failure") {
		struct JsonIntsComp {
			uint8_t u8;
			uint16_t u16;
			int8_t s8;
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Ints", 0, (uint32_t)sizeof(JsonIntsComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonIntsComp));
		auto& item = cc.get(entity);

		JsonIntsComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offU8 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.u8) - pBase);
		const auto offU16 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.u16) - pBase);
		const auto offS8 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.s8) - pBase);
		CHECK(item.set_field("u8", 0, ser::serialization_type_id::u8, offU8, (uint32_t)sizeof(layout.u8)));
		CHECK(item.set_field("u16", 0, ser::serialization_type_id::u16, offU16, (uint32_t)sizeof(layout.u16)));
		CHECK(item.set_field("s8", 0, ser::serialization_type_id::s8, offS8, (uint32_t)sizeof(layout.s8)));

		JsonIntsComp out{};
		ser::ser_json reader("{\"u8\":-3,\"u16\":70000,\"s8\":300.9}");
		bool okRead = true;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK_FALSE(okRead);
		CHECK(out.u8 == (uint8_t)0);
		CHECK(out.u16 == (uint16_t)65535);
		CHECK(out.s8 == (int8_t)127);
	}

	SUBCASE("json_to_component reports string truncation as best-effort failure") {
		struct JsonNameComp {
			char name[4];
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_String_Truncate", 0, (uint32_t)sizeof(JsonNameComp),
				ecs::DataStorageType::Table, (uint32_t)alignof(JsonNameComp));
		auto& item = cc.get(entity);

		JsonNameComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);
		CHECK(item.set_field("name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));

		JsonNameComp out{};
		ser::ser_json reader("{\"name\":\"abcdef\"}");
		bool okRead = true;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK_FALSE(okRead);
		CHECK(strcmp(out.name, "abc") == 0);
	}

	SUBCASE("json_to_component fails on malformed raw byte arrays") {
		struct JsonRawComp {
			uint32_t a;
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Raw_Malformed", 0, (uint32_t)sizeof(JsonRawComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRawComp));
		auto& item = cc.get(entity);
		item.clear_fields();

		{
			JsonRawComp out{};
			ser::ser_json reader("{\"$raw\":[1,2.5]}");
			bool okRead = true;
			CHECK_FALSE(ecs::json_to_component(item, &out, reader, okRead));
		}

		{
			JsonRawComp out{};
			ser::ser_json reader("{\"$raw\":[1,256]}");
			bool okRead = true;
			CHECK_FALSE(ecs::json_to_component(item, &out, reader, okRead));
		}
	}

	SUBCASE("json_to_component marks payloads without schema or raw data as best-effort failures") {
		struct JsonRawComp {
			uint32_t a;
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_No_Data", 0, (uint32_t)sizeof(JsonRawComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRawComp));
		auto& item = cc.get(entity);
		item.clear_fields();

		JsonRawComp out{};
		ser::ser_json reader("{\"foo\":1}");
		bool okRead = true;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK_FALSE(okRead);
	}

	SUBCASE("json_to_component reports structured diagnostics") {
		struct JsonDiagComp {
			uint8_t value;
			char name[4];
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)cc.add(
				entity, "Runtime_Component_Json_Diagnostics", 0, (uint32_t)sizeof(JsonDiagComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonDiagComp));
		auto& item = cc.get(entity);

		JsonDiagComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offValue = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.value) - pBase);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);
		CHECK(item.set_field("value", 0, ser::serialization_type_id::u8, offValue, (uint32_t)sizeof(layout.value)));
		CHECK(item.set_field("name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));

		JsonDiagComp out{};
		ser::ser_json reader("{\"value\":-2,\"name\":\"abcdef\",\"unknown\":1}");
		ser::JsonDiagnostics diagnostics;
		CHECK(ecs::json_to_component(item, &out, reader, diagnostics, "Runtime_Component_Json_Diagnostics"));
		CHECK(diagnostics.hasWarnings);

		bool hasUnknownField = false;
		bool hasAdjustedValue = false;
		bool hasAdjustedName = false;
		for (const auto& diag: diagnostics.items) {
			if (diag.reason == ser::JsonDiagReason::UnknownField && diag.path == "Runtime_Component_Json_Diagnostics.unknown")
				hasUnknownField = true;
			if (diag.reason == ser::JsonDiagReason::FieldValueAdjusted &&
					diag.path == "Runtime_Component_Json_Diagnostics.value")
				hasAdjustedValue = true;
			if (diag.reason == ser::JsonDiagReason::FieldValueAdjusted &&
					diag.path == "Runtime_Component_Json_Diagnostics.name")
				hasAdjustedName = true;
		}
		CHECK(hasUnknownField);
		CHECK(hasAdjustedValue);
		CHECK(hasAdjustedName);
	}
}

TEST_CASE("Serialization - runtime context init") {
	ser::bin_stream stream;

	const auto ctx = ser::serializer::bind_ctx(stream);
	ser::serializer serializer{ctx};
	CHECK(serializer.valid());

	uint32_t in = 0xDEADBEEF;
	uint32_t out = 0;
	serializer.save(in);
	serializer.seek(0);
	serializer.load(out);
	CHECK(in == out);
}

TEST_CASE("Serialization - simple") {
	{
		Int3 in{1, 2, 3}, out{};

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		Position in{1, 2, 3}, out{};

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		PositionSoA in{1, 2, 3}, out{};

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStruct1 in{1, 2, true, 3.12345f}, out{};

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStruct2 in{FooNonTrivial(111), 1, 2, true, 3.12345f}, out{};

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}
}

struct SerializeStructSArray {
	cnt::sarray<uint32_t, 8> arr;
	float f;
};

GAIA_MSVC_WARNING_PUSH()
GAIA_MSVC_WARNING_DISABLE(4324)

struct SerializeStructSArray_PositionSoA {
	cnt::sarray_soa<PositionSoA, 8> arr;
	float f;

	bool operator==(const SerializeStructSArray_PositionSoA& other) const {
		return f == other.f && arr == other.arr;
	}
};

GAIA_MSVC_WARNING_POP()

struct SerializeStructSArrayNonTrivial {
	cnt::sarray<FooNonTrivial, 8> arr;
	float f;

	bool operator==(const SerializeStructSArrayNonTrivial& other) const {
		return f == other.f && arr == other.arr;
	}
};

GAIA_MSVC_WARNING_PUSH()
GAIA_MSVC_WARNING_DISABLE(4324)

struct SerializeStructDArray_PositionSoA {
	cnt::darr_soa<PositionSoA> arr;
	float f;

	bool operator==(const SerializeStructDArray_PositionSoA& other) const {
		return f == other.f && arr == other.arr;
	}
};

struct SerializeStructDArray_PositionSoA_AOS {
	cnt::darr<PositionSoA> arr;
	float f;

	bool operator==(const SerializeStructDArray_PositionSoA_AOS& other) const {
		return f == other.f && arr == other.arr;
	}
};

GAIA_MSVC_WARNING_POP()

struct SerializeStructDArray {
	cnt::darr<uint32_t> arr;
	float f;
};

struct SerializeStructDArrayNonTrivial {
	cnt::darr<uint32_t> arr;
	float f;

	bool operator==(const SerializeStructDArrayNonTrivial& other) const {
		return f == other.f && arr == other.arr;
	}
};

struct SerializeCustomStructInternalDArray {
	cnt::darr<CustomStructInternal> arr;
	float f;

	bool operator==(const SerializeCustomStructInternalDArray& other) const {
		return f == other.f && arr == other.arr;
	}
};

TEST_CASE("Serialization - arrays") {
	{
		SerializeStructSArray in{}, out{};
		GAIA_EACH(in.arr) in.arr[i] = i;
		in.f = 3.12345f;

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStructSArray_PositionSoA in{}, out{};
		GAIA_EACH(in.arr) {
			in.arr.view_mut<0>()[i] = (float)i;
			in.arr.view_mut<1>()[i] = (float)i;
			in.arr.view_mut<2>()[i] = (float)i;
		}
		in.f = 3.12345f;

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStructSArrayNonTrivial in{}, out{};
		GAIA_EACH(in.arr) in.arr[i] = FooNonTrivial(i);
		in.f = 3.12345f;

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStructDArray in{}, out{};
		in.arr.resize(10);
		GAIA_EACH(in.arr) in.arr[i] = i;
		in.f = 3.12345f;

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStructDArray_PositionSoA_AOS in{}, out{};
		GAIA_FOR(10) {
			in.arr.push_back({(float)i, (float)i, (float)i});
		}
		in.f = 3.12345f;

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeStructDArray_PositionSoA in{}, out{};
		GAIA_FOR(10) {
			in.arr.push_back({(float)i, (float)i, (float)i});
		}
		in.f = 3.12345f;

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));
	}

	{
		SerializeCustomStructInternalDArray in{}, out{};
		in.arr.resize(10);
		for (auto& a: in.arr) {
			a.ptr = new char[5];
			a.ptr[0] = 'g';
			a.ptr[1] = 'a';
			a.ptr[2] = 'i';
			a.ptr[3] = 'a';
			a.ptr[4] = '\0';
			a.size = 5;
		}
		in.f = 3.12345f;

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(CompareSerializableTypes(in, out));

		for (auto& a: in.arr)
			delete[] a.ptr;
	}
}

TEST_CASE("Serialization - hashmap") {
	{
		gaia::cnt::map<int, CustomStruct> in{}, out{};
		GAIA_FOR(5) {
			CustomStruct str;
			str.ptr = new char[6];
			str.ptr[0] = 'g';
			str.ptr[1] = 'a';
			str.ptr[2] = 'i';
			str.ptr[3] = 'a';
			str.ptr[4] = '0' + (char)i;
			str.ptr[5] = '\0';
			str.size = 6;
			in.insert({i, str});
		}

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(in.size() == out.size());
		if (in.size() == out.size()) {
			auto it0 = in.begin();
			auto it1 = out.begin();
			while (it0 != in.end()) {
				CHECK(CompareSerializableTypes(it0->first, it1->first));
				CHECK(CompareSerializableTypes(it0->second, it1->second));
				++it0;
				++it1;
			}
		}

		for (auto& it: in)
			delete[] it.second.ptr;
	}
}

TEST_CASE("Serialization - hashset") {
	{
		gaia::cnt::set<CustomStruct> in{}, out{};
		GAIA_FOR(5) {
			CustomStruct str;
			str.ptr = new char[6];
			str.ptr[0] = 'g';
			str.ptr[1] = 'a';
			str.ptr[2] = 'i';
			str.ptr[3] = 'a';
			str.ptr[4] = '0' + (char)i;
			str.ptr[5] = '\0';
			str.size = 6;
			in.insert({str});
		}

		ser::ser_buffer_binary s;

		ser::save(s, in);
		s.seek(0);
		ser::load(s, out);

		CHECK(in.size() == out.size());
		// TODO: Hashset does not necessasrily return iterated items in the same order
		//.      in which we inserted them. Sort before comparing.
		// if (in.size() == out.size()) {
		// 	auto it0 = in.begin();
		// 	auto it1 = out.begin();
		// 	while (it0 != in.end()) {
		// 		CHECK(CompareSerializableTypes(*it0, *it1));
		// 		++it0;
		// 		++it1;
		// 	}
		// }

		for (auto& it: in)
			delete[] it.ptr;
	}
}

static uint32_t find_serialized_entity_archetype_idx(ecs::World& world, ecs::Entity entity) {
	ser::bin_stream buffer;
	world.set_serializer(buffer);
	world.save();

	auto s = ser::make_serializer(buffer);
	s.seek(0);

	uint32_t version = 0;
	uint32_t lastCoreComponentId = 0;
	uint32_t newEntities = 0;
	s.load(version);
	s.load(lastCoreComponentId);
	s.load(newEntities);
	(void)version;
	(void)lastCoreComponentId;

	GAIA_FOR(newEntities) {
		bool isAlive = false;
		s.load(isAlive);
		if (isAlive) {
			uint32_t idx = 0;
			uint32_t dataRaw = 0;
			uint16_t row = 0;
			uint16_t flags = 0;
			uint32_t refCnt = 0;
			uint32_t archetypeIdx = 0;
			uint32_t chunkIdx = 0;
			s.load(idx);
			s.load(dataRaw);
			s.load(row);
			s.load(flags);
			s.load(refCnt);
			s.load(archetypeIdx);
			s.load(chunkIdx);
			(void)dataRaw;
			(void)row;
			(void)flags;
			(void)refCnt;
			(void)chunkIdx;

			if (idx == entity.id())
				return archetypeIdx;
		} else {
			ecs::Identifier id = ecs::IdentifierBad;
			uint32_t nextFreeIdx = ecs::Entity::IdMask;
			s.load(id);
			s.load(nextFreeIdx);
			(void)id;
			(void)nextFreeIdx;
		}
	}

	return BadIndex;
}

static ser::bin_stream
make_legacy_named_entity_snapshot(uint32_t oldLastCoreComponentId, uint32_t archetypeIdx, const char* name) {
	ser::bin_stream buffer;
	auto s = ser::make_serializer(buffer);

	const auto legacyEntity =
			ecs::Entity((ecs::EntityId)(oldLastCoreComponentId + 1), 0, true, false, ecs::EntityKind::EK_Gen);
	ecs::EntityContainerCtx ctx{true, false, ecs::EntityKind::EK_Gen};
	auto ec = ecs::EntityContainer::create(legacyEntity.id(), legacyEntity.gen(), &ctx);
	ec.row = 0;
	ec.flags = 0;
	ec.refCnt = 1;

	s.save((uint32_t)2); // WorldSerializerVersion
	s.save(oldLastCoreComponentId);

	s.save((uint32_t)1); // newEntities
	s.save(true);
	s.save(ec.idx);
	s.save(ec.dataRaw);
	s.save(ec.row);
	s.save(ec.flags);
	s.save(ec.refCnt);
	s.save(archetypeIdx);
	s.save((uint32_t)0); // chunkIdx

	s.save((uint32_t)0); // pairsCnt
	s.save((uint32_t)ecs::Entity::IdMask);
	s.save((uint32_t)0); // freeItems

	s.save((uint32_t)1); // archetypesSize
	s.save((uint32_t)1); // idsSize
	s.save(ecs::GAIA_ID(EntityDesc));
	s.save((uint32_t)0); // firstFreeChunkIdx
	s.save(archetypeIdx);
	s.save((uint32_t)1); // chunkCnt
	s.save((uint32_t)0); // chunkIdx
	s.save((uint16_t)1); // count
	s.save((uint16_t)1); // countEnabled
	s.save((uint16_t)0); // dead
	s.save((uint16_t)0); // lifespanCountdown
	s.save(legacyEntity);
	s.save(ecs::EntityDesc{});
	s.save((uint32_t)0); // worldVersion

	s.save((uint32_t)1); // namesCnt
	s.save(legacyEntity);
	s.save(true); // isOwned
	const auto len = (uint32_t)strlen(name);
	s.save(len);
	s.save_raw(name, len, ser::serialization_type_id::c8);

	s.save((uint32_t)0); // aliasCnt

	return buffer;
}

static std::string make_binary_snapshot_json(const ser::bin_stream& buffer) {
	std::string json = "{\"format\":1,\"binary\":[";
	const auto* pData = (const uint8_t*)buffer.data();
	GAIA_FOR(buffer.bytes()) {
		if (i != 0)
			json += ',';
		json += std::to_string((uint32_t)pData[i]);
	}
	json += "]}";
	return json;
}

TEST_CASE("Serialization - world self") {
	auto initComponents = [](ecs::World& w) {
		(void)w.add<Position>();
		(void)w.add<PositionSoA>();
	};

	ecs::World in;
	initComponents(in);

	ecs::Entity eats = in.add();
	ecs::Entity carrot = in.add();
	in.del(carrot);
	in.update();
	carrot = in.add();
	ecs::Entity salad = in.add();
	ecs::Entity apple = in.add();

	in.add<Position>(eats, {1, 2, 3});
	in.add<PositionSoA>(eats, {10, 20, 30});
	in.name(eats, "Eats");
	in.name(carrot, "Carrot");
	in.name(salad, "Salad");
	in.name(apple, "Apple");

	in.save();

	//--------
	in.cleanup();
	initComponents(in);
	CHECK(in.load());

	Position pos = in.get<Position>(eats);
	CHECK(pos.x == 1.f);
	CHECK(pos.y == 2.f);
	CHECK(pos.z == 3.f);

	PositionSoA pos_soa = in.get<PositionSoA>(eats);
	CHECK(pos_soa.x == 10.f);
	CHECK(pos_soa.y == 20.f);
	CHECK(pos_soa.z == 30.f);

	auto eats2 = in.get("Eats");
	auto carrot2 = in.get("Carrot");
	auto salad2 = in.get("Salad");
	auto apple2 = in.get("Apple");
	CHECK(eats2 == eats);
	CHECK(carrot2 == carrot);
	CHECK(salad2 == salad);
	CHECK(apple2 == apple);
}

TEST_CASE("Serialization - world other") {
	ecs::World in;
	auto initComponents = [](ecs::World& w) {
		(void)w.add<Position>();
		(void)w.add<PositionSoA>();
	};

	initComponents(in);

	ecs::Entity eats = in.add();
	ecs::Entity carrot = in.add();
	ecs::Entity salad = in.add();
	ecs::Entity apple = in.add();

	in.add<Position>(eats, {1, 2, 3});
	in.add<PositionSoA>(eats, {10, 20, 30});
	in.name(eats, "Eats");
	in.name(carrot, "Carrot");
	in.name(salad, "Salad");
	in.name(apple, "Apple");

	ser::bin_stream buffer;
	in.set_serializer(buffer);
	in.save();

	TestWorld twld;
	initComponents(wld);
	CHECK(wld.load(buffer));

	Position pos = wld.get<Position>(eats);
	CHECK(pos.x == 1.f);
	CHECK(pos.y == 2.f);
	CHECK(pos.z == 3.f);

	PositionSoA pos_soa = in.get<PositionSoA>(eats);
	CHECK(pos_soa.x == 10.f);
	CHECK(pos_soa.y == 20.f);
	CHECK(pos_soa.z == 30.f);

	auto eats2 = wld.get("Eats");
	auto carrot2 = wld.get("Carrot");
	auto salad2 = wld.get("Salad");
	auto apple2 = wld.get("Apple");
	CHECK(eats2 == eats);
	CHECK(carrot2 == carrot);
	CHECK(salad2 == salad);
	CHECK(apple2 == apple);
}

TEST_CASE("Serialization - world compatibility when core components are added later") {
	TestWorld archetypeWorld;
	const auto warmup = archetypeWorld.m_w.add();
	archetypeWorld.m_w.name(warmup, "Warmup");

	const auto archetypeIdx = find_serialized_entity_archetype_idx(archetypeWorld.m_w, warmup);
	CHECK(archetypeIdx != BadIndex);
	if (archetypeIdx == BadIndex)
		return;

	const auto currLastCoreComponentId = ecs::GAIA_ID(LastCoreComponent).id();
	CHECK(currLastCoreComponentId > 0);
	if (currLastCoreComponentId == 0)
		return;
	auto legacyBuffer = make_legacy_named_entity_snapshot(currLastCoreComponentId - 1, archetypeIdx, "Legacy");

	TestWorld twldOut;
	CHECK(twldOut.m_w.load(legacyBuffer));

	const auto legacy = twldOut.m_w.get("Legacy");
	CHECK(legacy != ecs::EntityBad);
	CHECK(legacy.id() == currLastCoreComponentId + 1);

	const auto next = twldOut.m_w.add();
	CHECK(next.id() == legacy.id() + 1);
}

TEST_CASE("Serialization - world json") {
	TestWorld twld;
	(void)wld.add<Position>();
	(void)wld.add<PositionSoA>();

	const auto positionEntity = wld.comp_cache().get<Position>().entity;
	auto& posItem = wld.comp_cache_mut().get(positionEntity);
	posItem.clear_fields();

	Position layout{};
	const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
	const auto offX = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.x) - pBase);
	const auto offY = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.y) - pBase);
	const auto offZ = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.z) - pBase);
	CHECK(posItem.set_field("x", 0, ser::serialization_type_id::f32, offX, (uint32_t)sizeof(layout.x)));
	CHECK(posItem.set_field("y", 0, ser::serialization_type_id::f32, offY, (uint32_t)sizeof(layout.y)));
	CHECK(posItem.set_field("z", 0, ser::serialization_type_id::f32, offZ, (uint32_t)sizeof(layout.z)));

	const auto e = wld.add();
	wld.add<Position>(e, {1.0f, 2.0f, 3.0f});
	wld.add<PositionSoA>(e, {10.0f, 20.0f, 30.0f});
	wld.name(e, "Player");

	ser::ser_json writer;
	const bool ok = wld.save_json(writer);
	CHECK(ok);

	const auto& json = writer.str();
	CHECK(json.find("\"format\":1") != BadIndex);
	CHECK(json.find("\"name\":\"Player\"") != BadIndex);
	CHECK(json.find("\"Position\":{\"x\":1,\"y\":2,\"z\":3}") != BadIndex);
	CHECK(json.find("\"PositionSoA\":{\"$raw\":[") != BadIndex);

	bool okStr = false;
	const auto jsonStr = wld.save_json(okStr);
	CHECK(okStr);
	CHECK(jsonStr.find("\"format\":1") != BadIndex);
	CHECK(jsonStr.find("\"binary\":[") != BadIndex);

	TestWorld twldOut;
	(void)twldOut.m_w.add<Position>();
	(void)twldOut.m_w.add<PositionSoA>();
	CHECK(twldOut.m_w.load_json(jsonStr));

	const auto loaded = twldOut.m_w.get("Player");
	CHECK(loaded != ecs::EntityBad);

	const auto posLoaded = twldOut.m_w.get<Position>(loaded);
	CHECK(posLoaded.x == 1.0f);
	CHECK(posLoaded.y == 2.0f);
	CHECK(posLoaded.z == 3.0f);

	const auto posSoaLoaded = twldOut.m_w.get<PositionSoA>(loaded);
	CHECK(posSoaLoaded.x == 10.0f);
	CHECK(posSoaLoaded.y == 20.0f);
	CHECK(posSoaLoaded.z == 30.0f);

	CHECK_FALSE(twldOut.m_w.load_json("{\"format\":1}"));

	ser::ser_json noBinaryWriter;
	const bool okNoBinary = wld.save_json(noBinaryWriter, ser::JsonSaveFlags::RawFallback);
	CHECK(okNoBinary);
	CHECK(noBinaryWriter.str().find("\"binary\":[") == BadIndex);
	TestWorld twldNoBinary;
	(void)twldNoBinary.m_w.add<Position>();
	(void)twldNoBinary.m_w.add<PositionSoA>();
	CHECK_FALSE(twldNoBinary.m_w.load_json(noBinaryWriter.str()));

	ser::ser_json strictWriter;
	const bool okStrict = wld.save_json(strictWriter, ser::JsonSaveFlags::BinarySnapshot /*no raw fallback on purpose*/);
	CHECK_FALSE(okStrict);
	CHECK(strictWriter.str().find("\"PositionSoA\":null") != BadIndex);
	ser::ser_json strictNoBinaryWriter;
	const bool okStrictNoBinary = wld.save_json(strictNoBinaryWriter, ser::JsonSaveFlags::None);
	CHECK_FALSE(okStrictNoBinary);
	TestWorld twldStrictNoBinary;
	(void)twldStrictNoBinary.m_w.add<Position>();
	(void)twldStrictNoBinary.m_w.add<PositionSoA>();
	CHECK_FALSE(twldStrictNoBinary.m_w.load_json(strictNoBinaryWriter.str()));

	TestWorld twldNoBinaryDiag;
	(void)twldNoBinaryDiag.m_w.add<Position>();
	(void)twldNoBinaryDiag.m_w.add<PositionSoA>();
	ser::JsonDiagnostics noBinaryDiagnostics;
	CHECK(twldNoBinaryDiag.m_w.load_json(noBinaryWriter.str(), noBinaryDiagnostics));
	CHECK(noBinaryDiagnostics.hasWarnings);
	bool hasSoaRawWarning = false;
	for (const auto& diag: noBinaryDiagnostics.items) {
		if (diag.reason == ser::JsonDiagReason::SoaRawUnsupported) {
			hasSoaRawWarning = true;
			break;
		}
	}
	CHECK(hasSoaRawWarning);

	ser::JsonDiagnostics missingArchetypesDiagnostics;
	CHECK_FALSE(twldNoBinaryDiag.m_w.load_json("{\"format\":1}", missingArchetypesDiagnostics));
	CHECK(missingArchetypesDiagnostics.hasErrors);
	bool hasMissingArchetypesError = false;
	for (const auto& diag: missingArchetypesDiagnostics.items) {
		if (diag.reason == ser::JsonDiagReason::MissingArchetypesSection) {
			hasMissingArchetypesError = true;
			break;
		}
	}
	CHECK(hasMissingArchetypesError);

	ser::JsonDiagnostics missingFormatDiagnostics;
	CHECK_FALSE(twldNoBinaryDiag.m_w.load_json("{\"archetypes\":[]}", missingFormatDiagnostics));
	CHECK(missingFormatDiagnostics.hasErrors);
	bool hasMissingFormatError = false;
	for (const auto& diag: missingFormatDiagnostics.items) {
		if (diag.reason == ser::JsonDiagReason::MissingFormatField) {
			hasMissingFormatError = true;
			break;
		}
	}
	CHECK(hasMissingFormatError);

	ser::JsonDiagnostics unsupportedFormatDiagnostics;
	CHECK_FALSE(twldNoBinaryDiag.m_w.load_json("{\"format\":2,\"archetypes\":[]}", unsupportedFormatDiagnostics));
	CHECK(unsupportedFormatDiagnostics.hasErrors);
	bool hasUnsupportedFormatError = false;
	for (const auto& diag: unsupportedFormatDiagnostics.items) {
		if (diag.reason == ser::JsonDiagReason::UnsupportedFormatVersion) {
			hasUnsupportedFormatError = true;
			break;
		}
	}
	CHECK(hasUnsupportedFormatError);
}

TEST_CASE("Serialization - world json compatibility when core components are added later") {
	TestWorld archetypeWorld;
	const auto warmup = archetypeWorld.m_w.add();
	archetypeWorld.m_w.name(warmup, "Warmup");

	const auto archetypeIdx = find_serialized_entity_archetype_idx(archetypeWorld.m_w, warmup);
	CHECK(archetypeIdx != BadIndex);
	if (archetypeIdx == BadIndex)
		return;

	const auto currLastCoreComponentId = ecs::GAIA_ID(LastCoreComponent).id();
	CHECK(currLastCoreComponentId > 0);
	if (currLastCoreComponentId == 0)
		return;
	auto legacyBuffer = make_legacy_named_entity_snapshot(currLastCoreComponentId - 1, archetypeIdx, "LegacyJson");
	const auto json = make_binary_snapshot_json(legacyBuffer);

	TestWorld twldOut;
	CHECK(twldOut.m_w.load_json(json.c_str(), (uint32_t)json.size()));

	const auto legacy = twldOut.m_w.get("LegacyJson");
	CHECK(legacy != ecs::EntityBad);
	CHECK(legacy.id() == currLastCoreComponentId + 1);
}

TEST_CASE("Serialization - world json schema nested arrays") {
	struct Vec3 {
		float x, y, z;
	};
	struct JsonComplexComp {
		Vec3 transform[2];
		uint16_t itemCounts[3];
		bool active[2];
		char label[12];
	};

	auto register_schema = [](ecs::World& world) {
		(void)world.add<JsonComplexComp>();
		const auto compEntity = world.comp_cache().get<JsonComplexComp>().entity;
		auto& item = world.comp_cache_mut().get(compEntity);
		item.clear_fields();

		JsonComplexComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offT0X = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.transform[0].x) - pBase);
		const auto offT0Y = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.transform[0].y) - pBase);
		const auto offT0Z = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.transform[0].z) - pBase);
		const auto offT1X = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.transform[1].x) - pBase);
		const auto offT1Y = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.transform[1].y) - pBase);
		const auto offT1Z = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.transform[1].z) - pBase);
		const auto offCount0 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.itemCounts[0]) - pBase);
		const auto offCount1 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.itemCounts[1]) - pBase);
		const auto offCount2 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.itemCounts[2]) - pBase);
		const auto offActive0 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.active[0]) - pBase);
		const auto offActive1 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.active[1]) - pBase);
		const auto offLabel = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.label[0]) - pBase);

		CHECK(item.set_field(
				"transform[0].x", 0, ser::serialization_type_id::f32, offT0X, (uint32_t)sizeof(layout.transform[0].x)));
		CHECK(item.set_field(
				"transform[0].y", 0, ser::serialization_type_id::f32, offT0Y, (uint32_t)sizeof(layout.transform[0].y)));
		CHECK(item.set_field(
				"transform[0].z", 0, ser::serialization_type_id::f32, offT0Z, (uint32_t)sizeof(layout.transform[0].z)));
		CHECK(item.set_field(
				"transform[1].x", 0, ser::serialization_type_id::f32, offT1X, (uint32_t)sizeof(layout.transform[1].x)));
		CHECK(item.set_field(
				"transform[1].y", 0, ser::serialization_type_id::f32, offT1Y, (uint32_t)sizeof(layout.transform[1].y)));
		CHECK(item.set_field(
				"transform[1].z", 0, ser::serialization_type_id::f32, offT1Z, (uint32_t)sizeof(layout.transform[1].z)));
		CHECK(item.set_field(
				"itemCounts[0]", 0, ser::serialization_type_id::u16, offCount0, (uint32_t)sizeof(layout.itemCounts[0])));
		CHECK(item.set_field(
				"itemCounts[1]", 0, ser::serialization_type_id::u16, offCount1, (uint32_t)sizeof(layout.itemCounts[1])));
		CHECK(item.set_field(
				"itemCounts[2]", 0, ser::serialization_type_id::u16, offCount2, (uint32_t)sizeof(layout.itemCounts[2])));
		CHECK(
				item.set_field("active[0]", 0, ser::serialization_type_id::b, offActive0, (uint32_t)sizeof(layout.active[0])));
		CHECK(
				item.set_field("active[1]", 0, ser::serialization_type_id::b, offActive1, (uint32_t)sizeof(layout.active[1])));
		CHECK(item.set_field("label", 0, ser::serialization_type_id::c8, offLabel, (uint32_t)sizeof(layout.label)));
	};

	TestWorld twld;
	register_schema(wld);

	const auto e = wld.add();
	JsonComplexComp value{};
	value.transform[0] = {1.25f, 2.5f, 3.75f};
	value.transform[1] = {4.0f, 5.25f, 6.5f};
	value.itemCounts[0] = 11;
	value.itemCounts[1] = 22;
	value.itemCounts[2] = 33;
	value.active[0] = true;
	value.active[1] = false;
	GAIA_STRCPY(value.label, sizeof(value.label), "crate");
	wld.add<JsonComplexComp>(e, value);
	wld.name(e, "ComplexEntity");

	ser::ser_json writer;
	CHECK(wld.save_json(writer));
	const auto& json = writer.str();
	const auto compSymbol = wld.symbol(wld.comp_cache().get<JsonComplexComp>().entity);
	util::str jsonCompPrefix;
	jsonCompPrefix.append('"');
	jsonCompPrefix.append(compSymbol);
	jsonCompPrefix.append("\":{\"transform[0].x\":1.25");
	CHECK(json.find(jsonCompPrefix.view()) != BadIndex);
	CHECK(json.find("\"transform[1].z\":6.5") != BadIndex);
	CHECK(json.find("\"itemCounts[2]\":33") != BadIndex);
	CHECK(json.find("\"active[0]\":true") != BadIndex);
	CHECK(json.find("\"label\":\"crate\"") != BadIndex);

	TestWorld twldOut;
	register_schema(twldOut.m_w);
	CHECK(twldOut.m_w.load_json(json));

	const auto loaded = twldOut.m_w.get("ComplexEntity");
	CHECK(loaded != ecs::EntityBad);
	const auto loadedComp = twldOut.m_w.get<JsonComplexComp>(loaded);
	CHECK(loadedComp.transform[0].x == 1.25f);
	CHECK(loadedComp.transform[0].y == 2.5f);
	CHECK(loadedComp.transform[0].z == 3.75f);
	CHECK(loadedComp.transform[1].x == 4.0f);
	CHECK(loadedComp.transform[1].y == 5.25f);
	CHECK(loadedComp.transform[1].z == 6.5f);
	CHECK(loadedComp.itemCounts[0] == 11);
	CHECK(loadedComp.itemCounts[1] == 22);
	CHECK(loadedComp.itemCounts[2] == 33);
	CHECK(loadedComp.active[0] == true);
	CHECK(loadedComp.active[1] == false);
	CHECK(std::string(loadedComp.label) == "crate");

	ser::ser_json noBinaryWriter;
	CHECK(wld.save_json(noBinaryWriter, ser::JsonSaveFlags::RawFallback));
	CHECK(noBinaryWriter.str().find("\"binary\":[") == BadIndex);

	TestWorld twldOutNoBinary;
	register_schema(twldOutNoBinary.m_w);
	CHECK(twldOutNoBinary.m_w.load_json(noBinaryWriter.str()));

	const auto loadedNoBinary = twldOutNoBinary.m_w.get("ComplexEntity");
	CHECK(loadedNoBinary != ecs::EntityBad);
	const auto loadedCompNoBinary = twldOutNoBinary.m_w.get<JsonComplexComp>(loadedNoBinary);
	CHECK(loadedCompNoBinary.transform[0].x == 1.25f);
	CHECK(loadedCompNoBinary.transform[0].y == 2.5f);
	CHECK(loadedCompNoBinary.transform[0].z == 3.75f);
	CHECK(loadedCompNoBinary.transform[1].x == 4.0f);
	CHECK(loadedCompNoBinary.transform[1].y == 5.25f);
	CHECK(loadedCompNoBinary.transform[1].z == 6.5f);
	CHECK(loadedCompNoBinary.itemCounts[0] == 11);
	CHECK(loadedCompNoBinary.itemCounts[1] == 22);
	CHECK(loadedCompNoBinary.itemCounts[2] == 33);
	CHECK(loadedCompNoBinary.active[0] == true);
	CHECK(loadedCompNoBinary.active[1] == false);
	CHECK(std::string(loadedCompNoBinary.label) == "crate");
}

TEST_CASE("Serialization - world + query") {
	auto initComponents = [](ecs::World& w) {
		(void)w.add<Position>();
		(void)w.add<PositionSoA>();
		(void)w.add<CustomStruct>();
	};

	char testStr[5] = {'g', 'a', 'i', 'a', '\0'};

	TestWorld in;
	initComponents(in.m_w);

	const uint32_t N = 2;

	cnt::darr<ecs::Entity> ents;
	ents.reserve(N);

	auto create = [&](uint32_t i) {
		auto e = in.m_w.add();
		ents.push_back(e);
		in.m_w.add<Position>(e, {(float)i, (float)i, (float)i});
		in.m_w.add<PositionSoA>(e, {(float)(i * 10), (float)(i * 10), (float)(i * 10)});
		in.m_w.add<CustomStruct>(e, {testStr, 5});
	};
	GAIA_FOR(N) create(i);

	ser::bin_stream buffer;
	in.m_w.set_serializer(buffer);
	in.m_w.save();

	// Load

	TestWorld twld;
	initComponents(wld);
	CHECK(wld.load(buffer));

	auto q1 = wld.query().template all<Position>();
	auto q2 = wld.query().template all<PositionSoA>();
	auto q3 = wld.query().template all<CustomStruct>();

	{
		const auto cnt = q1.count();
		CHECK(cnt == N);
	}
	{
		const auto cnt = q2.count();
		CHECK(cnt == N);
	}
	{
		const auto cnt = q3.count();
		CHECK(cnt == N);
	}

	{
		cnt::darr<ecs::Entity> arr;
		q1.arr(arr);

		// Make sure the same entities are returned by each and arr
		// and that they match out expectations.
		CHECK(arr.size() == N);
		GAIA_EACH(arr) CHECK(arr[i] == ents[i]);

		uint32_t entIdx = 0;
		q1.each([&](ecs::Entity ent) {
			CHECK(ent == arr[entIdx++]);
		});
		entIdx = 0;
		q1.each([&](ecs::Iter& it) {
			auto entView = it.view<ecs::Entity>();
			GAIA_EACH(it) CHECK(entView[i] == arr[entIdx++]);
		});
	}

	{
		cnt::darr<Position> arr;
		q1.arr(arr);
		CHECK(arr.size() == N);
		GAIA_EACH(arr) {
			const auto& pos = arr[i];
			CHECK(pos.x == (float)i);
			CHECK(pos.y == (float)i);
			CHECK(pos.z == (float)i);
		}
	}
	{
		cnt::darr<PositionSoA> arr;
		q2.arr(arr);
		CHECK(arr.size() == N);
		GAIA_EACH(arr) {
			const auto& pos = arr[i];
			CHECK(pos.x == (float)(i * 10));
			CHECK(pos.y == (float)(i * 10));
			CHECK(pos.z == (float)(i * 10));
		}
	}
	{
		cnt::darr_soa<PositionSoA> arr;
		q2.arr(arr);
		CHECK(arr.size() == N);
		auto ppx = arr.view<0>();
		auto ppy = arr.view<1>();
		auto ppz = arr.view<2>();
		GAIA_EACH(arr) {
			const auto x = ppx[i];
			const auto y = ppy[i];
			const auto z = ppz[i];
			CHECK(x == (float)(i * 10));
			CHECK(y == (float)(i * 10));
			CHECK(z == (float)(i * 10));
		}
	}
	{
		cnt::darr<CustomStruct> arr;
		q3.arr(arr);
		CHECK(arr.size() == N);
		GAIA_EACH(arr) {
			const auto& val = arr[i];
			CHECK(val.size == 5);
			const int cmp_res = strncmp(val.ptr, testStr, 5);
			CHECK(cmp_res == 0);
		}
	}
}

//------------------------------------------------------------------------------
// Multithreading
//------------------------------------------------------------------------------

template <typename TQueue>
void TestJobQueue_PushPopSteal(bool reverse) {
	mt::JobHandle handle;
	TQueue q;
	bool res;

	for (uint32_t i = 0; i < 5; ++i) {
		CHECK(q.empty());
		res = q.try_push(mt::JobHandle(1, 0, 0));
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_push(mt::JobHandle(2, 0, 0));
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_push(mt::JobHandle(3, 0, 0));
		CHECK(res);
		CHECK_FALSE(q.empty());

		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_pop(handle);
		CHECK(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
	}

	{
		mt::JobHandle handles[3] = {mt::JobHandle(1, 0, 0), mt::JobHandle(2, 0, 0), mt::JobHandle(3, 0, 0)};
		res = q.try_push(std::span(handles, 3));
		CHECK(res);
		CHECK_FALSE(q.empty());

		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_pop(handle);
		CHECK(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
	}

	{
		CHECK(q.empty());
		(void)q.try_push(mt::JobHandle(1, 0, 0));
		CHECK_FALSE(q.empty());
		(void)q.try_push(mt::JobHandle(2, 0, 0));
		CHECK_FALSE(q.empty());

		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		if (reverse)
			CHECK(handle.id() == 1);
		else
			CHECK(handle.id() == 2);

		res = q.try_pop(handle);
		CHECK(res);
		CHECK(q.empty());
		if (reverse)
			CHECK(handle.id() == 2);
		else
			CHECK(handle.id() == 1);

		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
	}

	{
		(void)q.try_push(mt::JobHandle(1, 0, 0));
		CHECK_FALSE(q.empty());
		(void)q.try_push(mt::JobHandle(2, 0, 0));
		CHECK_FALSE(q.empty());

		res = q.try_steal(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		if (reverse)
			CHECK(handle.id() == 2);
		else
			CHECK(handle.id() == 1);

		res = q.try_steal(handle);
		CHECK(res);
		CHECK(q.empty());
		if (reverse)
			CHECK(handle.id() == 1);
		else
			CHECK(handle.id() == 2);

		res = q.try_steal(handle);
		CHECK(res);
		CHECK(handle == (mt::JobHandle)mt::JobNull_t{});
		CHECK(q.empty());
	}
}

template <typename TQueue>
void TestJobQueue_PushPop(bool reverse) {
	mt::JobHandle handle;
	TQueue q;

	{
		q.try_push(mt::JobHandle(1, 0, 0));
		q.try_push(mt::JobHandle(2, 0, 0));

		auto res = q.try_pop(handle);
		CHECK(res);
		if (reverse)
			CHECK(handle.id() == 1);
		else
			CHECK(handle.id() == 2);

		res = q.try_pop(handle);
		CHECK(res);
		if (reverse)
			CHECK(handle.id() == 2);
		else
			CHECK(handle.id() == 1);

		res = q.try_pop(handle);
		CHECK_FALSE(res);
	}
}

constexpr uint32_t JobQueueMTTesterItems = 500;

template <typename TQueue>
struct JobQueueMTTester_PushPopSteal {
	using QueueType = TQueue;

	TQueue* q;
	bool* terminate;
	uint32_t thread_idx;

	std::thread t;
	uint32_t processed = 0;

	JobQueueMTTester_PushPopSteal(TQueue* q_, bool* terminate_, uint32_t idx):
			q(q_), terminate(terminate_), thread_idx(idx) {}
	JobQueueMTTester_PushPopSteal() = default;

	void init() {
		processed = 0;
		t = std::thread([this]() {
			run();
		});
	}

	void fini() {
		if (t.joinable())
			t.join();
	}

	void run() {
		mt::JobHandle handle;

		if (0 == thread_idx) {
			rnd::pseudo_random rng{};
			uint32_t itemsToInsert = JobQueueMTTesterItems;

			// This thread generates 2 items per tick and consumes one
			while (itemsToInsert > 0) {
				// Keep inserting items
				if (!q->try_push(mt::JobHandle(itemsToInsert, 0, 0))) {
					std::this_thread::yield();
					continue;
				}

				--itemsToInsert;

				if (rng.range(0, 1) == 0)
					std::this_thread::yield();

				if (!q->try_pop(handle))
					std::this_thread::yield();
				else
					++processed;
			}

			// Make sure all items are consumed
			while (!q->empty())
				std::this_thread::yield();
			*terminate = true;
		} else {
			while (!*terminate) {
				// Other threads steal work
				const bool res = q->try_steal(handle);

				// Failed race, try again
				if (!res)
					continue;

				// Empty queue, wait a bit and try again
				if (res && handle == (mt::JobHandle)mt::JobNull_t{}) {
					std::this_thread::yield();
					continue;
				}

				// Processed
				++processed;
			}
		}
	}
};

template <typename TQueue>
struct JobQueueMTTester_PushPop {
	using QueueType = TQueue;

	TQueue* q;
	bool* terminate;
	uint32_t thread_idx;

	std::thread t;
	uint32_t processed = 0;

	JobQueueMTTester_PushPop(TQueue* q_, bool* terminate_, uint32_t idx): q(q_), terminate(terminate_), thread_idx(idx) {}
	JobQueueMTTester_PushPop() = default;

	void init() {
		processed = 0;
		t = std::thread([this]() {
			run();
		});
	}

	void fini() {
		if (t.joinable())
			t.join();
	}

	void run() {
		mt::JobHandle handle;

		if (0 == thread_idx) {
			rnd::pseudo_random rng{};
			uint32_t itemsToInsert = JobQueueMTTesterItems;

			// This thread generates 2 items per tick and consumes one
			while (itemsToInsert > 0) {
				// Keep inserting items
				if (!q->try_push(mt::JobHandle(itemsToInsert, 0, 0))) {
					std::this_thread::yield();
					continue;
				}

				--itemsToInsert;

				if (rng.range(0, 1) == 0)
					std::this_thread::yield();

				if (!q->try_pop(handle))
					std::this_thread::yield();
				else
					++processed;
			}

			// Make sure all items are consumed
			while (!q->empty())
				std::this_thread::yield();
			*terminate = true;
		} else {
			while (!*terminate) {
				// Other threads steal work
				if (!q->try_pop(handle)) {
					std::this_thread::yield();
					continue;
				}

				++processed;
			}
		}
	}
};

template <typename TTestType>
void TestJobQueueMT(uint32_t threadCnt) {
	CHECK(threadCnt > 1);

	typename TTestType::QueueType q;
	bool terminate = false;
	cnt::darray<TTestType> testers;
	GAIA_FOR(threadCnt) testers.push_back(TTestType(&q, &terminate, i));

	GAIA_FOR_(100, j) {
		GAIA_FOR(threadCnt) testers[i].init();
		GAIA_FOR(threadCnt) testers[i].fini();
		uint32_t total = 0;
		GAIA_FOR(threadCnt) {
			total += testers[i].processed;
		}
		CHECK(total == JobQueueMTTesterItems);
		terminate = false;
	}
}

TEST_CASE("JobQueue") {
	using jc = mt::JobQueue<1024>;
	using mpmc = mt::MpmcQueue<mt::JobHandle, 1024>;

	SUBCASE("Basic") {
		TestJobQueue_PushPopSteal<jc>(false);
		TestJobQueue_PushPop<mpmc>(true);
	}

	using mt_tester_jc = JobQueueMTTester_PushPopSteal<jc>;
	using mt_tester_mpmc = JobQueueMTTester_PushPop<mpmc>;

	SUBCASE("MT - 2 threads") {
		TestJobQueueMT<mt_tester_jc>(2);
		TestJobQueueMT<mt_tester_mpmc>(2);
	}

	SUBCASE("MT - 4 threads") {
		TestJobQueueMT<mt_tester_jc>(4);
		TestJobQueueMT<mt_tester_mpmc>(4);
	}

	SUBCASE("MT - 16 threads") {
		TestJobQueueMT<mt_tester_jc>(16);
		TestJobQueueMT<mt_tester_mpmc>(16);
	}
}

static uint32_t JobSystemFunc(std::span<const uint32_t> arr) {
	uint32_t sum = 0;
	GAIA_EACH(arr) sum += arr[i];
	return sum;
}

template <typename Func>
void Run_Schedule_Simple(
		const uint32_t* pArr, uint32_t* pRes, uint32_t jobCnt, uint32_t ItemsPerJob, Func func, uint32_t depCnt) {
	auto& tp = mt::ThreadPool::get();
	std::atomic_uint32_t cnt = 0;

	auto* pHandles = (mt::JobHandle*)alloca(sizeof(mt::JobHandle) * jobCnt);
	GAIA_FOR(jobCnt) {
		mt::Job job;
		job.func = [&, i, func]() {
			const auto idxStart = i * ItemsPerJob;
			const auto idxEnd = (i + 1) * ItemsPerJob;
			pRes[i] += func({pArr + idxStart, idxEnd - idxStart});
			cnt.fetch_add(1, std::memory_order_relaxed);
		};
		pHandles[i] = tp.add(job);
	}

	mt::Job dependencyJob;
	dependencyJob.func = [&]() {
		const bool isLast = cnt.load(std::memory_order_acquire) == jobCnt;
		CHECK(isLast);
	};
	auto* pDepHandles = (mt::JobHandle*)alloca(sizeof(mt::JobHandle) * depCnt);
	GAIA_FOR(depCnt) {
		pDepHandles[i] = tp.add(dependencyJob);
		tp.dep(std::span(pHandles, jobCnt), pDepHandles[i]);
		tp.submit(pDepHandles[i]);
	}

	tp.submit(std::span(pHandles, jobCnt));

	GAIA_FOR(depCnt) {
		tp.wait(pDepHandles[i]);
	}

	GAIA_FOR(jobCnt) CHECK(pRes[i] == ItemsPerJob);
	CHECK(cnt == jobCnt);
}

TEST_CASE("Multithreading - Schedule") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t JobCount = 64;
	constexpr uint32_t ItemsPerJob = 5000;
	constexpr uint32_t N = JobCount * ItemsPerJob;

	cnt::sarray<uint32_t, JobCount> res;

	cnt::darr<uint32_t> arr;
	arr.resize(N);
	GAIA_EACH(arr) arr[i] = 1;

	SUBCASE("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 1);
	}
	SUBCASE("Max workers Deps2") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 2);
	}
	SUBCASE("Max workers Deps3") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 3);
	}
	SUBCASE("Max workers Deps4") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 4);
	}
	SUBCASE("0 workers") {
		tp.set_max_workers(0, 0);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 1);
	}
	SUBCASE("0 workers Deps2") {
		tp.set_max_workers(0, 0);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 2);
	}
	SUBCASE("0 workers Deps3") {
		tp.set_max_workers(0, 0);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 3);
	}
	SUBCASE("0 workers Deps4") {
		tp.set_max_workers(0, 0);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 4);
	}
}

TEST_CASE("Multithreading - ScheduleParallel") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t JobCount = 64;
	constexpr uint32_t ItemsPerJob = 5000;
	constexpr uint32_t N = JobCount * ItemsPerJob;

	cnt::darr<uint32_t> arr;
	arr.resize(N);
	GAIA_EACH(arr) arr[i] = 1;

	std::atomic_uint32_t sum1 = 0;
	mt::JobParallel j1;
	j1.func = [&arr, &sum1](const mt::JobArgs& args) {
		sum1 += JobSystemFunc({arr.data() + args.idxStart, args.idxEnd - args.idxStart});
	};

	auto work = [&]() {
		auto jobHandle = tp.sched_par(j1, N, ItemsPerJob);
		tp.wait(jobHandle);
		CHECK(sum1 == N);
	};

	SUBCASE("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		work();
	}
	SUBCASE("0 workers") {
		tp.set_max_workers(0, 0);

		work();
	}
}

TEST_CASE("Multithreading - complete") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t Jobs = 15000;

	cnt::darr<mt::JobHandle> handles;
	cnt::darr<uint32_t> res;

	handles.resize(Jobs);
	res.resize(Jobs);

	std::atomic_uint32_t cnt = 0;

	auto work = [&]() {
		GAIA_EACH(res) res[i] = BadIndex;

		GAIA_FOR(Jobs) {
			mt::Job job;
			job.flags = mt::JobCreationFlags::ManualDelete;
			job.func = [&, i]() {
				res[i] = i;
				++cnt;
			};
			handles[i] = tp.add(job);
		}

		tp.submit(std::span(handles.data(), handles.size()));

		GAIA_FOR(Jobs) {
			tp.wait(handles[i]);
			CHECK(res[i] == i);
			tp.del(handles[i]);
		}

		CHECK(cnt == Jobs);
	};

	SUBCASE("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		work();
	}
	SUBCASE("0 workers") {
		tp.set_max_workers(0, 0);

		work();
	}
}

TEST_CASE("Multithreading - CompleteMany") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t Iters = 15000;

	auto work = [&]() {
		srand(0);
		uint32_t res = BadIndex;

		GAIA_FOR(Iters) {
			mt::Job job0{[&res, i]() {
				res = (i + 1);
			}};
			mt::Job job1{[&res, i]() {
				res *= (i + 1);
			}};
			mt::Job job2{[&res, i]() {
				res /= (i + 1); // we add +1 everywhere to avoid division by zero at i==0
			}};

			const mt::JobHandle jobHandle[] = {tp.add(job0), tp.add(job1), tp.add(job2)};

			tp.dep(jobHandle[0], jobHandle[1]);
			tp.dep(jobHandle[1], jobHandle[2]);

			// 2, 0, 1 -> wrong sum
			// Submit jobs in random order to make sure this doesn't work just by accident
			const uint32_t startIdx0 = (uint32_t)rand() % 3;
			const uint32_t startIdx1 = (startIdx0 + 1) % 3;
			const uint32_t startIdx2 = (startIdx0 + 2) % 3;
			tp.submit(jobHandle[startIdx0]);
			tp.submit(jobHandle[startIdx1]);
			tp.submit(jobHandle[startIdx2]);

			tp.wait(jobHandle[2]);

			CHECK(res == (i + 1));
		}
	};

	SUBCASE("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		work();
	}
	SUBCASE("0 workers") {
		tp.set_max_workers(0, 0);

		work();
	}
}

TEST_CASE("Multithreading - Reset reusable handles") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t Iters = 1024;

	std::atomic_uint32_t stage = 0;
	std::atomic_uint32_t firstCnt = 0;
	std::atomic_uint32_t secondCnt = 0;
	std::atomic_bool ordered = true;

	mt::Job job1;
	job1.flags = mt::JobCreationFlags::ManualDelete;
	job1.func = [&]() {
		firstCnt.fetch_add(1, std::memory_order_relaxed);
		stage.store(1, std::memory_order_release);
	};

	mt::Job job2;
	job2.flags = mt::JobCreationFlags::ManualDelete;
	job2.func = [&]() {
		if (stage.load(std::memory_order_acquire) != 1)
			ordered.store(false, std::memory_order_relaxed);
		secondCnt.fetch_add(1, std::memory_order_relaxed);
	};

	auto handle1 = tp.add(job1);
	auto handle2 = tp.add(job2);
	mt::JobHandle handles[] = {handle1, handle2};

	tp.dep(handle1, handle2);
	GAIA_FOR(Iters) {
		stage.store(0, std::memory_order_relaxed);

		tp.submit(handle2);
		tp.submit(handle1);
		tp.reset(std::span(handles, 2));

		CHECK(ordered.load(std::memory_order_relaxed));
		tp.dep_refresh(handle1, handle2);
	}

	// Final run leaves handles in Done state so they can be deleted.
	stage.store(0, std::memory_order_relaxed);
	tp.submit(handle2);
	tp.submit(handle1);
	tp.wait(handle2);

	CHECK(ordered.load(std::memory_order_relaxed));
	CHECK(firstCnt.load(std::memory_order_relaxed) == Iters + 1);
	CHECK(secondCnt.load(std::memory_order_relaxed) == Iters + 1);

	tp.del(handle1);
	tp.del(handle2);
}

#if GAIA_SYSTEMS_ENABLED

struct SomeData1 {
	uint32_t val;
};
struct SomeData2 {
	uint32_t val;
};

TEST_CASE("Multithreading - Systems") {
	auto& tp = mt::ThreadPool::get();

	uint32_t data1 = 0;
	uint32_t data2 = 0;
	TestWorld twld;

	constexpr uint32_t Iters = 15000;

	{
		// 10k of SomeData1
		auto e = wld.add();
		wld.add<SomeData1>(e, {0});
		wld.copy_n(e, 9999);

		// 10k of SomeData1, SomeData2
		auto e2 = wld.add();
		wld.add<SomeData1>(e2, {0});
		wld.add<SomeData2>(e2, {0});
		wld.copy_n(e2, 9999);
	}

	auto sys1 = wld.system()
									.name("sys1")
									.all<SomeData1>()
									.all<SomeData2>() //
									.on_each([&](SomeData1, SomeData2) {
										++data1;
										++data2;
									});
	auto sys2 = wld.system()
									.name("sys2")
									.all<SomeData1>() //
									.on_each([&](SomeData1) {
										++data1;
									});

	auto handle1 = sys1.job_handle();
	auto handle2 = sys2.job_handle();

	tp.dep(handle1, handle2);
	GAIA_FOR(Iters - 1) {
		tp.submit(handle2);
		tp.submit(handle1);
		tp.wait(handle2);
		GAIA_ASSERT(data1 == 30000);
		GAIA_ASSERT(data2 == 10000);

		data1 = 0;
		data2 = 0;
		tp.reset_state(handle1);
		tp.reset_state(handle2);
		tp.dep_refresh(handle1, handle2);
	}
	{
		tp.submit(handle2);
		tp.submit(handle1);
		tp.wait(handle2);
		GAIA_ASSERT(data1 == 30000);
		GAIA_ASSERT(data2 == 10000);
	}
}

#endif

TEST_CASE("Multithreading - Reset handles missing TLS worker context") {
	auto& tp = mt::ThreadPool::get();

	// Keep this deterministic regardless of previous tests.
	tp.set_max_workers(2, 2);
	CHECK(tp.workers() == 1);

	// Simulate shutdown from a thread that has no TLS worker context bound.
	mt::detail::tl_workerCtx = nullptr;

	// set_max_workers() calls reset() internally, which used to dereference null TLS context.
	tp.set_max_workers(2, 2);
	CHECK(tp.workers() == 1);
}

TEST_CASE("Multithreading - Update auto-delete job") {
	auto& tp = mt::ThreadPool::get();
	tp.set_max_workers(0, 0);
	CHECK(tp.workers() == 0);

	std::atomic_uint32_t executed = 0;

	// update() drives main_thread_tick() and must not double-delete auto jobs.
	constexpr uint32_t Iters = 1024;
	GAIA_FOR(Iters) {
		mt::Job job;
		job.func = [&]() {
			executed.fetch_add(1, std::memory_order_relaxed);
		};

		const auto handle = tp.add(job);
		tp.submit(handle);
		tp.update();
	}

	CHECK(executed.load(std::memory_order_relaxed) == Iters);
}

TEST_CASE("Multithreading - Update auto-delete with workers") {
	auto& tp = mt::ThreadPool::get();
	tp.set_max_workers(2, 2);
	CHECK(tp.workers() == 1);

	constexpr uint32_t Jobs = 2048;
	std::atomic_uint32_t executed = 0;

	cnt::darr<mt::JobHandle> handles;
	handles.resize(Jobs);

	GAIA_FOR(Jobs) {
		mt::Job job;
		job.func = [&]() {
			executed.fetch_add(1, std::memory_order_relaxed);
		};
		handles[i] = tp.add(job);
	}

	mt::Job sync;
	sync.flags = mt::JobCreationFlags::ManualDelete;
	const auto syncHandle = tp.add(sync);
	tp.dep(std::span(handles.data(), handles.size()), syncHandle);

	tp.submit(syncHandle);
	tp.submit(std::span(handles.data(), handles.size()));

	// Keep ticking from the main thread while worker threads are active.
	// This exercises the same path that previously double-deleted auto jobs.
	while (executed.load(std::memory_order_relaxed) < Jobs) {
		tp.update();
	}

	tp.wait(syncHandle);
	tp.del(syncHandle);
	CHECK(executed.load(std::memory_order_relaxed) == Jobs);
}

TEST_CASE("Multithreading - Handle reuse mixed delete modes") {
	auto& tp = mt::ThreadPool::get();
	tp.set_max_workers(2, 2);
	CHECK(tp.workers() == 1);

	std::atomic_uint32_t autoCnt = 0;
	std::atomic_uint32_t manualCnt = 0;

	constexpr uint32_t Iters = 4096;
	GAIA_FOR(Iters) {
		mt::Job autoJob;
		autoJob.func = [&]() {
			autoCnt.fetch_add(1, std::memory_order_relaxed);
		};

		mt::Job manualJob;
		manualJob.flags = mt::JobCreationFlags::ManualDelete;
		manualJob.func = [&]() {
			manualCnt.fetch_add(1, std::memory_order_relaxed);
		};

		const auto autoHandle = tp.add(autoJob);
		const auto manualHandle = tp.add(manualJob);

		tp.submit(autoHandle);
		tp.submit(manualHandle);
		tp.wait(manualHandle);
		tp.del(manualHandle);

		if ((i & 7) == 0)
			tp.update();
	}

	// Drain any remaining auto jobs.
	while (autoCnt.load(std::memory_order_relaxed) < Iters)
		tp.update();

	CHECK(autoCnt.load(std::memory_order_relaxed) == Iters);
	CHECK(manualCnt.load(std::memory_order_relaxed) == Iters);
}

TEST_CASE("Parent - non-fragmenting relation and structural archetype cache") {
	TestWorld twld;

	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto e1 = wld.add();
	const auto e2 = wld.add();

	wld.add<Position>(e1);
	wld.add<Position>(e2);

	auto q = wld.query().all<Position>();
	(void)q.count();
	auto& info = q.fetch();
	CHECK(info.cache_archetype_view().size() == 1);

	wld.parent(e1, rootA);
	wld.parent(e2, rootB);

	CHECK(wld.has(e1, ecs::Pair(ecs::Parent, rootA)));
	CHECK(wld.has(e2, ecs::Pair(ecs::Parent, rootB)));
	CHECK(q.count() == 2);
	CHECK(info.cache_archetype_view().size() == 1);
}

TEST_CASE("Parent - targets and sources use adjunct storage") {
	TestWorld twld;

	const auto root = wld.add();
	const auto child = wld.add();

	wld.parent(child, root);

	CHECK(wld.has(child, ecs::Pair(ecs::Parent, root)));
	CHECK(wld.target(child, ecs::Parent) == root);

	cnt::darray<ecs::Entity> targets;
	wld.targets(child, ecs::Parent, [&](ecs::Entity target) {
		targets.push_back(target);
	});
	CHECK(targets.size() == 1);
	CHECK(targets[0] == root);

	cnt::darray<ecs::Entity> sources;
	wld.sources(ecs::Parent, root, [&](ecs::Entity source) {
		sources.push_back(source);
	});
	CHECK(sources.size() == 1);
	CHECK(sources[0] == child);
}

TEST_CASE("Parent - deleting target deletes children through adjunct relation") {
	TestWorld twld;

	const auto root = wld.add();
	const auto child = wld.add();

	wld.add<Position>(child);
	wld.parent(child, root);

	wld.del(root);
	wld.update();

	CHECK(!wld.has(root));
	CHECK(!wld.has(child));
}

TEST_CASE("Parent - breadth-first traversal on adjunct relation") {
	TestWorld twld;

	const auto root = wld.add();
	const auto child = wld.add();
	const auto leaf = wld.add();

	wld.parent(child, root);
	wld.parent(leaf, child);

	cnt::darray<ecs::Entity> traversed;
	wld.sources_bfs(ecs::Parent, root, [&](ecs::Entity entity) {
		traversed.push_back(entity);
	});

	CHECK(traversed.size() == 2);
	CHECK(traversed[0] == child);
	CHECK(traversed[1] == leaf);
}

TEST_CASE("Parent - direct query terms are evaluated as entity filters") {
	TestWorld twld;

	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto eA = wld.add();
	const auto eB = wld.add();
	const auto eC = wld.add();

	wld.add<Position>(eA);
	wld.add<Position>(eB);
	wld.add<Position>(eC);
	wld.add<Scale>(eC);

	wld.parent(eA, rootA);
	wld.parent(eB, rootB);

	auto qAll = wld.query().all<Position>().all(ecs::Pair(ecs::Parent, rootA));
	CHECK(qAll.count() == 1);
	expect_exact_entities(qAll, {eA});
	CHECK(!qAll.empty());

	auto qNo = wld.query().all<Position>().no(ecs::Pair(ecs::Parent, rootA));
	CHECK(qNo.count() == 2);
	expect_exact_entities(qNo, {eB, eC});

	auto qOr = wld.query().or_(ecs::Pair(ecs::Parent, rootA)).or_<Scale>();
	CHECK(qOr.count() == 2);
	expect_exact_entities(qOr, {eA, eC});
}

TEST_CASE("Sparse DontFragment component and adjunct storage") {
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

TEST_CASE("DontFragment table component uses out-of-line storage") {
	TestWorld twld;

	const auto& compItem = wld.add<Position>();
	wld.add(compItem.entity, ecs::DontFragment);
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Sparse);
	CHECK(wld.is_out_of_line_component(compItem.entity));

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

TEST_CASE("Sparse component uses out-of-line storage and still fragments") {
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

TEST_CASE("Table component can opt into sparse storage via trait") {
	TestWorld twld;

	const auto& compItem = wld.add<Position>();
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Table);
	CHECK_FALSE(wld.has(compItem.entity, ecs::Sparse));

	wld.add(compItem.entity, ecs::Sparse);
	CHECK(compItem.comp.storage_type() == ecs::DataStorageType::Sparse);
	CHECK(wld.has(compItem.entity, ecs::Sparse));
	CHECK(wld.is_out_of_line_component(compItem.entity));
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

TEST_CASE("Sparse prefab instantiate copies out-of-line payload") {
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

TEST_CASE("Sparse prefab sync keeps copied out-of-line payload after prefab delete") {
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

	const auto& runtimeComp = wld.add(
			"Runtime_Sparse_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse, (uint32_t)alignof(Position));
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

TEST_CASE("Sparse runtime-registered component uses out-of-line storage and still fragments") {
	TestWorld twld;

	const auto& runtimeComp = wld.add(
			"Runtime_Sparse_Fragmenting_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
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

TEST_CASE("Runtime-registered table component can opt into sparse storage via trait") {
	TestWorld twld;

	const auto& runtimeComp = wld.add(
			"Runtime_Table_Position_Becomes_Sparse", (uint32_t)sizeof(Position), ecs::DataStorageType::Table,
			(uint32_t)alignof(Position));
	CHECK(runtimeComp.comp.storage_type() == ecs::DataStorageType::Table);
	CHECK_FALSE(wld.has(runtimeComp.entity, ecs::Sparse));

	wld.add(runtimeComp.entity, ecs::Sparse);
	CHECK(runtimeComp.comp.storage_type() == ecs::DataStorageType::Sparse);
	CHECK(wld.has(runtimeComp.entity, ecs::Sparse));
	CHECK(wld.is_out_of_line_component(runtimeComp.entity));

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

	const auto& runtimeComp = wld.add(
			"Runtime_Table_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Table, (uint32_t)alignof(Position));
	wld.add(runtimeComp.entity, ecs::DontFragment);
	CHECK(runtimeComp.comp.storage_type() == ecs::DataStorageType::Sparse);
	CHECK(wld.is_out_of_line_component(runtimeComp.entity));

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

	const auto& runtimeTableComp = wld.add(
			"Runtime_Table_Position_Setter", (uint32_t)sizeof(Position), ecs::DataStorageType::Table,
			(uint32_t)alignof(Position));
	const auto& runtimeSparseComp = wld.add(
			"Runtime_Sparse_Position_Setter", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
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

	const auto& runtimeComp = wld.add(
			"Runtime_Sparse_Position_Delete", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
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

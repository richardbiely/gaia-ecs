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
	CHECK(other.comp.id() == other.entity.id());
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
	{
		uint8_t soaSizes[meta::StructToTupleMaxTypes]{};
		const auto desc = ecs::detail::ComponentDesc<Position>::make(
				util::str_view("Position"), std::span<uint8_t, meta::StructToTupleMaxTypes>{soaSizes});

		CHECK(desc.name == util::str_view("Position"));
		CHECK(desc.size == (uint32_t)sizeof(Position));
		CHECK(desc.alig == (uint32_t)alignof(Position));
		CHECK(desc.storageType == ecs::DataStorageType::Table);
		CHECK(desc.soa == 0);
		CHECK(desc.pSoaSizes == soaSizes);
		CHECK(desc.hashLookup == ecs::ComponentLookupHash{meta::type_info::hash<Position>()});
		CHECK(desc.funcCtor == nullptr);
		CHECK(desc.funcDtor == nullptr);
		CHECK(desc.funcCopy != nullptr);
		CHECK(desc.funcMove != nullptr);
		CHECK(desc.funcSwap != nullptr);
		CHECK(desc.funcCmp != nullptr);
		CHECK(desc.funcSave != nullptr);
		CHECK(desc.funcLoad != nullptr);
	}
}

TEST_CASE("Component cache - runtime registration") {
	constexpr uint32_t RuntimePayloadSize = 12;
	constexpr uint32_t RuntimePayloadAlign = 4;
	constexpr uint32_t RuntimeXOffset = 0;
	constexpr uint32_t RuntimeYOffset = 4;
	constexpr uint32_t RuntimeZOffset = 8;

	const auto write_f32 = [](void* data, uint32_t offset, float value) {
		memcpy((uint8_t*)data + offset, &value, sizeof(value));
	};
	const auto read_f32 = [](const void* data, uint32_t offset) {
		float value{};
		memcpy(&value, (const uint8_t*)data + offset, sizeof(value));
		return value;
	};
	const auto write_xyz = [&](void* data, float x, float y, float z) {
		write_f32(data, RuntimeXOffset, x);
		write_f32(data, RuntimeYOffset, y);
		write_f32(data, RuntimeZOffset, z);
	};

	SUBCASE("basic registration populates runtime metadata and lookups") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		constexpr const char* RuntimeCompName = "Runtime_Component_Basic";
		const auto entity = wld.add();
		const auto& item = add_runtime_component(
				cc, entity, RuntimeCompName, RuntimePayloadSize, ecs::DataStorageType::Table, RuntimePayloadAlign);

		const auto nameLen = (uint32_t)GAIA_STRLEN(RuntimeCompName, ecs::ComponentCacheItem::MaxNameLength);
		CHECK(item.entity == entity);
		CHECK(item.comp.id() == item.entity.id());
		CHECK(item.comp.size() == RuntimePayloadSize);
		CHECK(item.comp.alig() == RuntimePayloadAlign);
		CHECK(item.comp.soa() == 0);
		CHECK(item.name.len() == nameLen);
		CHECK(strcmp(item.name.str(), RuntimeCompName) == 0);
		CHECK(item.hashLookup.hash == core::calculate_hash64(RuntimeCompName, nameLen));

		CHECK(cc.find(item.entity) == &item);
		CHECK(wld.symbol(RuntimeCompName) == item.entity);
		CHECK(wld.symbol(RuntimeCompName, nameLen) == item.entity);
		CHECK(wld.symbol(RuntimeCompName, nameLen - 1) == ecs::EntityBad);
		CHECK(wld.get(RuntimeCompName) == item.entity);
	}

	SUBCASE("plain descriptor registration populates runtime metadata") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		constexpr const char* RuntimeCompName = "Runtime_Component_Desc";
		const auto nameLen = (uint32_t)GAIA_STRLEN(RuntimeCompName, ecs::ComponentCacheItem::MaxNameLength);
		const auto entity = wld.add();

		ecs::ComponentDesc desc{};
		desc.name = util::str_view(RuntimeCompName, nameLen);
		desc.size = RuntimePayloadSize;
		desc.alig = RuntimePayloadAlign;
		desc.storageType = ecs::DataStorageType::Table;

		const auto& item = cc.add(entity, desc);
		CHECK(item.entity == entity);
		CHECK(item.comp.size() == RuntimePayloadSize);
		CHECK(item.comp.alig() == RuntimePayloadAlign);
		CHECK(item.comp.storage_type() == ecs::DataStorageType::Table);
		CHECK(item.name.len() == nameLen);
		CHECK(strcmp(item.name.str(), RuntimeCompName) == 0);
		CHECK(cc.find(entity) == &item);
		CHECK(wld.symbol(RuntimeCompName) == item.entity);
	}

	SUBCASE("reflected primitive ids and runtime fields are available") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		const auto* f32Info = cc.find(ecs::F32);
		CHECK(f32Info != nullptr);
		if (f32Info != nullptr) {
			CHECK(f32Info->typeKind == ecs::RuntimeTypeKind::Primitive);
			CHECK(f32Info->primitiveKind == ecs::RuntimePrimitiveKind::F32);
			CHECK(f32Info->comp.size() == 4);
			CHECK(f32Info->comp.alig() == 4);
		}

		constexpr const char* RuntimeCompName = "Runtime_Component_Fields";
		const auto entity = wld.add();

		ecs::ComponentDesc desc{};
		desc.name = runtime_component_name_view(RuntimeCompName);
		desc.size = 24;
		desc.alig = 4;
		desc.typeKind = ecs::RuntimeTypeKind::Struct;

		auto& item = const_cast<ecs::ComponentCacheItem&>(cc.add(entity, desc));
		CHECK(item.typeKind == ecs::RuntimeTypeKind::Struct);
		CHECK(item.primitiveKind == ecs::RuntimePrimitiveKind::None);
		CHECK(item.field_count() == 0);

		CHECK(cc.add_field(entity, {util::str_view("x"), ecs::F32, 0, 0}));
		CHECK(cc.add_field(entity, {util::str_view("color"), ecs::F32, 8, 4}));
		CHECK_FALSE(cc.add_field(entity, {util::str_view("bad"), ecs::EntityBad, 0, 0}));
		CHECK_FALSE(cc.add_field(entity, {util::str_view("overflow"), ecs::F32, 24, 1}));
		CHECK(item.field_count() == 2);

		const ecs::RuntimeField* field = nullptr;
		CHECK(item.field(util::str_view("x"), &field));
		CHECK(field != nullptr);
		if (field != nullptr) {
			CHECK(field->type == ecs::F32);
			CHECK(field->offset == 0);
			CHECK(field->count == 0);
			CHECK(item.field_element_count(*field) == 1);
		}

		CHECK(item.field(util::str_view("color"), &field));
		CHECK(field != nullptr);
		if (field != nullptr) {
			CHECK(field->offset == 8);
			CHECK(field->count == 4);
			CHECK(item.field_element_count(*field) == 4);
		}
	}

	SUBCASE("duplicate runtime registration is idempotent") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		constexpr const char* RuntimeCompName = "Runtime_Component_Duplicate";
		const auto entityA = wld.add();
		const auto& original = add_runtime_component(cc, entityA, RuntimeCompName, 16, ecs::DataStorageType::Table, 4);
		const auto originalHash = original.hashLookup.hash;

		constexpr uint8_t SoaSizes[] = {1, 2, 4};
		const ecs::ComponentLookupHash customHash{0x123456789abcdef0ull};
		const auto entityB = wld.add(ecs::EntityKind::EK_Uni);
		const auto& duplicate = add_runtime_component(
				cc, entityB, RuntimeCompName, 64, ecs::DataStorageType::Table, 8, 3, SoaSizes, customHash);

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

		const auto& item =
				add_runtime_component(cc, entity, RuntimeCompName, 32, ecs::DataStorageType::Table, 8, 3, SoaSizes, customHash);

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
		CHECK(cc.get(item.entity).entity == item.entity);
	}

	SUBCASE("typed/runtime registration sync component record") {
		TestWorld twld;

		const auto& typed = wld.add<Position>();
		CHECK(wld.get<ecs::Component>(typed.entity) == typed.comp);
		const auto typedSymbol = wld.symbol(typed.entity);
		CHECK(typedSymbol == gaia::util::str_view(typed.name.str(), typed.name.len()));

		const auto& runtime = add_runtime_component(
				wld, "Runtime_Component_Finalize", RuntimePayloadSize, ecs::DataStorageType::Sparse, RuntimePayloadAlign);
		CHECK(wld.get<ecs::Component>(runtime.entity) == runtime.comp);
		const auto runtimeSymbol = wld.symbol(runtime.entity);
		CHECK(runtimeSymbol == gaia::util::str_view("Runtime_Component_Finalize"));
		CHECK(wld.has(runtime.entity, ecs::Sparse));
	}

	SUBCASE("symbol path alias and display name each have explicit behavior") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		auto entityA = wld.add();
		(void)add_runtime_component(cc, entityA, "Gameplay::Device", 0, ecs::DataStorageType::Table, 1);
		auto& itemA = cc.get(entityA);
		CHECK(wld.get("Device") == itemA.entity);
		CHECK(wld.display_name(itemA.entity) == "Gameplay.Device");

		auto entityB = wld.add();
		(void)add_runtime_component(cc, entityB, "Debug::Device", 0, ecs::DataStorageType::Table, 1);
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
		(void)add_runtime_component(cc, entityA, "Gameplay::Device", 0, ecs::DataStorageType::Table, 1);

		const auto entityB = wld.add();
		(void)add_runtime_component(cc, entityB, "Debug::Device", 0, ecs::DataStorageType::Table, 1);

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

	SUBCASE("world resolve reports unique short-symbol matches") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		const auto entity = wld.add();
		(void)add_runtime_component(cc, entity, "Gameplay::Device", 0, ecs::DataStorageType::Table, 1);

		cnt::darray<ecs::Entity> out;
		wld.resolve(out, "Device");
		CHECK(out.size() == 1);
		CHECK(out[0] == entity);
	}

	SUBCASE("path ambiguity resolves when one component leaves the shared path") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		const auto entityA = wld.add();
		(void)add_runtime_component(cc, entityA, "Gameplay::Device", 0, ecs::DataStorageType::Table, 1);

		const auto entityB = wld.add();
		(void)add_runtime_component(cc, entityB, "Debug::Device", 0, ecs::DataStorageType::Table, 1);

		CHECK(wld.path(entityA, "shared.Device"));
		CHECK(wld.path("shared.Device") == entityA);

		CHECK(wld.path(entityB, "shared.Device"));
		CHECK(wld.path("shared.Device") == ecs::EntityBad);
		CHECK(wld.get("shared.Device") == ecs::EntityBad);

		CHECK(wld.path(entityB, "debug.Device"));
		CHECK(wld.path("shared.Device") == entityA);
		CHECK(wld.get("shared.Device") == entityA);
	}

	SUBCASE("path diagnostics report shared-path components") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		const auto entityA = wld.add();
		(void)add_runtime_component(cc, entityA, "Gameplay::Device", 0, ecs::DataStorageType::Table, 1);
		CHECK(wld.path(entityA, "shared.Device"));

		const auto entityB = wld.add();
		(void)add_runtime_component(cc, entityB, "Debug::Device", 0, ecs::DataStorageType::Table, 1);
		CHECK(wld.path(entityB, "shared.Device"));

		cnt::darray<ecs::Entity> out;
		wld.resolve(out, "shared.Device");
		CHECK(out.size() == 2);
		CHECK((out[0] == entityA || out[1] == entityA));
		CHECK((out[0] == entityB || out[1] == entityB));
	}

	SUBCASE("schema field registration supports add update and clear") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		const auto entity = wld.add();
		(void)add_runtime_component(cc, entity, "Runtime_Component_Schema", 24, ecs::DataStorageType::Table, 8);
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

	SUBCASE("runtime registration uses entity-backed lookups") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		const auto entity = wld.add();
		const auto& item =
				add_runtime_component(cc, entity, "Runtime_Component_Map_Path", 12, ecs::DataStorageType::Table, 4);

		CHECK(item.comp.id() == item.entity.id());
		CHECK(cc.get(item.entity).entity == item.entity);
		CHECK(cc.find(item.entity) == &item);
		CHECK(wld.symbol("Runtime_Component_Map_Path") == item.entity);
		CHECK(wld.get("Runtime_Component_Map_Path") == item.entity);
	}

	SUBCASE("runtime raw payload access supports chunk-backed components") {
		TestWorld twld;

		const auto& runtimeComp = add_runtime_component(
				wld, "Runtime_Component_Raw", RuntimePayloadSize, ecs::DataStorageType::Table, RuntimePayloadAlign);
		CHECK(wld.comp_cache_mut().add_field(runtimeComp.entity, {util::str_view("x"), ecs::F32, RuntimeXOffset, 0}));
		CHECK(wld.comp_cache_mut().add_field(runtimeComp.entity, {util::str_view("y"), ecs::F32, RuntimeYOffset, 0}));
		CHECK(wld.comp_cache_mut().add_field(runtimeComp.entity, {util::str_view("z"), ecs::F32, RuntimeZOffset, 0}));

		uint32_t addHits = 0;
		uint8_t addSeen[RuntimePayloadSize]{};
		const auto onAdd = wld.observer()
													 .event(ecs::ObserverEvent::OnAdd)
													 .all(runtimeComp.entity)
													 .on_each([&](ecs::Entity entity) {
														 ++addHits;
														 const auto payload = wld.get_raw(entity, runtimeComp.entity);
														 CHECK(payload.valid());
														 CHECK(payload.size == RuntimePayloadSize);
														 if (payload.data != nullptr)
															 memcpy(addSeen, payload.data, RuntimePayloadSize);
													 })
													 .entity();
		(void)onAdd;

		uint32_t setHits = 0;
		uint8_t setSeen[RuntimePayloadSize]{};
		const auto onSet = wld.observer()
													 .event(ecs::ObserverEvent::OnSet)
													 .all(runtimeComp.entity)
													 .on_each([&](ecs::Entity entity) {
														 ++setHits;
														 const auto payload = wld.get_raw(entity, runtimeComp.entity);
														 CHECK(payload.valid());
														 CHECK(payload.size == RuntimePayloadSize);
														 if (payload.data != nullptr)
															 memcpy(setSeen, payload.data, RuntimePayloadSize);
													 })
													 .entity();
		(void)onSet;

		const auto entity = wld.add();
		uint8_t initial[RuntimePayloadSize]{};
		write_xyz(initial, 1.0f, 2.0f, 3.0f);
		CHECK(wld.add_raw(entity, runtimeComp.entity, initial, RuntimePayloadSize));
		CHECK(addHits == 1);
		CHECK(read_f32(addSeen, RuntimeXOffset) == doctest::Approx(1.0f));
		CHECK(read_f32(addSeen, RuntimeYOffset) == doctest::Approx(2.0f));
		CHECK(read_f32(addSeen, RuntimeZOffset) == doctest::Approx(3.0f));
		CHECK(setHits == 0);

		const auto payload = wld.get_raw(entity, runtimeComp.entity);
		CHECK(payload.valid());
		CHECK(payload.size == RuntimePayloadSize);
		CHECK(payload.data != nullptr);
		if (payload.data != nullptr) {
			CHECK(read_f32(payload.data, RuntimeXOffset) == doctest::Approx(1.0f));
			CHECK(read_f32(payload.data, RuntimeYOffset) == doctest::Approx(2.0f));
			CHECK(read_f32(payload.data, RuntimeZOffset) == doctest::Approx(3.0f));
		}

		auto mutablePayload = wld.mut_raw(entity, runtimeComp.entity);
		CHECK(mutablePayload.valid());
		CHECK(mutablePayload.size == RuntimePayloadSize);
		CHECK(mutablePayload.data != nullptr);
		if (mutablePayload.data != nullptr) {
			write_xyz(mutablePayload.data, 4.0f, 5.0f, 6.0f);
		}
		CHECK(setHits == 0);

		wld.modify_raw(entity, runtimeComp.entity);
		CHECK(setHits == 1);
		CHECK(read_f32(setSeen, RuntimeXOffset) == doctest::Approx(4.0f));
		CHECK(read_f32(setSeen, RuntimeYOffset) == doctest::Approx(5.0f));
		CHECK(read_f32(setSeen, RuntimeZOffset) == doctest::Approx(6.0f));

		uint8_t replacement[RuntimePayloadSize]{};
		write_xyz(replacement, 7.0f, 8.0f, 9.0f);
		CHECK(wld.set_raw(entity, runtimeComp.entity, replacement, RuntimePayloadSize));
		CHECK(setHits == 2);
		CHECK(read_f32(setSeen, RuntimeXOffset) == doctest::Approx(7.0f));
		CHECK(read_f32(setSeen, RuntimeYOffset) == doctest::Approx(8.0f));
		CHECK(read_f32(setSeen, RuntimeZOffset) == doctest::Approx(9.0f));

		CHECK_FALSE(wld.add_raw(entity, runtimeComp.entity, replacement, RuntimePayloadSize - 1));
	}

	SUBCASE("runtime raw payload access supports tags") {
		TestWorld twld;

		const auto& tagComp = add_runtime_component(wld, "Runtime_Component_Raw_Tag", 0, ecs::DataStorageType::Table, 1);
		const auto entity = wld.add();

		CHECK(wld.add_raw(entity, tagComp.entity, nullptr, 0));
		const auto payload = wld.get_raw(entity, tagComp.entity);
		CHECK(payload.valid());
		CHECK(payload.data == nullptr);
		CHECK(payload.size == 0);

		const auto mutablePayload = wld.mut_raw(entity, tagComp.entity);
		CHECK(mutablePayload.valid());
		CHECK(mutablePayload.data == nullptr);
		CHECK(mutablePayload.size == 0);
		CHECK(wld.set_raw(entity, tagComp.entity, nullptr, 0));
	}

	SUBCASE("runtime component cursor walks fields and mutates strict primitives") {
		TestWorld twld;

		const auto& runtimeComp = add_runtime_component(
				wld, "Runtime_Component_Cursor", RuntimePayloadSize, ecs::DataStorageType::Table, RuntimePayloadAlign);
		CHECK(wld.comp_cache_mut().add_field(runtimeComp.entity, {util::str_view("x"), ecs::F32, RuntimeXOffset, 0}));
		CHECK(wld.comp_cache_mut().add_field(runtimeComp.entity, {util::str_view("y"), ecs::F32, RuntimeYOffset, 0}));
		CHECK(wld.comp_cache_mut().add_field(runtimeComp.entity, {util::str_view("z"), ecs::F32, RuntimeZOffset, 0}));

		const auto entity = wld.add();
		uint8_t initial[RuntimePayloadSize]{};
		write_xyz(initial, 1.0f, 2.0f, 3.0f);
		CHECK(wld.add_raw(entity, runtimeComp.entity, initial, RuntimePayloadSize));

		auto cursor = wld.cursor(entity, runtimeComp.entity);
		CHECK(cursor.valid());
		CHECK(cursor.type() == runtimeComp.entity);
		CHECK(cursor.size() == RuntimePayloadSize);
		CHECK(cursor.field_count() == 3);
		CHECK(cursor.ptr() != nullptr);
		CHECK(cursor.mut_ptr() == nullptr);
		CHECK_FALSE(cursor.get_f32(nullptr));
		const float readOnlyWrite = 10.0f;
		CHECK(cursor.write_bytes(&readOnlyWrite, sizeof(readOnlyWrite)) == ecs::ComponentWriteResult::ReadOnly);

		CHECK(cursor.field(util::str_view("y")));
		CHECK(cursor.valid());
		CHECK(cursor.depth() == 1);
		CHECK(cursor.type() == ecs::F32);
		CHECK(cursor.size() == sizeof(float));
		float y = 0.0f;
		CHECK(cursor.get_f32(&y));
		CHECK(y == doctest::Approx(2.0f));
		const float readOnlyFieldWrite = 20.0f;
		CHECK(cursor.write_bytes(&readOnlyFieldWrite, sizeof(readOnlyFieldWrite)) == ecs::ComponentWriteResult::ReadOnly);
		CHECK(cursor.parent());
		CHECK(cursor.depth() == 0);

		CHECK(cursor.field(2));
		float z = 0.0f;
		CHECK(cursor.get_f32(&z));
		CHECK(z == doctest::Approx(3.0f));

		uint32_t setHits = 0;
		uint8_t setSeen[RuntimePayloadSize]{};
		const auto onSet = wld.observer()
													 .event(ecs::ObserverEvent::OnSet)
													 .all(runtimeComp.entity)
													 .on_each([&](ecs::Entity observed) {
														 CHECK(observed == entity);
														 ++setHits;
														 const auto observedPayload = wld.get_raw(observed, runtimeComp.entity);
														 CHECK(observedPayload.valid());
														 CHECK(observedPayload.size == RuntimePayloadSize);
														 if (observedPayload.data != nullptr)
															 memcpy(setSeen, observedPayload.data, RuntimePayloadSize);
													 })
													 .entity();
		(void)onSet;

		auto mutableCursor = wld.cursor_mut(entity, runtimeComp.entity);
		CHECK(mutableCursor.valid());
		CHECK(mutableCursor.field(util::str_view("x")));
		float xWrite = 42.0f;
		CHECK(mutableCursor.write_bytes(&xWrite, sizeof(xWrite)) == ecs::ComponentWriteResult::Ok);
		CHECK(setHits == 1);
		float x = 0.0f;
		CHECK(mutableCursor.get_f32(&x));
		CHECK(x == doctest::Approx(42.0f));
		CHECK(read_f32(setSeen, RuntimeXOffset) == doctest::Approx(42.0f));

		xWrite = 17.0f;
		CHECK(mutableCursor.write_bytes(&xWrite, sizeof(xWrite)) == ecs::ComponentWriteResult::Ok);
		CHECK(setHits == 2);
		CHECK(mutableCursor.get_f32(&x));
		CHECK(x == doctest::Approx(17.0f));

		xWrite = 23.0f;
		CHECK(mutableCursor.write_bytes(&xWrite, sizeof(xWrite)) == ecs::ComponentWriteResult::Ok);
		CHECK(setHits == 3);
		CHECK(mutableCursor.get_f32(&x));
		CHECK(x == doctest::Approx(23.0f));

		const uint8_t tooSmall[sizeof(float) - 1]{};
		CHECK(mutableCursor.write_bytes(tooSmall, sizeof(tooSmall)) == ecs::ComponentWriteResult::OutOfRange);
		CHECK(setHits == 3);
	}

	SUBCASE("runtime raw payload access rejects unsupported storage") {
		TestWorld twld;

		const auto& sparseComp =
				add_runtime_component(wld, "Runtime_Component_Raw_Sparse", 4, ecs::DataStorageType::Sparse, 4);
		const auto entity = wld.add();
		uint32_t value = 42;

		CHECK_FALSE(wld.add_raw(entity, sparseComp.entity, &value, sizeof(value)));
		CHECK_FALSE(wld.get_raw(entity, sparseComp.entity).valid());
		CHECK_FALSE(wld.mut_raw(entity, sparseComp.entity).valid());
		CHECK_FALSE(wld.set_raw(entity, sparseComp.entity, &value, sizeof(value)));
	}
}

TEST_CASE("ArchetypeGraph") {
	TestWorld twld;

	const auto rel = wld.add();
	const auto tgt = wld.add();
	const auto ent = wld.add();
	const auto pairEnt = (ecs::Entity)ecs::Pair(rel, tgt);

	wld.name(rel, "Rel");
	wld.name(tgt, "Tgt");
	wld.name(ent, "Entity");

	ecs::ArchetypeGraph graph;
	CHECK(graph.find_edge_right(ent) == ecs::ArchetypeIdHashPairBad);
	CHECK(graph.find_edge_left(pairEnt) == ecs::ArchetypeIdHashPairBad);

	graph.add_edge_right(ent, 17, {101});
	graph.add_edge_left(pairEnt, 23, {202});
	graph.add_edge_right(ent, 17, {101});
	graph.add_edge_left(pairEnt, 23, {202});

	{
		const auto edge = graph.find_edge_right(ent);
		CHECK(edge.id == 17);
		CHECK(edge.hash == ecs::ArchetypeIdHash{101});
	}
	{
		const auto edge = graph.find_edge_left(pairEnt);
		CHECK(edge.id == 23);
		CHECK(edge.hash == ecs::ArchetypeIdHash{202});
	}

	CHECK(graph.right_edges().size() == 1);
	CHECK(graph.left_edges().size() == 1);

	const auto& graphConst = graph;
	CHECK(graphConst.right_edges().size() == 1);
	CHECK(graphConst.left_edges().size() == 1);

	const auto logLevelBackup = util::g_logLevelMask;
	util::g_logLevelMask = 0;
	graphConst.diag(wld);
	util::g_logLevelMask = logLevelBackup;

	graph.del_edge_right(ent);
	graph.del_edge_left(pairEnt);
	CHECK(graph.find_edge_right(ent) == ecs::ArchetypeIdHashPairBad);
	CHECK(graph.find_edge_left(pairEnt) == ecs::ArchetypeIdHashPairBad);
}

TEST_CASE("EntityContainer helpers") {
	ecs::EntityContainerCtx entityCtx{true, false, ecs::EntityKind::EK_Gen};
	auto entityContainer = ecs::EntityContainer::create(7, 3, &entityCtx);
	CHECK(entityContainer.idx == 7);
	CHECK(entityContainer.data.gen == 3);
	CHECK(entityContainer.data.ent == 1);
	CHECK(entityContainer.data.pair == 0);
	CHECK(entityContainer.data.kind == (uint32_t)ecs::EntityKind::EK_Gen);
	CHECK(ecs::EntityContainer::handle(entityContainer) == ecs::Entity(7, 3, true, false, ecs::EntityKind::EK_Gen));
	CHECK(cnt::to_page_storage_id<ecs::EntityContainer>::get(entityContainer) == 7);

	entityContainer.req_del();
	CHECK((entityContainer.flags & ecs::DeleteRequested) != 0);

	ecs::EntityContainers containers;
	const auto entityHandle = containers.entities.alloc(&entityCtx);
	auto& entityRecord = containers.entities[entityHandle.id()];
	entityRecord.row = 11;

	const auto pairHandle = (ecs::Entity)ecs::Pair(ecs::ChildOf, entityHandle);
	ecs::EntityContainerCtx pairCtx{pairHandle.entity(), pairHandle.pair(), pairHandle.kind()};
	auto pairRecord = ecs::EntityContainer::create(pairHandle.id(), pairHandle.gen(), &pairCtx);
	pairRecord.row = 22;
	(void)containers.pairs.try_emplace(ecs::EntityLookupKey(pairHandle), pairRecord);

	CHECK(containers[entityHandle].row == 11);
	CHECK(containers[pairHandle].row == 22);

	const auto& containersConst = containers;
	CHECK(containersConst[entityHandle].row == 11);
	CHECK(containersConst[pairHandle].row == 22);
}

TEST_CASE("Component helpers") {
	const ecs::Component denseComp(
			11, 0, (uint32_t)sizeof(Position), (uint32_t)alignof(Position), ecs::DataStorageType::Table);
	const ecs::Component sparseAosComp(
			12, 0, (uint32_t)sizeof(Position), (uint32_t)alignof(Position), ecs::DataStorageType::Sparse);
	const ecs::Component sparseSoaComp(
			13, 2, (uint32_t)sizeof(Position), (uint32_t)alignof(Position), ecs::DataStorageType::Sparse);
	const ecs::Component tagComp(14, 0, 0, 1, ecs::DataStorageType::Table);

	CHECK(ecs::component_has_inline_data(denseComp));
	CHECK_FALSE(ecs::component_has_out_of_line_data(denseComp));
	CHECK_FALSE(ecs::component_has_inline_data(sparseAosComp));
	CHECK(ecs::component_has_out_of_line_data(sparseAosComp));
	CHECK(ecs::component_has_inline_data(sparseSoaComp));
	CHECK_FALSE(ecs::component_has_out_of_line_data(sparseSoaComp));
	CHECK_FALSE(ecs::component_has_inline_data(tagComp));
	CHECK_FALSE(ecs::component_has_out_of_line_data(tagComp));

	TestWorld twld;
	const auto pos = wld.add<Position>().entity;
	const auto rot = wld.add<Rotation>().entity;
	const auto scl = wld.add<Scale>().entity;

	cnt::sarr<ecs::Entity, 3> comps;
	comps[0] = pos;
	comps[1] = rot;
	comps[2] = scl;

	const auto hashFromSpan = ecs::calc_lookup_hash(ecs::EntitySpan{comps.data(), comps.size()});
	cnt::sarr<uint64_t, 3> compValues;
	compValues[0] = pos.value();
	compValues[1] = rot.value();
	compValues[2] = scl.value();

	const auto hashFromContainer = ecs::calc_lookup_hash(compValues);
	const auto hashContainerExpected =
			ecs::ComponentLookupHash{core::hash_combine(compValues[0], compValues[1], compValues[2])};
	const auto hashSpanExpected = ecs::ComponentLookupHash{core::hash_combine(
			core::calculate_hash64(pos.value()), core::calculate_hash64(rot.value()), core::calculate_hash64(scl.value()))};

	CHECK(hashFromContainer == hashContainerExpected);
	CHECK(hashFromSpan == hashSpanExpected);
	CHECK(ecs::calc_lookup_hash(ecs::EntitySpan{}) == ecs::ComponentLookupHash{0});
	CHECK(ecs::calc_lookup_hash<>() == ecs::ComponentLookupHash{0});
	CHECK(ecs::calc_lookup_hash<Position>() == ecs::ComponentLookupHash{meta::type_info::hash<Position>()});
	CHECK(
			ecs::calc_lookup_hash<Position, Rotation>() ==
			ecs::ComponentLookupHash{
					core::hash_combine(meta::type_info::hash<Position>(), meta::type_info::hash<Rotation>())});

	CHECK(ecs::comp_idx<3>(comps.data(), pos) == 0);
	CHECK(ecs::comp_idx<3>(comps.data(), rot) == 1);
	CHECK(ecs::comp_idx(ecs::EntitySpan{comps.data(), comps.size()}, scl) == 2);
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

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

struct ErasedPairRelationTag {};
struct ErasedPairTargetTag {};
struct ErasedPairPayload {
	float x;
	float y;
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
	const auto read_u32 = [](const void* data, uint32_t offset) {
		uint32_t value{};
		memcpy(&value, (const uint8_t*)data + offset, sizeof(value));
		return value;
	};
	const auto write_xyz = [&](void* data, float x, float y, float z) {
		write_f32(data, RuntimeXOffset, x);
		write_f32(data, RuntimeYOffset, y);
		write_f32(data, RuntimeZOffset, z);
	};
	const ecs::RuntimeFieldDesc RuntimeXYZFields[] = {
			{util::str_view("x"), ecs::F32, RuntimeXOffset, 0},
			{util::str_view("y"), ecs::F32, RuntimeYOffset, 0},
			{util::str_view("z"), ecs::F32, RuntimeZOffset, 0}};
	constexpr uint32_t RuntimeXYZFieldCount = 3;

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

	SUBCASE("opaque runtime type exposes semantic metadata and serializer capabilities") {
		TestWorld twld;

		ecs::ComponentDesc semanticDesc{};
		semanticDesc.name = util::str_view("Runtime_Type_Opaque_XYZ");
		semanticDesc.size = RuntimePayloadSize;
		semanticDesc.alig = RuntimePayloadAlign;
		semanticDesc.storageType = ecs::DataStorageType::Table;
		semanticDesc.typeKind = ecs::RuntimeTypeKind::Struct;
		semanticDesc.fields = RuntimeXYZFields;
		semanticDesc.fieldCount = RuntimeXYZFieldCount;
		auto& semanticType = wld.add(semanticDesc);

		auto save_opaque = [](ser::serializer& s, const void* data, uint32_t from, uint32_t to, uint32_t cap) {
			const auto* bytes = (const uint8_t*)data;
			GAIA_FOR2_(from, to, row) {
				s.save_raw(bytes + (uintptr_t)12 * (row % cap), 12, ser::serialization_type_id::trivial_wrapper);
			}
		};
		auto load_opaque = [](ser::serializer& s, void* data, uint32_t from, uint32_t to, uint32_t cap) {
			auto* bytes = (uint8_t*)data;
			GAIA_FOR2_(from, to, row) {
				s.load_raw(bytes + (uintptr_t)12 * (row % cap), 12, ser::serialization_type_id::trivial_wrapper);
			}
		};

		ecs::ComponentDesc desc{};
		desc.name = util::str_view("Runtime_Component_Opaque_XYZ");
		desc.size = RuntimePayloadSize;
		desc.alig = RuntimePayloadAlign;
		desc.storageType = ecs::DataStorageType::Table;
		desc.typeKind = ecs::RuntimeTypeKind::Opaque;
		desc.opaqueAsType = semanticType.entity;
		desc.funcSave = save_opaque;
		desc.funcLoad = load_opaque;
		auto& item = wld.add(desc);

		CHECK(item.typeKind == ecs::RuntimeTypeKind::Opaque);
		CHECK(item.opaque_as_type() == semanticType.entity);
		CHECK(item.field_count() == 0);
		CHECK(item.element_type() == ecs::EntityBad);
		CHECK(item.element_count() == 0);
		CHECK(item.has_custom_serializer());
		CHECK(item.has_custom_deserializer());

		uint8_t payload[RuntimePayloadSize]{};
		write_xyz(payload, 1.0f, 2.0f, 3.0f);
		ser::ser_buffer_binary buffer;
		auto writer = ser::make_serializer(buffer);
		item.save(writer, payload, 0, 1, 1);
		CHECK(buffer.bytes() == RuntimePayloadSize);

		uint8_t loaded[RuntimePayloadSize]{};
		auto reader = ser::make_serializer(buffer);
		reader.seek(0);
		item.load(reader, loaded, 0, 1, 1);
		CHECK(read_f32(loaded, RuntimeXOffset) == doctest::Approx(1.0f));
		CHECK(read_f32(loaded, RuntimeYOffset) == doctest::Approx(2.0f));
		CHECK(read_f32(loaded, RuntimeZOffset) == doctest::Approx(3.0f));

		const auto e = wld.add();
		CHECK(wld.add_raw(e, item.entity, payload, RuntimePayloadSize));
		auto cursor = wld.cursor(e, item.entity);
		CHECK(cursor.valid());
		CHECK(cursor.type() == item.entity);
		CHECK(cursor.type_kind() == ecs::RuntimeTypeKind::Opaque);
		CHECK(cursor.opaque_as_type() == semanticType.entity);
		CHECK(cursor.field_count() == 0);
		CHECK_FALSE(cursor.field(util::str_view("x")));

		ecs::ComponentDesc noSerDesc = desc;
		noSerDesc.name = util::str_view("Runtime_Component_Opaque_XYZ_NoSer");
		noSerDesc.funcSave = nullptr;
		noSerDesc.funcLoad = nullptr;
		auto& noSerItem = wld.add(noSerDesc);
		CHECK(noSerItem.typeKind == ecs::RuntimeTypeKind::Opaque);
		CHECK(noSerItem.opaque_as_type() == semanticType.entity);
		CHECK_FALSE(noSerItem.has_custom_serializer());
		CHECK_FALSE(noSerItem.has_custom_deserializer());
	}

	SUBCASE("opaque adapter projects semantic fields through cursor writes") {
		TestWorld twld;

		struct RuntimeOpaqueHandle {
			uint32_t index = 0;
		};

		struct RuntimeOpaqueStore {
			uint8_t payloads[2][RuntimePayloadSize]{};
			uint32_t projectCalls = 0;
			uint32_t commitCalls = 0;
		};

		RuntimeOpaqueStore store{};
		write_xyz(store.payloads[0], 1.0f, 2.0f, 3.0f);

		ecs::RuntimeOpaqueAdapter adapter{};
		adapter.ctx = &store;
		adapter.project = [](void* ctx, const ecs::RuntimeOpaqueScope& opaque, ecs::RuntimeOpaqueValue& outValue) {
			auto& opaqueStore = *(RuntimeOpaqueStore*)ctx;
			++opaqueStore.projectCalls;
			const auto* handle = (const RuntimeOpaqueHandle*)opaque.data;
			if (handle == nullptr || handle->index >= 2)
				return false;
			outValue.data = opaqueStore.payloads[handle->index];
			if (opaque.mutData != nullptr)
				outValue.mutData = opaqueStore.payloads[handle->index];
			outValue.size = 12U;
			return true;
		};
		adapter.commit = [](void* ctx, ecs::RuntimeOpaqueScope&, ecs::RuntimeOpaqueValue&) {
			auto& opaqueStore = *(RuntimeOpaqueStore*)ctx;
			++opaqueStore.commitCalls;
			return true;
		};

		ecs::ComponentDesc semanticDesc{};
		semanticDesc.name = util::str_view("Runtime_Type_Opaque_Adapter_XYZ");
		semanticDesc.size = RuntimePayloadSize;
		semanticDesc.alig = RuntimePayloadAlign;
		semanticDesc.storageType = ecs::DataStorageType::Table;
		semanticDesc.typeKind = ecs::RuntimeTypeKind::Struct;
		semanticDesc.fields = RuntimeXYZFields;
		semanticDesc.fieldCount = RuntimeXYZFieldCount;
		auto& semanticType = wld.add(semanticDesc);

		ecs::ComponentDesc opaqueDesc{};
		opaqueDesc.name = util::str_view("Runtime_Component_Opaque_Adapter_Handle");
		opaqueDesc.size = sizeof(RuntimeOpaqueHandle);
		opaqueDesc.alig = alignof(RuntimeOpaqueHandle);
		opaqueDesc.storageType = ecs::DataStorageType::Table;
		opaqueDesc.typeKind = ecs::RuntimeTypeKind::Opaque;
		opaqueDesc.opaqueAsType = semanticType.entity;
		opaqueDesc.opaqueAdapter = &adapter;
		auto& opaqueType = wld.add(opaqueDesc);

		CHECK(opaqueType.opaque_adapter() == &adapter);

		const auto entity = wld.add();
		RuntimeOpaqueHandle handle{0};
		CHECK(wld.add_raw(entity, opaqueType.entity, &handle, sizeof(handle)));

		auto cursor = wld.cursor(entity, opaqueType.entity);
		CHECK(cursor.valid());
		CHECK(cursor.type_kind() == ecs::RuntimeTypeKind::Opaque);
		CHECK(cursor.opaque_as_type() == semanticType.entity);
		CHECK(cursor.field_count() == 3);
		CHECK(cursor.field(util::str_view("y")));
		const auto y = cursor.f32();
		CHECK(y);
		CHECK(y.value == doctest::Approx(2.0f));

		uint32_t setHits = 0;
		const auto onSet = wld.observer()
													 .event(ecs::ObserverEvent::OnSet)
													 .all(opaqueType.entity)
													 .on_each([&](ecs::Entity observed) {
														 CHECK(observed == entity);
														 ++setHits;
													 })
													 .entity();
		(void)onSet;

		auto mutableCursor = wld.cursor_mut(entity, opaqueType.entity);
		CHECK(mutableCursor.field(util::str_view("z")));
		CHECK(mutableCursor.f32(42.0f));
		CHECK(store.commitCalls == 1);
		CHECK(setHits == 1);
		CHECK(read_f32(store.payloads[0], RuntimeZOffset) == doctest::Approx(42.0f));

		bool jsonOk = false;
		const auto json = ecs::component_to_json(opaqueType, &handle, jsonOk);
		CHECK(jsonOk);
		CHECK(strstr(json.data(), "\"x\":1") != nullptr);
		CHECK(strstr(json.data(), "\"z\":42") != nullptr);

		RuntimeOpaqueHandle loadedHandle{1};
		ser::ser_json reader("{\"x\":7,\"y\":8,\"z\":9}");
		bool readOk = false;
		CHECK(ecs::json_to_component(opaqueType, &loadedHandle, reader, readOk));
		CHECK(readOk);
		CHECK(read_f32(store.payloads[1], RuntimeXOffset) == doctest::Approx(7.0f));
		CHECK(read_f32(store.payloads[1], RuntimeZOffset) == doctest::Approx(9.0f));
	}

	SUBCASE("dynamic vector runtime type exposes element metadata without inline traversal") {
		TestWorld twld;

		ecs::ComponentDesc elementDesc{};
		elementDesc.name = util::str_view("Runtime_Type_Vector_XYZ_Element");
		elementDesc.size = RuntimePayloadSize;
		elementDesc.alig = RuntimePayloadAlign;
		elementDesc.storageType = ecs::DataStorageType::Table;
		elementDesc.typeKind = ecs::RuntimeTypeKind::Struct;
		elementDesc.fields = RuntimeXYZFields;
		elementDesc.fieldCount = RuntimeXYZFieldCount;
		auto& elementType = wld.add(elementDesc);

		ecs::ComponentDesc vectorDesc{};
		vectorDesc.name = util::str_view("Runtime_Type_Vector_XYZ");
		vectorDesc.size = 0;
		vectorDesc.alig = 1;
		vectorDesc.storageType = ecs::DataStorageType::Table;
		vectorDesc.typeKind = ecs::RuntimeTypeKind::Vector;
		vectorDesc.elementType = elementType.entity;
		auto& vectorType = wld.add(vectorDesc);

		CHECK(vectorType.typeKind == ecs::RuntimeTypeKind::Vector);
		CHECK(vectorType.element_type() == elementType.entity);
		CHECK(vectorType.element_count() == 0);
		CHECK(vectorType.field_count() == 0);

		const auto e = wld.add();
		CHECK(wld.add_raw(e, vectorType.entity, nullptr, 0));
		auto cursor = wld.cursor(e, vectorType.entity);
		CHECK(cursor.valid());
		CHECK(cursor.type_kind() == ecs::RuntimeTypeKind::Vector);
		CHECK(cursor.element_type() == elementType.entity);
		CHECK(cursor.count().status == ecs::CursorStatus::TypeMismatch);
		CHECK(cursor.resize(1).status == ecs::CursorStatus::TypeMismatch);
		CHECK_FALSE(cursor.elem(0));
		CHECK_FALSE(cursor.field(util::str_view("x")));
	}

	SUBCASE("dynamic vector adapter projects cursor elements and semantic JSON") {
		TestWorld twld;

		struct RuntimeVectorHeader {
			uint32_t count = 0;
			uint32_t capacity = 0;
			uint8_t* data = nullptr;
		};

		struct RuntimeVectorStats {
			uint32_t countCalls = 0;
			uint32_t elementCalls = 0;
			uint32_t resizeCalls = 0;
			uint32_t commitCalls = 0;
		};

		RuntimeVectorStats stats{};
		ecs::RuntimeSequenceAdapter adapter{};
		adapter.ctx = &stats;
		adapter.count = [](void* ctx, const ecs::RuntimeSequenceScope& sequence, uint32_t& outCount) {
			auto& adapterStats = *(RuntimeVectorStats*)ctx;
			++adapterStats.countCalls;
			const auto* header = (const RuntimeVectorHeader*)sequence.data;
			if (header == nullptr || header->count > header->capacity)
				return false;
			outCount = header->count;
			return true;
		};
		adapter.element = [](void* ctx, const ecs::RuntimeSequenceScope& sequence, uint32_t index,
												 ecs::RuntimeSequenceElement& outElement) {
			auto& adapterStats = *(RuntimeVectorStats*)ctx;
			++adapterStats.elementCalls;
			const auto* header = (const RuntimeVectorHeader*)sequence.data;
			if (header == nullptr || header->data == nullptr || index >= header->count)
				return false;
			outElement.data = header->data + (uintptr_t)12U * index;
			if (sequence.mutData != nullptr)
				outElement.mutData = header->data + (uintptr_t)12U * index;
			outElement.size = 12U;
			return true;
		};
		adapter.resize = [](void* ctx, ecs::RuntimeSequenceScope& sequence, uint32_t count) {
			auto& adapterStats = *(RuntimeVectorStats*)ctx;
			++adapterStats.resizeCalls;
			auto* header = (RuntimeVectorHeader*)sequence.mutData;
			if (header == nullptr || count > header->capacity)
				return false;
			header->count = count;
			return true;
		};
		adapter.commitElement = [](void* ctx, ecs::RuntimeSequenceScope&, ecs::RuntimeSequenceElement&) {
			auto& adapterStats = *(RuntimeVectorStats*)ctx;
			++adapterStats.commitCalls;
			return true;
		};

		ecs::ComponentDesc elementDesc{};
		elementDesc.name = util::str_view("Runtime_Type_Vector_Adapter_XYZ_Element");
		elementDesc.size = RuntimePayloadSize;
		elementDesc.alig = RuntimePayloadAlign;
		elementDesc.storageType = ecs::DataStorageType::Table;
		elementDesc.typeKind = ecs::RuntimeTypeKind::Struct;
		elementDesc.fields = RuntimeXYZFields;
		elementDesc.fieldCount = RuntimeXYZFieldCount;
		auto& elementType = wld.add(elementDesc);

		ecs::ComponentDesc vectorDesc{};
		vectorDesc.name = util::str_view("Runtime_Type_Vector_Adapter_XYZ");
		vectorDesc.size = sizeof(RuntimeVectorHeader);
		vectorDesc.alig = alignof(RuntimeVectorHeader);
		vectorDesc.storageType = ecs::DataStorageType::Table;
		vectorDesc.typeKind = ecs::RuntimeTypeKind::Vector;
		vectorDesc.elementType = elementType.entity;
		vectorDesc.sequenceAdapter = &adapter;
		auto& vectorType = wld.add(vectorDesc);

		const ecs::RuntimeFieldDesc containerFields[] = {{util::str_view("points"), vectorType.entity, 0, 0}};
		ecs::ComponentDesc containerDesc{};
		containerDesc.name = util::str_view("Runtime_Component_Vector_Adapter_Container");
		containerDesc.size = sizeof(RuntimeVectorHeader);
		containerDesc.alig = alignof(RuntimeVectorHeader);
		containerDesc.storageType = ecs::DataStorageType::Table;
		containerDesc.typeKind = ecs::RuntimeTypeKind::Struct;
		containerDesc.fields = containerFields;
		containerDesc.fieldCount = 1;
		auto& containerType = wld.add(containerDesc);

		uint8_t elements[RuntimePayloadSize * 3]{};
		write_xyz(elements, 1.0f, 2.0f, 3.0f);
		write_xyz(elements + RuntimePayloadSize, 4.0f, 5.0f, 6.0f);
		RuntimeVectorHeader header{2, 3, elements};

		const auto entity = wld.add();
		CHECK(wld.add_raw(entity, containerType.entity, &header, sizeof(header)));

		auto cursor = wld.cursor(entity, containerType.entity);
		CHECK(cursor.field(util::str_view("points")));
		const auto count = cursor.count();
		CHECK(count);
		CHECK(count.value == 2);
		CHECK(cursor.elem(1));
		CHECK(cursor.type() == elementType.entity);
		CHECK(cursor.field(util::str_view("y")));
		const auto y = cursor.f32();
		CHECK(y);
		CHECK(y.value == doctest::Approx(5.0f));

		uint32_t setHits = 0;
		const auto onSet = wld.observer()
													 .event(ecs::ObserverEvent::OnSet)
													 .all(containerType.entity)
													 .on_each([&](ecs::Entity observed) {
														 CHECK(observed == entity);
														 ++setHits;
													 })
													 .entity();
		(void)onSet;

		auto mutableCursor = wld.cursor_mut(entity, containerType.entity);
		CHECK(mutableCursor.field(util::str_view("points")));
		CHECK(mutableCursor.elem(0));
		CHECK(mutableCursor.field(util::str_view("x")));
		CHECK(mutableCursor.f32(42.0f));
		CHECK(stats.commitCalls == 1);
		CHECK(setHits == 1);
		CHECK(read_f32(elements, RuntimeXOffset) == doctest::Approx(42.0f));

		bool jsonOk = false;
		const auto json = ecs::component_to_json(containerType, &header, jsonOk);
		CHECK(jsonOk);
		CHECK(strstr(json.data(), "\"points\"") != nullptr);
		CHECK(strstr(json.data(), "42") != nullptr);
		CHECK(strstr(json.data(), "5") != nullptr);

		uint8_t loadedElements[RuntimePayloadSize * 3]{};
		RuntimeVectorHeader loadedHeader{0, 3, loadedElements};
		ser::ser_json reader("{\"points\":[{\"x\":7,\"y\":8,\"z\":9},{\"x\":10,\"y\":11,\"z\":12}]}");
		bool readOk = false;
		CHECK(ecs::json_to_component(containerType, &loadedHeader, reader, readOk));
		CHECK(readOk);
		CHECK(loadedHeader.count == 2);
		CHECK(read_f32(loadedElements, RuntimeXOffset) == doctest::Approx(7.0f));
		CHECK(read_f32(loadedElements + RuntimePayloadSize, RuntimeZOffset) == doctest::Approx(12.0f));
		CHECK(stats.resizeCalls >= 1);
	}

	SUBCASE("world descriptor registration creates a usable runtime component") {
		TestWorld twld;

		constexpr const char* RuntimeCompName = "Runtime_Component_World_Desc";
		const auto nameLen = (uint32_t)GAIA_STRLEN(RuntimeCompName, ecs::ComponentCacheItem::MaxNameLength);

		ecs::ComponentDesc desc{};
		desc.name = util::str_view(RuntimeCompName, nameLen);
		desc.size = RuntimePayloadSize;
		desc.alig = RuntimePayloadAlign;
		desc.storageType = ecs::DataStorageType::Table;
		desc.fields = RuntimeXYZFields;
		desc.fieldCount = 1;

		auto& item = wld.add(desc);
		CHECK(item.entity != ecs::EntityBad);
		CHECK(item.comp.id() == item.entity.id());
		CHECK(item.comp.size() == RuntimePayloadSize);
		CHECK(item.comp.alig() == RuntimePayloadAlign);
		CHECK(item.comp.storage_type() == ecs::DataStorageType::Table);
		CHECK(item.name.len() == nameLen);
		CHECK(strcmp(item.name.str(), RuntimeCompName) == 0);
		CHECK(wld.comp_cache().find(item.entity) == &item);
		CHECK(wld.symbol(RuntimeCompName) == item.entity);
		CHECK(wld.get(RuntimeCompName) == item.entity);

		CHECK(item.field_count() == 1);

		const ecs::RuntimeField* field = item.field(0);
		CHECK(field != nullptr);
		CHECK(strcmp(field->name, "x") == 0);
		CHECK(field->type == ecs::F32);
		CHECK(field->offset == RuntimeXOffset);
		CHECK(field->count == 0);

		field = item.field(util::str_view("x"));
		CHECK(field != nullptr);
		CHECK(field->offset == RuntimeXOffset);

		CHECK(item.field(1) == nullptr);

		const auto entity = wld.add();
		uint8_t initial[RuntimePayloadSize]{};
		write_f32(initial, RuntimeXOffset, 1.0f);
		CHECK(wld.add_raw(entity, item.entity, initial, RuntimePayloadSize));

		auto cursor = wld.cursor_mut(entity, item.entity);
		CHECK(cursor.valid());
		CHECK(cursor.field("x"));
		CHECK(cursor.f32(2.0f));

		const auto payload = wld.get_raw(entity, item.entity);
		CHECK(payload.valid());
		if (payload.valid())
			CHECK(read_f32(payload.data, RuntimeXOffset) == doctest::Approx(2.0f));

		const auto& duplicate = wld.add(desc);
		CHECK(&duplicate == &item);
		CHECK(duplicate.entity == item.entity);
	}

	SUBCASE("reflected primitive ids and runtime fields are available") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		ser::serialization_type_id serType = ser::serialization_type_id::ignore;
		CHECK(ecs::S8 == ecs::runtime_primitive_type_entity(ser::serialization_type_id::s8));
		CHECK(ecs::S16 == ecs::runtime_primitive_type_entity(ser::serialization_type_id::s16));
		CHECK(ecs::S32 == ecs::runtime_primitive_type_entity(ser::serialization_type_id::s32));
		CHECK(ecs::S64 == ecs::runtime_primitive_type_entity(ser::serialization_type_id::s64));
		CHECK(ecs::Bool == ecs::runtime_primitive_type_entity(ser::serialization_type_id::b));
		CHECK(ecs::Char8 == ecs::runtime_primitive_type_entity(ser::serialization_type_id::c8));
		CHECK(ecs::Char16 == ecs::runtime_primitive_type_entity(ser::serialization_type_id::c16));
		CHECK(ecs::Char32 == ecs::runtime_primitive_type_entity(ser::serialization_type_id::c32));
		CHECK(ser::serialization_type_size(ser::serialization_type_id::trivial_wrapper, 4) == 4);
		CHECK(ser::serialization_type_size(ser::serialization_type_id::data_and_size, 0) == sizeof(uintptr_t));
		CHECK(ecs::F8 == ecs::runtime_primitive_type_entity(ser::serialization_type_id::f8));
		CHECK(ser::serialization_type_size(ser::serialization_type_id::f8, 0) == 1);
		CHECK(ecs::F16 == ecs::runtime_primitive_type_entity(ser::serialization_type_id::f16));
		CHECK(ecs::F32 == ecs::runtime_primitive_type_entity(ser::serialization_type_id::f32));
		CHECK(ecs::F64 == ecs::runtime_primitive_type_entity(ser::serialization_type_id::f64));
		CHECK(ecs::runtime_primitive_serialization_type(ecs::F32, serType));
		CHECK(serType == ser::serialization_type_id::f32);
		CHECK(ecs::runtime_primitive_type_entity(ser::serialization_type_id::trivial_wrapper) == ecs::EntityBad);

		const auto* f32Info = cc.find(ecs::F32);
		CHECK(f32Info != nullptr);
		if (f32Info != nullptr) {
			CHECK(f32Info->typeKind == ecs::RuntimeTypeKind::Primitive);
			CHECK(f32Info->primitive_type() == ecs::F32);
			CHECK(f32Info->comp.size() == 4);
			CHECK(f32Info->comp.alig() == 4);
		}

		constexpr const char* RuntimeCompName = "Runtime_Component_Fields";
		const auto entity = wld.add();

		const ecs::RuntimeFieldDesc fields[] = {
				{util::str_view("x"), ecs::F32, 0, 0}, //
				{util::str_view("color"), ecs::F32, 8, 4} //
		};

		ecs::ComponentDesc desc{};
		desc.name = runtime_component_name_view(RuntimeCompName);
		desc.size = 24;
		desc.alig = 4;
		desc.typeKind = ecs::RuntimeTypeKind::Struct;
		desc.fields = fields;
		desc.fieldCount = 2;

		auto& item = const_cast<ecs::ComponentCacheItem&>(cc.add(entity, desc));
		CHECK(item.typeKind == ecs::RuntimeTypeKind::Struct);
		CHECK(item.primitive_type() == ecs::EntityBad);
		CHECK(item.field_count() == 2);

		const ecs::RuntimeField* field = item.field(util::str_view("x"));
		CHECK(field != nullptr);
		if (field != nullptr) {
			CHECK(field->type == ecs::F32);
			CHECK(field->offset == 0);
			CHECK(field->count == 0);
			CHECK(item.field_element_count(*field) == 1);
		}

		field = item.field(util::str_view("color"));
		CHECK(field != nullptr);
		if (field != nullptr) {
			CHECK(field->offset == 8);
			CHECK(field->count == 4);
			CHECK(item.field_element_count(*field) == 4);
		}
	}

	SUBCASE("runtime enum and bitmask metadata is descriptor owned") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		const ecs::RuntimeConstantDesc movementConstants[] = {
				{util::str_view("Idle"), 0}, //
				{util::str_view("Walk"), 1}, //
				{util::str_view("Run"), 2} //
		};

		ecs::ComponentDesc movementDesc{};
		movementDesc.name = runtime_component_name_view("Runtime_Type_MovementMode");
		movementDesc.size = 4;
		movementDesc.alig = 4;
		movementDesc.typeKind = ecs::RuntimeTypeKind::Enum;
		movementDesc.underlyingType = ecs::U32;
		movementDesc.constants = movementConstants;
		movementDesc.constantCount = 3;
		auto& movementType = cc.add(wld.add(), movementDesc);

		CHECK(movementType.typeKind == ecs::RuntimeTypeKind::Enum);
		CHECK(movementType.primitive_type() == ecs::U32);
		CHECK(movementType.constant_count() == 3);
		const ecs::RuntimeConstant* walk = movementType.constant(util::str_view("Walk"));
		CHECK(walk != nullptr);
		if (walk != nullptr) {
			CHECK(strcmp(walk->name, "Walk") == 0);
			CHECK(walk->value == 1);
		}
		const ecs::RuntimeConstant* run = movementType.constant_by_value(2);
		CHECK(run != nullptr);
		if (run != nullptr)
			CHECK(strcmp(run->name, "Run") == 0);
		CHECK(movementType.constant(util::str_view("Fly")) == nullptr);
		CHECK(movementType.constant_by_value(3) == nullptr);

		const ecs::RuntimeConstantDesc collisionConstants[] = {
				{util::str_view("Static"), 1}, //
				{util::str_view("Dynamic"), 2}, //
				{util::str_view("Trigger"), 4} //
		};

		ecs::ComponentDesc collisionDesc{};
		collisionDesc.name = runtime_component_name_view("Runtime_Type_CollisionMask");
		collisionDesc.size = 4;
		collisionDesc.alig = 4;
		collisionDesc.typeKind = ecs::RuntimeTypeKind::Bitmask;
		collisionDesc.underlyingType = ecs::U32;
		collisionDesc.constants = collisionConstants;
		collisionDesc.constantCount = 3;
		auto& collisionType = cc.add(wld.add(), collisionDesc);

		CHECK(collisionType.typeKind == ecs::RuntimeTypeKind::Bitmask);
		CHECK(collisionType.primitive_type() == ecs::U32);
		CHECK(collisionType.constant_count() == 3);
		const ecs::RuntimeConstant* trigger = collisionType.constant(util::str_view("Trigger"));
		CHECK(trigger != nullptr);
		if (trigger != nullptr)
			CHECK(trigger->value == 4);
		CHECK(collisionType.constant_by_value(2)->value == 2);

		const ecs::RuntimeFieldDesc actorFields[] = {
				{util::str_view("movement"), movementType.entity, 0, 0}, //
				{util::str_view("collision"), collisionType.entity, 4, 0} //
		};

		ecs::ComponentDesc actorDesc{};
		actorDesc.name = runtime_component_name_view("Runtime_Component_ActorFlags");
		actorDesc.size = 8;
		actorDesc.alig = 4;
		actorDesc.typeKind = ecs::RuntimeTypeKind::Struct;
		actorDesc.fields = actorFields;
		actorDesc.fieldCount = 2;
		auto& actorComp = cc.add(wld.add(), actorDesc);

		uint32_t payload[] = {2, 1 | 4};
		const auto entity = wld.add();
		CHECK(wld.add_raw(entity, actorComp.entity, payload, sizeof(payload)));

		auto cursor = wld.cursor_mut(entity, actorComp.entity);
		CHECK(cursor.field(util::str_view("movement")));
		CHECK(cursor.type() == movementType.entity);
		const auto movement = cursor.u32();
		CHECK(movement);
		CHECK(movement.value == 2);
		CHECK(cursor.u32(1));

		CHECK(cursor.parent());
		CHECK(cursor.field(util::str_view("collision")));
		CHECK(cursor.type() == collisionType.entity);
		const auto collision = cursor.u32();
		CHECK(collision);
		CHECK(collision.value == (1 | 4));
		CHECK(cursor.u32(2 | 4));

		const auto finalPayload = wld.get_raw(entity, actorComp.entity);
		CHECK(finalPayload.valid());
		CHECK(finalPayload.data != nullptr);
		CHECK(read_u32(finalPayload.data, 0) == 1);
		CHECK(read_u32(finalPayload.data, 4) == (2 | 4));
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

	SUBCASE("runtime SoA fields expose non-contiguous views and cursors") {
		TestWorld twld;
		constexpr uint8_t SoaSizes[] = {sizeof(uint32_t), sizeof(double)};
		const ecs::RuntimeFieldDesc fields[] = {
				{util::str_view("health"), ecs::U32, 0, 0}, //
				{util::str_view("score"), ecs::F64, 8, 0} //
		};

		ecs::ComponentDesc desc{};
		desc.name = util::str_view("Runtime_Component_SoA_Fields");
		desc.size = 16;
		desc.alig = 8;
		desc.soa = 2;
		desc.pSoaSizes = SoaSizes;
		desc.fields = fields;
		desc.fieldCount = 2;
		auto& runtimeComp = wld.add(desc);

		auto entity = ecs::EntityBad;
		uint32_t setHits = 0;
		const auto onSet = wld.observer()
													 .event(ecs::ObserverEvent::OnSet)
													 .all(runtimeComp.entity)
													 .on_each([&](ecs::Entity observed) {
														 CHECK(observed == entity);
														 ++setHits;
													 })
													 .entity();
		(void)onSet;

		entity = wld.add();
		wld.add(entity, runtimeComp.entity);
		CHECK_FALSE(wld.get_raw(entity, runtimeComp.entity).valid());
		CHECK_FALSE(wld.mut_raw(entity, runtimeComp.entity).valid());

		auto health = wld.mut_raw_field(entity, runtimeComp.entity, 0);
		CHECK(health.valid());
		CHECK(health.size == sizeof(uint32_t));
		const uint32_t initialHealth = 42;
		memcpy(health.data, &initialHealth, sizeof(initialHealth));
		wld.modify_raw(entity, runtimeComp.entity);
		CHECK(setHits == 1);

		auto mutableCursor = wld.cursor_mut(entity, runtimeComp.entity);
		CHECK(mutableCursor.valid());
		CHECK(mutableCursor.ptr() == nullptr);
		CHECK(mutableCursor.field_count() == 2);
		uint8_t rootPayload[16]{};
		CHECK(mutableCursor.set_raw(rootPayload, sizeof(rootPayload)).status == ecs::CursorStatus::Invalid);
		CHECK(mutableCursor.field(util::str_view("score")));
		CHECK(mutableCursor.f64(3.5));
		CHECK(setHits == 2);

		const auto healthView = wld.get_raw_field(entity, runtimeComp.entity, 0);
		CHECK(healthView.valid());
		CHECK(healthView.size == sizeof(uint32_t));
		CHECK(*(const uint32_t*)healthView.data == initialHealth);
		const auto scoreView = wld.get_raw_field(entity, runtimeComp.entity, 1);
		CHECK(scoreView.valid());
		CHECK(scoreView.size == sizeof(double));
		CHECK(*(const double*)scoreView.data == doctest::Approx(3.5));
		CHECK_FALSE(wld.get_raw_field(entity, runtimeComp.entity, 2).valid());
		CHECK(wld.acc(entity).get_raw_field(runtimeComp.entity, 0).data == healthView.data);
		CHECK(wld.acc_mut(entity).mut_raw_field(runtimeComp.entity, 1).data == scoreView.data);

		wld.add(runtimeComp.entity, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
		const auto inherited = wld.add();
		wld.as(inherited, entity);
		CHECK(wld.get_raw_field(inherited, runtimeComp.entity, 0).data == healthView.data);
		auto inheritedCursor = wld.cursor(inherited, runtimeComp.entity);
		CHECK(inheritedCursor.valid());
		CHECK(inheritedCursor.field(1));
		CHECK(inheritedCursor.f64().value == doctest::Approx(3.5));
		CHECK_FALSE(wld.mut_raw_field(inherited, runtimeComp.entity, 0).valid());
		CHECK_FALSE(wld.cursor_mut(inherited, runtimeComp.entity).valid());

		const auto readCursor = wld.cursor(entity, runtimeComp.entity);
		CHECK(readCursor.valid());
		CHECK(readCursor.ptr() == nullptr);
		auto scoreCursor = readCursor;
		CHECK(scoreCursor.field(1));
		CHECK(scoreCursor.f64().value == doctest::Approx(3.5));

		const auto copy = wld.copy(entity);
		CHECK(*(const uint32_t*)wld.get_raw_field(copy, runtimeComp.entity, 0).data == initialHealth);
		CHECK(*(const double*)wld.get_raw_field(copy, runtimeComp.entity, 1).data == doctest::Approx(3.5));

		wld.add<Position>(entity, {1.0f, 2.0f, 3.0f});
		CHECK(*(const uint32_t*)wld.get_raw_field(entity, runtimeComp.entity, 0).data == initialHealth);
		CHECK(*(const double*)wld.get_raw_field(entity, runtimeComp.entity, 1).data == doctest::Approx(3.5));
	}

	SUBCASE("runtime SoA fields expose contiguous iterator field views") {
		TestWorld twld;
		constexpr uint8_t SoaSizes[] = {sizeof(uint32_t), sizeof(double)};
		const ecs::RuntimeFieldDesc fields[] = {
				{util::str_view("health"), ecs::U32, 0, 0}, //
				{util::str_view("score"), ecs::F64, 8, 0} //
		};

		ecs::ComponentDesc desc{};
		desc.name = util::str_view("Runtime_Component_SoA_Iter_Fields");
		desc.size = 16;
		desc.alig = 8;
		desc.soa = 2;
		desc.pSoaSizes = SoaSizes;
		desc.fields = fields;
		desc.fieldCount = 2;
		auto& runtimeComp = wld.add(desc);

		const auto entityA = wld.add();
		const auto entityB = wld.add();
		wld.add(entityA, runtimeComp.entity);
		wld.add(entityB, runtimeComp.entity);

		*(uint32_t*)wld.mut_raw_field(entityA, runtimeComp.entity, 0).data = 11;
		*(uint32_t*)wld.mut_raw_field(entityB, runtimeComp.entity, 0).data = 22;
		*(double*)wld.mut_raw_field(entityA, runtimeComp.entity, 1).data = 1.5;
		*(double*)wld.mut_raw_field(entityB, runtimeComp.entity, 1).data = 2.5;

		auto q = wld.query().all(runtimeComp.entity);
		wld.enable(entityA, false);
		uint32_t enabledRows = 0;
		q.each(
				[&](ecs::Iter& it) {
					const auto health = it.view_raw_field(0, 0);
					CHECK(health.size() == 1);
					CHECK(*(const uint32_t*)health[0].data == 22);
					enabledRows += it.size();
				},
				ecs::Constraints::EnabledOnly);
		CHECK(enabledRows == 1);
		wld.enable(entityA, true);

		wld.add(runtimeComp.entity, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
		const auto inherited = wld.add();
		wld.as(inherited, entityA);

		uint32_t directRows = 0;
		uint32_t inheritedBatches = 0;
		q.each([&](ecs::Iter& it) {
			const auto entities = it.view<ecs::Entity>();
			if (entities[0] == inherited) {
				CHECK(it.view_raw_field(0, 0).size() == 0);
				CHECK(it.view_raw_field_mut(0, 0).size() == 0);
				CHECK(it.sview_raw_field_mut(0, 0).size() == 0);
				++inheritedBatches;
				return;
			}

			CHECK(it.size() == 2);
			CHECK(it.view_raw(0).size() == 0);
			directRows += it.size();

			const auto health = it.view_raw_field(0, 0);
			CHECK(health.size() == it.size());
			CHECK(health[0].valid());
			CHECK(health[1].valid());
			CHECK(health[0].size == sizeof(uint32_t));
			CHECK((const uint8_t*)health[1].data - (const uint8_t*)health[0].data == sizeof(uint32_t));
			CHECK(*(const uint32_t*)health[0].data == 11);
			CHECK(*(const uint32_t*)health[1].data == 22);

			const auto score = it.view_raw_field(0, 1);
			CHECK(score.size() == it.size());
			CHECK(score[0].size == sizeof(double));
			CHECK((const uint8_t*)score[1].data - (const uint8_t*)score[0].data == sizeof(double));
			CHECK(*(const double*)score[0].data == doctest::Approx(1.5));
			CHECK(*(const double*)score[1].data == doctest::Approx(2.5));

			CHECK(it.view_raw_field(0, 2).size() == 0);
		});
		CHECK(directRows == 2);
		CHECK(inheritedBatches == 1);
	}

	SUBCASE("runtime SoA iterator field mutation tracks component writes") {
		TestWorld twld;
		constexpr uint8_t SoaSizes[] = {sizeof(uint32_t), sizeof(double)};
		const ecs::RuntimeFieldDesc fields[] = {
				{util::str_view("health"), ecs::U32, 0, 0}, //
				{util::str_view("score"), ecs::F64, 8, 0} //
		};

		ecs::ComponentDesc desc{};
		desc.name = util::str_view("Runtime_Component_SoA_Iter_Field_Mutation");
		desc.size = 16;
		desc.alig = 8;
		desc.soa = 2;
		desc.pSoaSizes = SoaSizes;
		desc.fields = fields;
		desc.fieldCount = 2;
		auto& runtimeComp = wld.add(desc);

		const auto entityA = wld.add();
		const auto entityB = wld.add();
		wld.add(entityA, runtimeComp.entity);
		wld.add(entityB, runtimeComp.entity);
		*(uint32_t*)wld.mut_raw_field(entityA, runtimeComp.entity, 0).data = 11;
		*(uint32_t*)wld.mut_raw_field(entityB, runtimeComp.entity, 0).data = 22;
		*(double*)wld.mut_raw_field(entityA, runtimeComp.entity, 1).data = 1.5;
		*(double*)wld.mut_raw_field(entityB, runtimeComp.entity, 1).data = 2.5;

		uint32_t setHits = 0;
		const auto onSet = wld.observer()
													 .event(ecs::ObserverEvent::OnSet)
													 .all(runtimeComp.entity)
													 .on_each([&](ecs::Entity) {
														 ++setHits;
													 })
													 .entity();
		(void)onSet;

		auto q = wld.query().all(runtimeComp.entity);
		q.each([&](ecs::Iter& it) {
			CHECK(it.view_raw_field_mut(0, 2).size() == 0);
			CHECK(it.sview_raw_field_mut(0, 2).size() == 0);
		});
		CHECK(setHits == 0);

		q.each([&](ecs::Iter& it) {
			auto health = it.view_raw_field_mut(0, 0);
			auto score = it.view_raw_field_mut(0, 1);
			CHECK(health.size() == it.size());
			CHECK(score.size() == it.size());
			GAIA_FOR(it.size()) {
				*(uint32_t*)health[i].data += 100;
				*(double*)score[i].data += 10.0;
			}
		});
		CHECK(setHits == 2);
		CHECK(*(const uint32_t*)wld.get_raw_field(entityA, runtimeComp.entity, 0).data == 111);
		CHECK(*(const uint32_t*)wld.get_raw_field(entityB, runtimeComp.entity, 0).data == 122);
		CHECK(*(const double*)wld.get_raw_field(entityA, runtimeComp.entity, 1).data == doctest::Approx(11.5));
		CHECK(*(const double*)wld.get_raw_field(entityB, runtimeComp.entity, 1).data == doctest::Approx(12.5));

		setHits = 0;
		q.each([&](ecs::Iter& it) {
			auto health = it.sview_raw_field_mut(0, 0);
			GAIA_FOR(it.size()) {
				*(uint32_t*)health[i].data += 1;
			}
		});
		CHECK(setHits == 0);
		CHECK(*(const uint32_t*)wld.get_raw_field(entityA, runtimeComp.entity, 0).data == 112);
		CHECK(*(const uint32_t*)wld.get_raw_field(entityB, runtimeComp.entity, 0).data == 123);

		q.each([&](ecs::Iter& it) {
			auto score = it.sview_raw_field_mut(0, 1);
			GAIA_FOR(it.size()) {
				*(double*)score[i].data += 1.0;
			}
			CHECK(setHits == 0);
			it.modify_raw(0);
		});
		CHECK(setHits == 2);

		setHits = 0;
		q.each([&](ecs::Iter& it) {
			auto health = it.view_raw_field_mut(0, 0);
			auto score = it.view_raw_field_mut(0, 1);
			GAIA_FOR(it.size()) {
				*(uint32_t*)health[i].data += 1;
				*(double*)score[i].data += 1.0;
			}
			it.modify_raw(0);
		});
		CHECK(setHits == 2);
		CHECK(*(const uint32_t*)wld.get_raw_field(entityA, runtimeComp.entity, 0).data == 113);
		CHECK(*(const uint32_t*)wld.get_raw_field(entityB, runtimeComp.entity, 0).data == 124);
		CHECK(*(const double*)wld.get_raw_field(entityA, runtimeComp.entity, 1).data == doctest::Approx(13.5));
		CHECK(*(const double*)wld.get_raw_field(entityB, runtimeComp.entity, 1).data == doctest::Approx(14.5));

		wld.enable(entityA, false);
		setHits = 0;
		q.each(
				[&](ecs::Iter& it) {
					auto health = it.view_raw_field_mut(0, 0);
					CHECK(health.size() == 1);
					*(uint32_t*)health[0].data += 1000;
				},
				ecs::Constraints::EnabledOnly);
		CHECK(setHits == 1);
		CHECK(*(const uint32_t*)wld.get_raw_field(entityA, runtimeComp.entity, 0).data == 113);
		CHECK(*(const uint32_t*)wld.get_raw_field(entityB, runtimeComp.entity, 0).data == 1124);
		wld.enable(entityA, true);
	}

	SUBCASE("runtime SoA pair fields expose contiguous iterator field views") {
		TestWorld twld;
		constexpr uint8_t SoaSizes[] = {sizeof(uint32_t), sizeof(double)};
		const ecs::RuntimeFieldDesc fields[] = {
				{util::str_view("health"), ecs::U32, 0, 0}, //
				{util::str_view("score"), ecs::F64, 8, 0} //
		};

		ecs::ComponentDesc desc{};
		desc.name = util::str_view("Runtime_Relation_SoA_Iter_Field_Mutation");
		desc.size = 16;
		desc.alig = 8;
		desc.soa = 2;
		desc.pSoaSizes = SoaSizes;
		desc.fields = fields;
		desc.fieldCount = 2;
		auto& relation = wld.add(desc);

		const auto target = wld.add();
		const ecs::Pair pair(relation.entity, target);
		const auto pairEntity = (ecs::Entity)pair;
		const auto entityA = wld.add();
		const auto entityB = wld.add();
		wld.add(entityA, pair);
		wld.add(entityB, pair);
		*(uint32_t*)wld.mut_raw_field(entityA, pairEntity, 0).data = 11;
		*(uint32_t*)wld.mut_raw_field(entityB, pairEntity, 0).data = 22;
		*(double*)wld.mut_raw_field(entityA, pairEntity, 1).data = 1.5;
		*(double*)wld.mut_raw_field(entityB, pairEntity, 1).data = 2.5;

		uint32_t setHits = 0;
		const auto onSet = wld.observer()
													 .event(ecs::ObserverEvent::OnSet)
													 .all(pairEntity)
													 .on_each([&](ecs::Entity) {
														 ++setHits;
													 })
													 .entity();
		(void)onSet;

		auto q = wld.query().all(pairEntity);
		q.each([&](ecs::Iter& it) {
			CHECK(it.view_raw(0).size() == 0);
			const auto health = it.view_raw_field(0, 0);
			CHECK(health.size() == it.size());
			if (health.size() != it.size())
				return;
			CHECK(health[0].valid());
			CHECK(health[0].size == sizeof(uint32_t));
			CHECK((const uint8_t*)health[1].data - (const uint8_t*)health[0].data == sizeof(uint32_t));
			CHECK(*(const uint32_t*)health[0].data == 11);
			CHECK(*(const uint32_t*)health[1].data == 22);

			auto score = it.view_raw_field_mut(0, 1);
			CHECK(score.size() == it.size());
			if (score.size() != it.size())
				return;
			CHECK(score[0].size == sizeof(double));
			GAIA_FOR(it.size()) {
				*(double*)score[i].data += 10.0;
			}
		});
		CHECK(setHits == 2);
		CHECK(*(const double*)wld.get_raw_field(entityA, pairEntity, 1).data == doctest::Approx(11.5));
		CHECK(*(const double*)wld.get_raw_field(entityB, pairEntity, 1).data == doctest::Approx(12.5));

		setHits = 0;
		q.each([&](ecs::Iter& it) {
			auto health = it.sview_raw_field_mut(0, 0);
			CHECK(health.size() == it.size());
			if (health.size() != it.size())
				return;
			GAIA_FOR(it.size()) {
				*(uint32_t*)health[i].data += 1;
			}
			CHECK(setHits == 0);
			it.modify_raw(0);
		});
		CHECK(setHits == 2);
		CHECK(*(const uint32_t*)wld.get_raw_field(entityA, pairEntity, 0).data == 12);
		CHECK(*(const uint32_t*)wld.get_raw_field(entityB, pairEntity, 0).data == 23);

		ecs::ComponentDesc targetDesc = desc;
		targetDesc.name = util::str_view("Runtime_Target_SoA_Iter_Field_Mutation");
		auto& targetPayload = wld.add(targetDesc);
		ecs::ComponentDesc relationTagDesc{};
		relationTagDesc.name = util::str_view("Runtime_Relation_Tag_SoA_Iter_Target_Payload");
		relationTagDesc.size = 0;
		relationTagDesc.alig = 1;
		auto& relationTag = wld.add(relationTagDesc);
		const ecs::Pair targetOwnedPair(relationTag.entity, targetPayload.entity);
		const auto targetOwnedPairEntity = (ecs::Entity)targetOwnedPair;
		const auto targetOwnedEntityA = wld.add();
		const auto targetOwnedEntityB = wld.add();
		wld.add(targetOwnedEntityA, targetOwnedPair);
		wld.add(targetOwnedEntityB, targetOwnedPair);
		*(uint32_t*)wld.mut_raw_field(targetOwnedEntityA, targetOwnedPairEntity, 0).data = 31;
		*(uint32_t*)wld.mut_raw_field(targetOwnedEntityB, targetOwnedPairEntity, 0).data = 42;
		*(double*)wld.mut_raw_field(targetOwnedEntityA, targetOwnedPairEntity, 1).data = 3.25;
		*(double*)wld.mut_raw_field(targetOwnedEntityB, targetOwnedPairEntity, 1).data = 4.5;

		uint32_t targetOwnedSetHits = 0;
		const auto targetOwnedOnSet = wld.observer()
																			.event(ecs::ObserverEvent::OnSet)
																			.all(targetOwnedPairEntity)
																			.on_each([&](ecs::Entity) {
																				++targetOwnedSetHits;
																			})
																			.entity();
		(void)targetOwnedOnSet;

		wld.query().all(targetOwnedPairEntity).each([&](ecs::Iter& it) {
			const auto health = it.view_raw_field(0, 0);
			CHECK(health.size() == it.size());
			if (health.size() != it.size())
				return;
			CHECK((const uint8_t*)health[1].data - (const uint8_t*)health[0].data == sizeof(uint32_t));
			CHECK(*(const uint32_t*)health[0].data == 31);
			CHECK(*(const uint32_t*)health[1].data == 42);

			auto score = it.view_raw_field_mut(0, 1);
			CHECK(score.size() == it.size());
			if (score.size() != it.size())
				return;
			GAIA_FOR(it.size()) {
				*(double*)score[i].data += 20.0;
			}
		});
		CHECK(targetOwnedSetHits == 2);
		CHECK(
				*(const double*)wld.get_raw_field(targetOwnedEntityA, targetOwnedPairEntity, 1).data == doctest::Approx(23.25));
		CHECK(
				*(const double*)wld.get_raw_field(targetOwnedEntityB, targetOwnedPairEntity, 1).data == doctest::Approx(24.5));

		targetOwnedSetHits = 0;
		wld.query().all(targetOwnedPairEntity).each([&](ecs::Iter& it) {
			auto health = it.sview_raw_field_mut(0, 0);
			CHECK(health.size() == it.size());
			if (health.size() != it.size())
				return;
			GAIA_FOR(it.size()) {
				*(uint32_t*)health[i].data += 2;
			}
			CHECK(targetOwnedSetHits == 0);
			it.modify_raw(0);
		});
		CHECK(targetOwnedSetHits == 2);
		CHECK(*(const uint32_t*)wld.get_raw_field(targetOwnedEntityA, targetOwnedPairEntity, 0).data == 33);
		CHECK(*(const uint32_t*)wld.get_raw_field(targetOwnedEntityB, targetOwnedPairEntity, 0).data == 44);

		auto checkWildcard = [&](ecs::Entity wildcardPair, uint32_t expectedRows) {
			uint32_t rows = 0;
			wld.query().all(wildcardPair).each([&](ecs::Iter& it) {
				rows += it.size();
				CHECK(it.view_raw_field(0, 0).size() == 0);
				CHECK(it.view_raw_field_mut(0, 0).size() == 0);
				CHECK(it.sview_raw_field_mut(0, 0).size() == 0);
			});
			CHECK(rows == expectedRows);
		};

		checkWildcard(ecs::Pair(relation.entity, ecs::All), 2);
		checkWildcard(ecs::Pair(ecs::All, target), 2);
		checkWildcard(ecs::Pair(relationTag.entity, ecs::All), 2);
		checkWildcard(ecs::Pair(ecs::All, targetPayload.entity), 2);

		uint32_t anyPairRows = 0;
		wld.query().all(ecs::Pair(ecs::All, ecs::All)).each([&](ecs::Iter& it) {
			const auto entities = it.view<ecs::Entity>();
			GAIA_FOR(it.size()) {
				const auto entity = entities[i];
				if (entity == entityA || entity == entityB || entity == targetOwnedEntityA || entity == targetOwnedEntityB)
					++anyPairRows;
			}
			CHECK(it.view_raw_field(0, 0).size() == 0);
			CHECK(it.view_raw_field_mut(0, 0).size() == 0);
			CHECK(it.sview_raw_field_mut(0, 0).size() == 0);
		});
		CHECK(anyPairRows == 4);
	}

	SUBCASE("runtime SoA iterator field views reject non-self sources") {
		TestWorld twld;
		constexpr uint8_t SoaSizes[] = {sizeof(uint32_t), sizeof(double)};
		const ecs::RuntimeFieldDesc fields[] = {
				{util::str_view("health"), ecs::U32, 0, 0}, //
				{util::str_view("score"), ecs::F64, 8, 0} //
		};

		ecs::ComponentDesc desc{};
		desc.name = util::str_view("Runtime_Component_SoA_Iter_NonSelf");
		desc.size = 16;
		desc.alig = 8;
		desc.soa = 2;
		desc.pSoaSizes = SoaSizes;
		desc.fields = fields;
		desc.fieldCount = 2;
		auto& runtimeComp = wld.add(desc);

		const auto source = wld.add();
		const auto directRow = wld.add();
		const auto traversedRow = wld.add();
		wld.add<Position>(directRow);
		wld.add<Position>(traversedRow);
		wld.add(source, runtimeComp.entity);
		wld.add(directRow, runtimeComp.entity);
		wld.add(traversedRow, runtimeComp.entity);
		wld.add(traversedRow, ecs::Pair(ecs::ChildOf, source));

		*(uint32_t*)wld.mut_raw_field(source, runtimeComp.entity, 0).data = 111;
		*(uint32_t*)wld.mut_raw_field(directRow, runtimeComp.entity, 0).data = 222;
		*(uint32_t*)wld.mut_raw_field(traversedRow, runtimeComp.entity, 0).data = 333;

		uint32_t setHits = 0;
		const auto onSet = wld.observer()
													 .event(ecs::ObserverEvent::OnSet)
													 .all(runtimeComp.entity)
													 .on_each([&](ecs::Entity) {
														 ++setHits;
													 })
													 .entity();
		(void)onSet;

		uint32_t explicitBatches = 0;
		auto explicitSource = wld.query().all<Position>().all(runtimeComp.entity, ecs::QueryTermOptions{}.src(source));
		explicitSource.each([&](ecs::Iter& it) {
			++explicitBatches;
			CHECK(it.view_raw_field(1, 0).size() == 0);
			CHECK(it.view_raw_field_mut(1, 0).size() == 0);
			CHECK(it.sview_raw_field_mut(1, 0).size() == 0);
			it.modify_raw(1);
		});
		CHECK(explicitBatches == 2);
		CHECK(setHits == 0);

		uint32_t traversalBatches = 0;
		auto traversedSource =
				wld.query().all<Position>().all(runtimeComp.entity, ecs::QueryTermOptions{}.trav_up(ecs::ChildOf));
		traversedSource.each([&](ecs::Iter& it) {
			++traversalBatches;
			CHECK(it.view_raw_field(1, 0).size() == 0);
			CHECK(it.view_raw_field_mut(1, 0).size() == 0);
			CHECK(it.sview_raw_field_mut(1, 0).size() == 0);
			it.modify_raw(1);
		});
		CHECK(traversalBatches == 2);
		CHECK(setHits == 0);

		const auto observerOnlyTag = wld.add();
		wld.add(traversedRow, observerOnlyTag);
		uint32_t observerBatches = 0;
		const auto traversedObserver = wld.observer()
																			 .event(ecs::ObserverEvent::OnAdd)
																			 .all<Position>()
																			 .all(runtimeComp.entity, ecs::QueryTermOptions{}.trav_up(ecs::ChildOf))
																			 .on_each([&](ecs::Iter& it) {
																				 ++observerBatches;
																				 CHECK(it.view_raw_field(1, 0).size() == 0);
																				 CHECK(it.view_raw_field_mut(1, 0).size() == 0);
																				 CHECK(it.sview_raw_field_mut(1, 0).size() == 0);
																				 it.modify_raw(1);
																			 })
																			 .entity();
		(void)traversedObserver;

		const auto observerTarget = wld.copy_ext(traversedRow);
		CHECK(observerBatches == 1);
		CHECK(setHits == 0);
		CHECK(*(const uint32_t*)wld.get_raw_field(observerTarget, runtimeComp.entity, 0).data == 333);

		CHECK(*(const uint32_t*)wld.get_raw_field(source, runtimeComp.entity, 0).data == 111);
		CHECK(*(const uint32_t*)wld.get_raw_field(directRow, runtimeComp.entity, 0).data == 222);
		CHECK(*(const uint32_t*)wld.get_raw_field(traversedRow, runtimeComp.entity, 0).data == 333);
	}

	SUBCASE("runtime unique SoA iterator field views are rejected") {
		TestWorld twld;
		constexpr uint8_t SoaSizes[] = {sizeof(uint32_t), sizeof(uint8_t)};
		auto& cc = wld.comp_cache_mut();
		const auto componentEntity = wld.add(ecs::EntityKind::EK_Uni);
		auto& runtimeComp = add_runtime_component(
				cc, componentEntity, "Runtime_Unique_Component_SoA_Iter_Fields", 8, ecs::DataStorageType::Table, 4, 2,
				SoaSizes);

		const auto entityA = wld.add();
		const auto entityB = wld.add();
		wld.add(entityA, runtimeComp.entity);
		wld.add(entityB, runtimeComp.entity);

		auto q = wld.query().all(runtimeComp.entity);
		q.each([&](ecs::Iter& it) {
			CHECK(it.view_raw_field(0, 0).size() == 0);
			CHECK(it.view_raw_field_mut(0, 0).size() == 0);
			CHECK(it.sview_raw_field_mut(0, 0).size() == 0);
		});
	}

	SUBCASE("runtime SoA field access rejects sparse storage and supports table pairs") {
		TestWorld twld;
		constexpr uint8_t SoaSizes[] = {sizeof(uint32_t)};
		const ecs::RuntimeFieldDesc fields[] = {{util::str_view("value"), ecs::U32, 0, 0}};

		ecs::ComponentDesc desc{};
		desc.name = util::str_view("Runtime_Component_SoA_Sparse_Unsupported");
		desc.size = sizeof(uint32_t);
		desc.alig = alignof(uint32_t);
		desc.soa = 1;
		desc.pSoaSizes = SoaSizes;
		desc.fields = fields;
		desc.fieldCount = 1;
		desc.storageType = ecs::DataStorageType::Sparse;
		auto& sparseComp = wld.add(desc);

		desc.name = util::str_view("Runtime_Component_SoA_Sparse_DontFragment_Unsupported");
		auto& sparseDontFragmentComp = wld.add(desc);
		wld.add(sparseDontFragmentComp.entity, ecs::DontFragment);

		const auto sparseEntity = wld.add();
		wld.add(sparseEntity, sparseComp.entity);
		CHECK_FALSE(wld.get_raw_field(sparseEntity, sparseComp.entity, 0).valid());
		CHECK_FALSE(wld.mut_raw_field(sparseEntity, sparseComp.entity, 0).valid());
		CHECK_FALSE(wld.cursor(sparseEntity, sparseComp.entity).valid());
		wld.query().all(sparseComp.entity).each([&](ecs::Iter& it) {
			CHECK(it.view_raw_field(0, 0).size() == 0);
			CHECK(it.view_raw_field_mut(0, 0).size() == 0);
			CHECK(it.sview_raw_field_mut(0, 0).size() == 0);
		});
		CHECK_FALSE(wld.cursor_mut(sparseEntity, sparseComp.entity).valid());

		const auto sparseDontFragmentEntity = wld.add();
		wld.add(sparseDontFragmentEntity, sparseDontFragmentComp.entity);
		CHECK_FALSE(wld.get_raw_field(sparseDontFragmentEntity, sparseDontFragmentComp.entity, 0).valid());
		CHECK_FALSE(wld.mut_raw_field(sparseDontFragmentEntity, sparseDontFragmentComp.entity, 0).valid());
		CHECK_FALSE(wld.cursor(sparseDontFragmentEntity, sparseDontFragmentComp.entity).valid());
		CHECK_FALSE(wld.cursor_mut(sparseDontFragmentEntity, sparseDontFragmentComp.entity).valid());
		wld.query().all(sparseDontFragmentComp.entity).each([&](ecs::Iter& it) {
			CHECK(it.view_raw_field(0, 0).size() == 0);
			CHECK(it.view_raw_field_mut(0, 0).size() == 0);
			CHECK(it.sview_raw_field_mut(0, 0).size() == 0);
		});

		desc.name = util::str_view("Runtime_Component_SoA_Relationship_Supported");
		desc.storageType = ecs::DataStorageType::Table;
		auto& relation = wld.add(desc);
		const auto target = wld.add();
		const auto source = wld.add();
		const auto relationPair = ecs::Pair(relation.entity, target);
		const auto pair = (ecs::Entity)relationPair;
		wld.add(source, relationPair);
		CHECK(wld.get_raw_field(source, pair, 0).valid());
		CHECK(wld.mut_raw_field(source, pair, 0).valid());
		CHECK(wld.cursor(source, pair).valid());
		CHECK(wld.cursor_mut(source, pair).valid());
		wld.query().all(pair).each([&](ecs::Iter& it) {
			CHECK(it.view_raw_field(0, 0).size() == 1);
			CHECK(it.view_raw_field(0, 0)[0].valid());
			CHECK(it.view_raw_field_mut(0, 0).size() == 1);
			CHECK(it.view_raw_field_mut(0, 0)[0].valid());
			CHECK(it.sview_raw_field_mut(0, 0).size() == 1);
			CHECK(it.sview_raw_field_mut(0, 0)[0].valid());
		});

		const auto& typedSoa = wld.add<PositionSoA>();
		CHECK(typedSoa.func_move_ctor != nullptr);
		CHECK(typedSoa.func_copy_ctor != nullptr);
		CHECK(typedSoa.func_copy != nullptr);
		CHECK(typedSoa.func_move != nullptr);
		CHECK(typedSoa.func_swap != nullptr);
	}

	SUBCASE("runtime unique SoA fields use one physical value per chunk") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		constexpr uint8_t SoaSizes[] = {4, 1, 2};
		const auto componentEntity = wld.add(ecs::EntityKind::EK_Uni);
		const auto& runtimeComp = add_runtime_component(
				cc, componentEntity, "Runtime_Component_Unique_SoA_Access", 32, ecs::DataStorageType::Table, 8, 3, SoaSizes);

		const auto entity = wld.add();
		wld.add(entity, runtimeComp.entity);
		auto value32 = wld.mut_raw_field(entity, runtimeComp.entity, 0);
		auto value8 = wld.mut_raw_field(entity, runtimeComp.entity, 1);
		auto value16 = wld.mut_raw_field(entity, runtimeComp.entity, 2);
		CHECK(value32.valid());
		CHECK(value8.valid());
		CHECK(value16.valid());
		if (value32.valid() && value8.valid() && value16.valid()) {
			const auto& ec = wld.fetch(entity);
			const auto compIdx = ec.pChunk->comp_idx(runtimeComp.entity);
			const std::span<const uint8_t> fieldSizes{SoaSizes};
			CHECK(
					value32.data == mem::data_view_policy_soa_erased::set(
															ec.pChunk->comp_ptr_mut(compIdx), runtimeComp.comp.alig(), fieldSizes, 0, 0, 1));
			CHECK(
					value8.data == mem::data_view_policy_soa_erased::set(
														 ec.pChunk->comp_ptr_mut(compIdx), runtimeComp.comp.alig(), fieldSizes, 1, 0, 1));
			CHECK(
					value16.data == mem::data_view_policy_soa_erased::set(
															ec.pChunk->comp_ptr_mut(compIdx), runtimeComp.comp.alig(), fieldSizes, 2, 0, 1));

			*(uint32_t*)value32.data = 0x12345678U;
			*(uint8_t*)value8.data = 0x5aU;
			*(uint16_t*)value16.data = 0x4321U;
			CHECK(*(const uint32_t*)wld.get_raw_field(entity, runtimeComp.entity, 0).data == 0x12345678U);
			CHECK(*(const uint8_t*)wld.get_raw_field(entity, runtimeComp.entity, 1).data == 0x5aU);
			CHECK(*(const uint16_t*)wld.get_raw_field(entity, runtimeComp.entity, 2).data == 0x4321U);

			alignas(8) uint8_t src[32]{};
			alignas(8) uint8_t dst[32]{};
			*(uint32_t*)mem::data_view_policy_soa_erased::set(src, 8, fieldSizes, 0, 0, 1) = 0x87654321U;
			*(uint8_t*)mem::data_view_policy_soa_erased::set(src, 8, fieldSizes, 1, 0, 1) = 0xa5U;
			*(uint16_t*)mem::data_view_policy_soa_erased::set(src, 8, fieldSizes, 2, 0, 1) = 0x1234U;
			runtimeComp.ctor_copy(dst, src, 0, 0, 64, 64);
			CHECK(*(const uint32_t*)mem::data_view_policy_soa_erased::get(dst, 8, fieldSizes, 0, 0, 1) == 0x87654321U);
			CHECK(*(const uint8_t*)mem::data_view_policy_soa_erased::get(dst, 8, fieldSizes, 1, 0, 1) == 0xa5U);
			CHECK(*(const uint16_t*)mem::data_view_policy_soa_erased::get(dst, 8, fieldSizes, 2, 0, 1) == 0x1234U);

			alignas(8) uint8_t other[32]{};
			*(uint32_t*)mem::data_view_policy_soa_erased::set(other, 8, fieldSizes, 0, 0, 1) = 0x0badc0deU;
			*(uint8_t*)mem::data_view_policy_soa_erased::set(other, 8, fieldSizes, 1, 0, 1) = 0x3cU;
			*(uint16_t*)mem::data_view_policy_soa_erased::set(other, 8, fieldSizes, 2, 0, 1) = 0xabcdU;
			runtimeComp.swap(src, other, 0, 0, 64, 64);
			CHECK(*(const uint32_t*)mem::data_view_policy_soa_erased::get(src, 8, fieldSizes, 0, 0, 1) == 0x0badc0deU);
			CHECK(*(const uint8_t*)mem::data_view_policy_soa_erased::get(src, 8, fieldSizes, 1, 0, 1) == 0x3cU);
			CHECK(*(const uint16_t*)mem::data_view_policy_soa_erased::get(src, 8, fieldSizes, 2, 0, 1) == 0xabcdU);
			CHECK(*(const uint32_t*)mem::data_view_policy_soa_erased::get(other, 8, fieldSizes, 0, 0, 1) == 0x87654321U);
			CHECK(*(const uint8_t*)mem::data_view_policy_soa_erased::get(other, 8, fieldSizes, 1, 0, 1) == 0xa5U);
			CHECK(*(const uint16_t*)mem::data_view_policy_soa_erased::get(other, 8, fieldSizes, 2, 0, 1) == 0x1234U);
		}
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

	SUBCASE("runtime field descriptors are copied at registration") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		ecs::RuntimeFieldDesc fields[] = {
				{util::str_view("x"), //
				 ecs::F32, 0, 0}, //
				{util::str_view("velocity"), ecs::F64, 12, 0} //
		};

		const auto entity = wld.add();
		(void)add_runtime_component_with_fields(
				cc, entity, "Runtime_Component_Fields", 24, ecs::DataStorageType::Table, 8, fields, 2);
		auto& item = cc.get(entity);

		CHECK(item.has_fields());
		CHECK(item.field_count() == 2);

		const auto* field = item.field(0);
		CHECK(field != nullptr);
		CHECK(strcmp(field->name, "x") == 0);
		CHECK(field->type == ecs::F32);
		CHECK(field->offset == 0);
		CHECK(field->count == 0);

		field = item.field(1);
		CHECK(field != nullptr);
		CHECK(strcmp(field->name, "velocity") == 0);
		CHECK(field->type == ecs::F64);
		CHECK(field->offset == 12);
		CHECK(field->count == 0);

		fields[0].name = util::str_view("mutated");
		fields[0].type = ecs::U32;
		fields[0].offset = 16;
		fields[0].count = 3;

		field = item.field(0);
		CHECK(field != nullptr);
		CHECK(strcmp(field->name, "x") == 0);
		CHECK(field->type == ecs::F32);
		CHECK(field->offset == 0);
		CHECK(field->count == 0);
		CHECK(item.field(util::str_view("mutated")) == nullptr);

		ecs::RuntimeFieldDesc invalid{};
		CHECK_FALSE(make_runtime_field(invalid, nullptr, 0, ser::serialization_type_id::u8, 0, 1));
		CHECK_FALSE(make_runtime_field(invalid, "", 0, ser::serialization_type_id::u8, 0, 1));

		char oversizedFieldName[ecs::ComponentCacheItem::MaxNameLength + 1] = {};
		GAIA_FOR(ecs::ComponentCacheItem::MaxNameLength) oversizedFieldName[i] = 'a';
		oversizedFieldName[ecs::ComponentCacheItem::MaxNameLength] = 0;
		CHECK_FALSE(make_runtime_field(
				invalid, oversizedFieldName, ecs::ComponentCacheItem::MaxNameLength, ser::serialization_type_id::u8, 0, 1));
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

		auto& runtimeComp = add_runtime_component_with_fields(
				wld, "Runtime_Component_Raw", RuntimePayloadSize, ecs::DataStorageType::Table, RuntimePayloadAlign,
				RuntimeXYZFields, RuntimeXYZFieldCount);

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
														 CHECK(payload.data != nullptr);
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
														 CHECK(payload.data != nullptr);
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
		CHECK(read_f32(payload.data, RuntimeXOffset) == doctest::Approx(1.0f));
		CHECK(read_f32(payload.data, RuntimeYOffset) == doctest::Approx(2.0f));
		CHECK(read_f32(payload.data, RuntimeZOffset) == doctest::Approx(3.0f));

		auto mutablePayload = wld.mut_raw(entity, runtimeComp.entity);
		CHECK(mutablePayload.valid());
		CHECK(mutablePayload.size == RuntimePayloadSize);
		CHECK(mutablePayload.data != nullptr);
		write_xyz(mutablePayload.data, 4.0f, 5.0f, 6.0f);
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

	SUBCASE("runtime raw payload access supports non-fragmenting sparse components") {
		TestWorld twld;
		static uint32_t ctorCalls = 0;
		static uint32_t dtorCalls = 0;
		ctorCalls = dtorCalls = 0;

		ecs::ComponentDesc desc{};
		desc.name = util::str_view("Runtime_Component_Raw_Sparse");
		desc.size = RuntimePayloadSize;
		desc.alig = RuntimePayloadAlign;
		desc.storageType = ecs::DataStorageType::Sparse;
		desc.fields = RuntimeXYZFields;
		desc.fieldCount = RuntimeXYZFieldCount;
		desc.funcCtor = [](void* data, uint32_t count) {
			++ctorCalls;
			memset(data, 0, (uintptr_t)RuntimePayloadSize * count);
		};
		desc.funcDtor = [](void*, uint32_t count) {
			dtorCalls += count;
		};
		auto& runtimeComp = wld.add(desc);
		wld.add(runtimeComp.entity, ecs::DontFragment);
		uint32_t setHits = 0;
		const auto onSet = wld.observer()
													 .event(ecs::ObserverEvent::OnSet)
													 .all(runtimeComp.entity)
													 .on_each([&](ecs::Entity) {
														 ++setHits;
													 })
													 .entity();
		(void)onSet;

		const auto entity = wld.add();
		uint8_t initial[RuntimePayloadSize]{};
		write_xyz(initial, 1.0f, 2.0f, 3.0f);
		CHECK(wld.add_raw(entity, runtimeComp.entity, initial, RuntimePayloadSize));
		CHECK(wld.has(entity, runtimeComp.entity));
		CHECK(ctorCalls == 1);
		CHECK(dtorCalls == 0);

		const auto payload = wld.get_raw(entity, runtimeComp.entity);
		CHECK(payload.valid());
		if (!payload.valid())
			return;
		CHECK(payload.size == RuntimePayloadSize);
		CHECK(read_f32(payload.data, RuntimeXOffset) == doctest::Approx(1.0f));
		CHECK(read_f32(payload.data, RuntimeYOffset) == doctest::Approx(2.0f));
		CHECK(read_f32(payload.data, RuntimeZOffset) == doctest::Approx(3.0f));
		const auto& typedPayload = wld.get<Position>(entity, runtimeComp.entity);
		CHECK(typedPayload.x == doctest::Approx(1.0f));
		CHECK(typedPayload.y == doctest::Approx(2.0f));
		CHECK(typedPayload.z == doctest::Approx(3.0f));

		const auto copy = wld.copy(entity);
		CHECK(wld.has(copy, runtimeComp.entity));
		const auto copiedPayload = wld.get_raw(copy, runtimeComp.entity);
		CHECK(copiedPayload.valid());
		if (!copiedPayload.valid())
			return;
		CHECK(read_f32(copiedPayload.data, RuntimeXOffset) == doctest::Approx(1.0f));
		CHECK(ctorCalls == 2);
		wld.del(copy);
		wld.update();
		CHECK(dtorCalls == 1);

		auto mutablePayload = wld.mut_raw(entity, runtimeComp.entity);
		CHECK(mutablePayload.valid());
		if (!mutablePayload.valid())
			return;
		write_xyz(mutablePayload.data, 4.0f, 5.0f, 6.0f);
		wld.modify_raw(entity, runtimeComp.entity);
		CHECK(setHits == 1);

		uint8_t replacement[RuntimePayloadSize]{};
		write_xyz(replacement, 7.0f, 8.0f, 9.0f);
		CHECK(wld.set_raw(entity, runtimeComp.entity, replacement, RuntimePayloadSize));
		CHECK(setHits == 2);
		const auto replaced = wld.get_raw(entity, runtimeComp.entity);
		CHECK(replaced.valid());
		if (!replaced.valid())
			return;
		CHECK(read_f32(replaced.data, RuntimeXOffset) == doctest::Approx(7.0f));
		CHECK(read_f32(replaced.data, RuntimeYOffset) == doctest::Approx(8.0f));
		CHECK(read_f32(replaced.data, RuntimeZOffset) == doctest::Approx(9.0f));
		auto cursor = wld.cursor_mut(entity, runtimeComp.entity);
		CHECK(cursor.field(util::str_view("x")));
		CHECK(cursor.f32(10.0f));
		CHECK(setHits == 3);
		CHECK(read_f32(wld.get_raw(entity, runtimeComp.entity).data, RuntimeXOffset) == doctest::Approx(10.0f));

		wld.del(entity, runtimeComp.entity);
		CHECK(dtorCalls == 2);
		CHECK_FALSE(wld.has(entity, runtimeComp.entity));
		CHECK(wld.add_raw(entity, runtimeComp.entity, initial, RuntimePayloadSize));
		CHECK(ctorCalls == 3);
		wld.del(entity);
		wld.update();
		CHECK(dtorCalls == 3);
	}

	SUBCASE("runtime sparse payloads preserve over-alignment") {
		TestWorld twld;
		constexpr uint32_t PayloadSize = 64;
		constexpr uint32_t PayloadAlignment = 64;

		ecs::ComponentDesc desc{};
		desc.name = util::str_view("Runtime_Component_Raw_Sparse_Aligned");
		desc.size = PayloadSize;
		desc.alig = PayloadAlignment;
		desc.storageType = ecs::DataStorageType::Sparse;
		auto& runtimeComp = wld.add(desc);
		wld.add(runtimeComp.entity, ecs::DontFragment);

		const auto entity = wld.add();
		alignas(PayloadAlignment) uint8_t initial[PayloadSize]{};
		CHECK(wld.add_raw(entity, runtimeComp.entity, initial, PayloadSize));
		const auto payload = wld.get_raw(entity, runtimeComp.entity);
		CHECK(payload.valid());
		CHECK((uintptr_t)payload.data % PayloadAlignment == 0);

		const auto copy = wld.copy(entity);
		const auto copiedPayload = wld.get_raw(copy, runtimeComp.entity);
		CHECK(copiedPayload.valid());
		CHECK((uintptr_t)copiedPayload.data % PayloadAlignment == 0);
	}

	SUBCASE("runtime sparse payload pages recycle small values") {
		TestWorld twld;
		static uint32_t ctorCalls = 0;
		static uint32_t dtorCalls = 0;
		ctorCalls = dtorCalls = 0;
		constexpr uint32_t EntityCount = 300;

		ecs::ComponentDesc desc{};
		desc.name = util::str_view("Runtime_Component_Raw_Sparse_Paged_Small");
		desc.size = 1;
		desc.alig = 1;
		desc.storageType = ecs::DataStorageType::Sparse;
		desc.funcCtor = [](void* data, uint32_t count) {
			ctorCalls += count;
			memset(data, 0, count);
		};
		desc.funcDtor = [](void*, uint32_t count) {
			dtorCalls += count;
		};
		auto& runtimeComp = wld.add(desc);
		wld.add(runtimeComp.entity, ecs::DontFragment);

		cnt::darray<ecs::Entity> entities;
		entities.reserve(EntityCount);
		bool allAdded = true;
		const uint8_t initial = 7;
		GAIA_FOR(EntityCount) {
			const auto entity = wld.add();
			entities.push_back(entity);
			allAdded = wld.add_raw(entity, runtimeComp.entity, &initial, sizeof(initial)) && allAdded;
		}
		CHECK(allAdded);
		CHECK(ctorCalls == EntityCount);

		for (auto entity: entities)
			wld.del(entity, runtimeComp.entity);
		CHECK(dtorCalls == EntityCount);

		const uint8_t replacement = 9;
		for (auto entity: entities)
			allAdded = wld.add_raw(entity, runtimeComp.entity, &replacement, sizeof(replacement)) && allAdded;
		CHECK(allAdded);
		CHECK(ctorCalls == EntityCount * 2);

		bool allRecycledValuesValid = true;
		for (auto entity: entities) {
			const auto payload = wld.get_raw(entity, runtimeComp.entity);
			allRecycledValuesValid =
					payload.valid() && *(const uint8_t*)payload.data == replacement && allRecycledValuesValid;
		}
		CHECK(allRecycledValuesValid);

		wld.del(runtimeComp.entity);
		wld.update();
		CHECK(dtorCalls == EntityCount * 2);
	}

	SUBCASE("typed-by-id first access keeps runtime sparse lifecycle metadata") {
		TestWorld twld;
		static uint32_t ctorCalls = 0;
		ctorCalls = 0;

		ecs::ComponentDesc desc{};
		desc.name = util::str_view("Runtime_Component_Raw_Sparse_Typed_First");
		desc.size = RuntimePayloadSize;
		desc.alig = RuntimePayloadAlign;
		desc.storageType = ecs::DataStorageType::Sparse;
		desc.funcCtor = [](void* data, uint32_t count) {
			ctorCalls += count;
			memset(data, 0, (uintptr_t)RuntimePayloadSize * count);
		};
		auto& runtimeComp = wld.add(desc);
		wld.add(runtimeComp.entity, ecs::DontFragment);

		const auto entity = wld.add();
		wld.add<Position>(entity, runtimeComp.entity, {1.0f, 2.0f, 3.0f});
		CHECK(ctorCalls == 1);
		const auto payload = wld.get_raw(entity, runtimeComp.entity);
		CHECK(payload.valid());
		CHECK(read_f32(payload.data, RuntimeXOffset) == doctest::Approx(1.0f));
	}

	SUBCASE("runtime component accessors expose raw payloads") {
		TestWorld twld;

		auto& runtimeComp = add_runtime_component_with_fields(
				wld, "Runtime_Component_Accessor_Raw", RuntimePayloadSize, ecs::DataStorageType::Table, RuntimePayloadAlign,
				RuntimeXYZFields, RuntimeXYZFieldCount);

		const auto entity = wld.add();
		uint8_t initial[RuntimePayloadSize]{};
		write_xyz(initial, 1.0f, 2.0f, 3.0f);
		CHECK(wld.add_raw(entity, runtimeComp.entity, initial, RuntimePayloadSize));

		uint32_t setHits = 0;
		uint8_t setSeen[RuntimePayloadSize]{};
		const auto onSet = wld.observer()
													 .event(ecs::ObserverEvent::OnSet)
													 .all(runtimeComp.entity)
													 .on_each([&](ecs::Entity observed) {
														 CHECK(observed == entity);
														 ++setHits;
														 const auto payload = wld.acc(observed).get_raw(runtimeComp.entity);
														 CHECK(payload.valid());
														 CHECK(payload.size == RuntimePayloadSize);
														 CHECK(payload.data != nullptr);
														 memcpy(setSeen, payload.data, RuntimePayloadSize);
													 })
													 .entity();
		(void)onSet;

		const auto payload = wld.acc(entity).get_raw(runtimeComp.entity);
		CHECK(payload.valid());
		CHECK(payload.size == RuntimePayloadSize);
		CHECK(payload.data != nullptr);
		CHECK(read_f32(payload.data, RuntimeXOffset) == doctest::Approx(1.0f));
		CHECK(read_f32(payload.data, RuntimeYOffset) == doctest::Approx(2.0f));
		CHECK(read_f32(payload.data, RuntimeZOffset) == doctest::Approx(3.0f));

		auto setter = wld.acc_mut(entity);
		auto mutablePayload = setter.mut_raw(runtimeComp.entity);
		CHECK(mutablePayload.valid());
		CHECK(mutablePayload.size == RuntimePayloadSize);
		CHECK(mutablePayload.data != nullptr);
		write_xyz(mutablePayload.data, 4.0f, 5.0f, 6.0f);
		CHECK(setHits == 0);

		setter.modify_raw(runtimeComp.entity);
		CHECK(setHits == 1);
		CHECK(read_f32(setSeen, RuntimeXOffset) == doctest::Approx(4.0f));
		CHECK(read_f32(setSeen, RuntimeYOffset) == doctest::Approx(5.0f));
		CHECK(read_f32(setSeen, RuntimeZOffset) == doctest::Approx(6.0f));

		uint8_t replacement[RuntimePayloadSize]{};
		write_xyz(replacement, 7.0f, 8.0f, 9.0f);
		wld.acc_mut(entity).set_raw(runtimeComp.entity, replacement, RuntimePayloadSize);
		CHECK(setHits == 2);
		CHECK(read_f32(setSeen, RuntimeXOffset) == doctest::Approx(7.0f));
		CHECK(read_f32(setSeen, RuntimeYOffset) == doctest::Approx(8.0f));
		CHECK(read_f32(setSeen, RuntimeZOffset) == doctest::Approx(9.0f));
	}

	SUBCASE("runtime components can be queried by component entity") {
		TestWorld twld;

		auto& runtimeComp = add_runtime_component_with_fields(
				wld, "Runtime_Component_Query", RuntimePayloadSize, ecs::DataStorageType::Table, RuntimePayloadAlign,
				RuntimeXYZFields, RuntimeXYZFieldCount);

		const auto entityA = wld.add();
		const auto entityB = wld.add();
		const auto entityWithoutRuntime = wld.add();

		uint8_t payloadA[RuntimePayloadSize]{};
		uint8_t payloadB[RuntimePayloadSize]{};
		write_xyz(payloadA, 1.0f, 2.0f, 3.0f);
		write_xyz(payloadB, 4.0f, 5.0f, 6.0f);
		CHECK(wld.add_raw(entityA, runtimeComp.entity, payloadA, RuntimePayloadSize));
		CHECK(wld.add_raw(entityB, runtimeComp.entity, payloadB, RuntimePayloadSize));

		uint32_t hits = 0;
		float xSum = 0.0f;
		auto q = wld.query().all(runtimeComp.entity);
		q.each([&](ecs::Entity entity) {
			CHECK(entity != entityWithoutRuntime);
			const auto payload = wld.get_raw(entity, runtimeComp.entity);
			CHECK(payload.valid());
			CHECK(payload.size == RuntimePayloadSize);
			CHECK(payload.data != nullptr);
			xSum += read_f32(payload.data, RuntimeXOffset);

			auto cursor = wld.cursor_mut(entity, runtimeComp.entity);
			CHECK(cursor.valid());
			CHECK(cursor.field(util::str_view("y")));
			CHECK(cursor.f32(10.0f + (float)hits));
			++hits;
		});

		CHECK(hits == 2);
		CHECK(xSum == doctest::Approx(5.0f));

		const auto payloadAfterA = wld.get_raw(entityA, runtimeComp.entity);
		const auto payloadAfterB = wld.get_raw(entityB, runtimeComp.entity);
		CHECK(payloadAfterA.valid());
		CHECK(payloadAfterB.valid());
		CHECK(payloadAfterA.data != nullptr);
		CHECK(payloadAfterB.data != nullptr);
		const auto yA = read_f32(payloadAfterA.data, RuntimeYOffset);
		const auto yB = read_f32(payloadAfterB.data, RuntimeYOffset);
		CHECK((yA == doctest::Approx(10.0f) || yA == doctest::Approx(11.0f)));
		CHECK((yB == doctest::Approx(10.0f) || yB == doctest::Approx(11.0f)));
		CHECK(yA != yB);

		CHECK_FALSE(wld.get_raw(entityWithoutRuntime, runtimeComp.entity).valid());
	}

	SUBCASE("runtime components expose raw iterator views") {
		TestWorld twld;

		auto& runtimeComp = add_runtime_component_with_fields(
				wld, "Runtime_Component_Query_Iter_View", RuntimePayloadSize, ecs::DataStorageType::Table, RuntimePayloadAlign,
				RuntimeXYZFields, RuntimeXYZFieldCount);

		const auto entityA = wld.add();
		const auto entityB = wld.add();

		uint8_t payloadA[RuntimePayloadSize]{};
		uint8_t payloadB[RuntimePayloadSize]{};
		write_xyz(payloadA, 1.0f, 2.0f, 3.0f);
		write_xyz(payloadB, 4.0f, 5.0f, 6.0f);
		CHECK(wld.add_raw(entityA, runtimeComp.entity, payloadA, RuntimePayloadSize));
		CHECK(wld.add_raw(entityB, runtimeComp.entity, payloadB, RuntimePayloadSize));

		uint32_t setHits = 0;
		float observedY = 0.0f;
		const auto onSet = wld.observer()
													 .event(ecs::ObserverEvent::OnSet)
													 .all(runtimeComp.entity)
													 .on_each([&](ecs::Entity entity) {
														 ++setHits;
														 const auto payload = wld.get_raw(entity, runtimeComp.entity);
														 CHECK(payload.valid());
														 CHECK(payload.size == RuntimePayloadSize);
														 CHECK(payload.data != nullptr);
														 observedY += read_f32(payload.data, RuntimeYOffset);
													 })
													 .entity();
		(void)onSet;

		float xSum = 0.0f;
		auto q = wld.query().all(runtimeComp.entity);
		q.each([&](ecs::Iter& it) {
			const auto raw = it.view_raw(0);
			CHECK(raw.size() == it.size());
			CHECK(it.view_raw_field(0, 0).size() == 0);
			CHECK(it.view_raw_field_mut(0, 0).size() == 0);
			CHECK(it.sview_raw_field_mut(0, 0).size() == 0);
			GAIA_FOR(it.size()) {
				const auto payload = raw[i];
				CHECK(payload.valid());
				CHECK(payload.size == RuntimePayloadSize);
				CHECK(payload.data != nullptr);
				xSum += read_f32(payload.data, RuntimeXOffset);
			}
		});

		CHECK(xSum == doctest::Approx(5.0f));

		q.each([&](ecs::Iter& it) {
			auto raw = it.view_raw_mut(0);
			CHECK(raw.size() == it.size());
			GAIA_FOR(it.size()) {
				auto payload = raw[i];
				CHECK(payload.valid());
				CHECK(payload.size == RuntimePayloadSize);
				CHECK(payload.data != nullptr);
				write_f32(payload.data, RuntimeYOffset, 10.0f + (float)i);
			}
		});

		CHECK(setHits == 2);
		CHECK(observedY == doctest::Approx(21.0f));

		setHits = 0;
		observedY = 0.0f;
		q.each([&](ecs::Iter& it) {
			auto raw = it.sview_raw_mut(0);
			CHECK(raw.size() == it.size());
			GAIA_FOR(it.size()) {
				auto payload = raw[i];
				CHECK(payload.valid());
				CHECK(payload.size == RuntimePayloadSize);
				CHECK(payload.data != nullptr);
				write_f32(payload.data, RuntimeZOffset, 20.0f + (float)i);
			}
			CHECK(setHits == 0);
			it.modify_raw(0);
		});

		CHECK(setHits == 2);

		const auto payloadAfterA = wld.get_raw(entityA, runtimeComp.entity);
		const auto payloadAfterB = wld.get_raw(entityB, runtimeComp.entity);
		CHECK(payloadAfterA.valid());
		CHECK(payloadAfterB.valid());
		CHECK(payloadAfterA.data != nullptr);
		CHECK(payloadAfterB.data != nullptr);
		const auto zA = read_f32(payloadAfterA.data, RuntimeZOffset);
		const auto zB = read_f32(payloadAfterB.data, RuntimeZOffset);
		CHECK((zA == doctest::Approx(20.0f) || zA == doctest::Approx(21.0f)));
		CHECK((zB == doctest::Approx(20.0f) || zB == doctest::Approx(21.0f)));
		CHECK(zA != zB);

		setHits = 0;
		q.each([&](ecs::Iter& it) {
			auto direct = it.view_raw_mut(0);
			auto resolved = it.view_raw_any_mut(runtimeComp.entity);
			CHECK(direct.size() == resolved.size());
			CHECK(direct[0].data == resolved[0].data);
		});
		CHECK(setHits == 2);
	}

	SUBCASE("runtime sparse components expose entity-resolved raw iterator views") {
		TestWorld twld;

		auto& runtimeComp = add_runtime_component_with_fields(
				wld, "Runtime_Component_Query_Iter_Sparse_View", RuntimePayloadSize, ecs::DataStorageType::Sparse,
				RuntimePayloadAlign, RuntimeXYZFields, RuntimeXYZFieldCount);
		wld.add(runtimeComp.entity, ecs::DontFragment);

		const auto entityA = wld.add();
		const auto entityB = wld.add();
		uint8_t payloadA[RuntimePayloadSize]{};
		uint8_t payloadB[RuntimePayloadSize]{};
		write_xyz(payloadA, 1.0f, 2.0f, 3.0f);
		write_xyz(payloadB, 4.0f, 5.0f, 6.0f);
		CHECK(wld.add_raw(entityA, runtimeComp.entity, payloadA, RuntimePayloadSize));
		CHECK(wld.add_raw(entityB, runtimeComp.entity, payloadB, RuntimePayloadSize));

		float xSum = 0.0f;
		auto q = wld.query().all(runtimeComp.entity);
		q.each([&](ecs::Iter& it) {
			CHECK(it.view_raw(0).size() == 0);
			const auto raw = it.view_raw_any(runtimeComp.entity);
			CHECK(raw.size() == it.size());
			GAIA_FOR(it.size()) {
				const auto payload = raw[i];
				CHECK(payload.valid());
				CHECK(payload.size == RuntimePayloadSize);
				xSum += read_f32(payload.data, RuntimeXOffset);
			}
		});
		CHECK(xSum == doctest::Approx(5.0f));

		uint32_t setHits = 0;
		const auto onSet = wld.observer()
													 .event(ecs::ObserverEvent::OnSet)
													 .all(runtimeComp.entity)
													 .on_each([&](ecs::Entity) {
														 ++setHits;
													 })
													 .entity();
		(void)onSet;

		q.each([&](ecs::Iter& it) {
			auto raw = it.view_raw_any_mut(runtimeComp.entity);
			GAIA_FOR(it.size()) {
				auto payload = raw[i];
				CHECK(payload.valid());
				write_f32(payload.data, RuntimeYOffset, 10.0f + (float)i);
			}
		});
		CHECK(setHits == 2);

		setHits = 0;
		q.each([&](ecs::Iter& it) {
			const auto hitsBefore = setHits;
			auto raw = it.sview_raw_any_mut(runtimeComp.entity);
			GAIA_FOR(it.size()) {
				auto payload = raw[i];
				CHECK(payload.valid());
				write_f32(payload.data, RuntimeZOffset, 20.0f + (float)i);
			}
			CHECK(setHits == hitsBefore);
			CHECK(it.modify_raw(runtimeComp.entity));
		});
		CHECK(setHits == 2);
	}

	SUBCASE("runtime sparse raw iterator views resolve inherited payloads") {
		TestWorld twld;

		auto& runtimeComp = add_runtime_component_with_fields(
				wld, "Runtime_Component_Query_Iter_Sparse_Inherited_View", RuntimePayloadSize, ecs::DataStorageType::Table,
				RuntimePayloadAlign, RuntimeXYZFields, RuntimeXYZFieldCount);
		wld.add(runtimeComp.entity, ecs::Sparse);
		wld.add(runtimeComp.entity, ecs::DontFragment);
		wld.add(runtimeComp.entity, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

		const auto base = wld.add();
		wld.add<Position>(base);
		uint8_t payload[RuntimePayloadSize]{};
		write_xyz(payload, 1.0f, 2.0f, 3.0f);
		CHECK(wld.add_raw(base, runtimeComp.entity, payload, RuntimePayloadSize));
		const auto child = wld.add();
		wld.add<Position>(child);
		wld.as(child, base);
		CHECK(wld.has_direct(base, runtimeComp.entity));
		CHECK_FALSE(wld.has_direct(child, runtimeComp.entity));
		const auto basePayload = wld.get_raw(base, runtimeComp.entity);
		CHECK(basePayload.valid());

		uint32_t rows = 0;
		auto q = wld.query().all<Position>();
		q.each([&](ecs::Iter& it) {
			const auto raw = it.view_raw_any(runtimeComp.entity);
			GAIA_FOR(it.size()) {
				const auto value = raw[i];
				CHECK(value.valid());
				CHECK(value.size == RuntimePayloadSize);
				CHECK(value.data == basePayload.data);
				CHECK(read_f32(value.data, RuntimeXOffset) == doctest::Approx(1.0f));
				++rows;
			}
		});
		CHECK(rows == 2);
	}

	SUBCASE("entity-resolved mutable raw views reject partial sparse ownership") {
		TestWorld twld;

		auto& runtimeComp = add_runtime_component_with_fields(
				wld, "Runtime_Component_Query_Iter_Partial_Sparse_View", RuntimePayloadSize, ecs::DataStorageType::Sparse,
				RuntimePayloadAlign, RuntimeXYZFields, RuntimeXYZFieldCount);
		wld.add(runtimeComp.entity, ecs::DontFragment);

		const auto entityA = wld.add();
		const auto entityB = wld.add();
		wld.add<Position>(entityA);
		wld.add<Position>(entityB);
		uint8_t payload[RuntimePayloadSize]{};
		CHECK(wld.add_raw(entityA, runtimeComp.entity, payload, RuntimePayloadSize));

		auto q = wld.query().all<Position>();
		q.each([&](ecs::Iter& it) {
			CHECK(it.view_raw_any_mut(runtimeComp.entity).size() == 0);
			CHECK_FALSE(it.modify_raw(runtimeComp.entity));
		});
		CHECK_FALSE(wld.mut_raw(entityB, runtimeComp.entity).valid());
	}

	SUBCASE("entity-resolved mutable raw views cap tracked sparse components safely") {
		TestWorld twld;

		constexpr uint32_t ComponentCount = ecs::ChunkHeader::MAX_COMPONENTS + 1;
		ecs::Entity components[ComponentCount]{};
		const auto entity = wld.add();
		wld.add<Position>(entity);
		uint32_t payload = 0;
		GAIA_FOR(ComponentCount) {
			char name[64]{};
			GAIA_STRFMT(name, sizeof(name), "Runtime_Component_Query_Iter_Sparse_View_%u", i);
			auto& component =
					add_runtime_component(wld, name, sizeof(payload), ecs::DataStorageType::Sparse, alignof(uint32_t));
			wld.add(component.entity, ecs::DontFragment);
			components[i] = component.entity;
			CHECK(wld.add_raw(entity, component.entity, &payload, sizeof(payload)));
		}

		auto q = wld.query().all<Position>();
		q.each([&](ecs::Iter& it) {
			GAIA_FOR(ecs::ChunkHeader::MAX_COMPONENTS) {
				CHECK(it.view_raw_any_mut(components[i]).size() == 1);
			}
			CHECK(it.view_raw_any_mut(components[ComponentCount - 1]).size() == 0);
			CHECK_FALSE(it.modify_raw(components[ComponentCount - 1]));
		});
	}

	SUBCASE("fragmenting runtime sparse components expose entity-resolved raw iterator views") {
		TestWorld twld;

		auto& runtimeComp = add_runtime_component_with_fields(
				wld, "Runtime_Component_Query_Iter_Fragmenting_Sparse_View", RuntimePayloadSize, ecs::DataStorageType::Sparse,
				RuntimePayloadAlign, RuntimeXYZFields, RuntimeXYZFieldCount);
		const auto entity = wld.add();
		uint8_t payload[RuntimePayloadSize]{};
		write_xyz(payload, 7.0f, 8.0f, 9.0f);
		CHECK(wld.add_raw(entity, runtimeComp.entity, payload, RuntimePayloadSize));

		auto q = wld.query().all(runtimeComp.entity);
		q.each([&](ecs::Iter& it) {
			CHECK(it.view_raw(0).size() == 0);
			const auto raw = it.view_raw_any(runtimeComp.entity);
			CHECK(raw.size() == 1);
			CHECK(raw[0].valid());
			CHECK(read_f32(raw[0].data, RuntimeXOffset) == doctest::Approx(7.0f));
		});
	}

	SUBCASE("entity-resolved raw iterator views read inherited runtime payloads") {
		TestWorld twld;

		auto& runtimeComp = add_runtime_component_with_fields(
				wld, "Runtime_Component_Query_Iter_Inherited_View", RuntimePayloadSize, ecs::DataStorageType::Table,
				RuntimePayloadAlign, RuntimeXYZFields, RuntimeXYZFieldCount);
		wld.add(runtimeComp.entity, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

		const auto base = wld.add();
		uint8_t payload[RuntimePayloadSize]{};
		write_xyz(payload, 3.0f, 4.0f, 5.0f);
		CHECK(wld.add_raw(base, runtimeComp.entity, payload, RuntimePayloadSize));

		const auto derived = wld.add();
		wld.as(derived, base);

		uint32_t rows = 0;
		float xSum = 0.0f;
		auto q = wld.query().all(runtimeComp.entity);
		q.each([&](ecs::Iter& it) {
			const auto entities = it.view<ecs::Entity>();
			const auto raw = it.view_raw_any(runtimeComp.entity);
			GAIA_FOR(it.size()) {
				const auto value = raw[i];
				CHECK(value.valid());
				xSum += read_f32(value.data, RuntimeXOffset);
				++rows;
			}
			if (entities[0] == derived) {
				CHECK(it.sview_raw_any_mut(runtimeComp.entity).size() == 0);
				CHECK_FALSE(it.modify_raw(runtimeComp.entity));
			}
		});
		CHECK(rows == 2);
		CHECK(xSum == doctest::Approx(6.0f));
		CHECK_FALSE(wld.mut_raw(derived, runtimeComp.entity).valid());
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

		constexpr uint32_t RuntimeCursorPayloadSize = 48;
		constexpr uint32_t RuntimeBoolOffset = 12;
		constexpr uint32_t RuntimeCharOffset = 13;
		constexpr uint32_t RuntimeNameOffset = 14;
		constexpr uint32_t RuntimeNameCount = 8;
		constexpr uint32_t RuntimeArrayOffset = 24;
		constexpr uint32_t RuntimeArrayCount = 2;
		constexpr uint32_t RuntimeChar16Offset = 32;
		constexpr uint32_t RuntimeChar32Offset = 36;

		const ecs::RuntimeFieldDesc RuntimeCursorFields[] = {
				{util::str_view("x"), ecs::F32, RuntimeXOffset, 0}, //
				{util::str_view("y"), ecs::F32, RuntimeYOffset, 0}, //
				{util::str_view("z"), ecs::F32, RuntimeZOffset, 0}, //
				{util::str_view("alive"), ecs::Bool, RuntimeBoolOffset, 0}, //
				{util::str_view("initial"), ecs::Char8, RuntimeCharOffset, 0}, //
				{util::str_view("name"), ecs::Char8, RuntimeNameOffset, RuntimeNameCount}, //
				{util::str_view("samples"), ecs::F32, RuntimeArrayOffset, RuntimeArrayCount}, //
				{util::str_view("wide16"), ecs::Char16, RuntimeChar16Offset, 0}, //
				{util::str_view("wide32"), ecs::Char32, RuntimeChar32Offset, 0} //
		};

		auto& runtimeComp = add_runtime_component_with_fields(
				wld, "Runtime_Component_Cursor", RuntimeCursorPayloadSize, ecs::DataStorageType::Table, RuntimePayloadAlign,
				RuntimeCursorFields, 9);

		const auto entity = wld.add();
		uint8_t initial[RuntimeCursorPayloadSize]{};
		write_xyz(initial, 1.0f, 2.0f, 3.0f);
		CHECK(wld.add_raw(entity, runtimeComp.entity, initial, RuntimeCursorPayloadSize));

		auto cursor = wld.cursor(entity, runtimeComp.entity);
		CHECK(cursor.valid());
		CHECK(cursor.type() == runtimeComp.entity);
		CHECK(cursor.size() == RuntimeCursorPayloadSize);
		CHECK(cursor.field_count() == 9);
		CHECK(cursor.ptr() != nullptr);
		CHECK(cursor.mut_ptr() == nullptr);
		CHECK_FALSE(cursor.f32());
		const float readOnlyWrite = 10.0f;
		CHECK(cursor.set_raw(&readOnlyWrite, sizeof(readOnlyWrite)).status == ecs::CursorStatus::ReadOnly);

		CHECK(cursor.field(util::str_view("y")));
		CHECK(cursor.valid());
		CHECK(cursor.depth() == 1);
		CHECK(cursor.type() == ecs::F32);
		CHECK(cursor.size() == sizeof(float));
		const auto y = cursor.f32();
		CHECK(y);
		CHECK(y.value == doctest::Approx(2.0f));
		const float readOnlyFieldWrite = 20.0f;
		CHECK(cursor.set_raw(&readOnlyFieldWrite, sizeof(readOnlyFieldWrite)).status == ecs::CursorStatus::ReadOnly);
		CHECK(cursor.s32().status == ecs::CursorStatus::TypeMismatch);
		CHECK(cursor.parent());
		CHECK(cursor.depth() == 0);

		CHECK(cursor.field(2));
		const auto z = cursor.f32();
		CHECK(z);
		CHECK(z.value == doctest::Approx(3.0f));
		float zRaw = 0.0f;
		const auto zRawBytes = cursor.get_raw(&zRaw, sizeof(zRaw));
		CHECK(zRawBytes);
		CHECK(zRawBytes.value == sizeof(zRaw));
		CHECK(zRaw == doctest::Approx(3.0f));

		uint32_t setHits = 0;
		uint8_t setSeen[RuntimeCursorPayloadSize]{};
		const auto onSet = wld.observer()
													 .event(ecs::ObserverEvent::OnSet)
													 .all(runtimeComp.entity)
													 .on_each([&](ecs::Entity observed) {
														 CHECK(observed == entity);
														 ++setHits;
														 const auto observedPayload = wld.get_raw(observed, runtimeComp.entity);
														 CHECK(observedPayload.valid());
														 CHECK(observedPayload.size == RuntimeCursorPayloadSize);
														 CHECK(observedPayload.data != nullptr);
														 memcpy(setSeen, observedPayload.data, RuntimeCursorPayloadSize);
													 })
													 .entity();
		(void)onSet;

		auto mutableCursor = wld.cursor_mut(entity, runtimeComp.entity);
		CHECK(mutableCursor.valid());
		CHECK(mutableCursor.field(util::str_view("x")));
		CHECK(mutableCursor.f32(42.0f));
		CHECK(setHits == 1);
		const auto x = mutableCursor.f32();
		CHECK(x);
		CHECK(x.value == doctest::Approx(42.0f));
		CHECK(read_f32(setSeen, RuntimeXOffset) == doctest::Approx(42.0f));

		CHECK(mutableCursor.f32(17.0f));
		CHECK(setHits == 2);
		const auto x2 = mutableCursor.f32();
		CHECK(x2);
		CHECK(x2.value == doctest::Approx(17.0f));

		CHECK(mutableCursor.f32(23.0f));
		CHECK(setHits == 3);
		const auto x3 = mutableCursor.f32();
		CHECK(x3);
		CHECK(x3.value == doctest::Approx(23.0f));

		CHECK(mutableCursor.parent());
		CHECK(mutableCursor.field(util::str_view("alive")));
		CHECK(mutableCursor.b(true));
		const auto alive = mutableCursor.b();
		CHECK(alive);
		CHECK(alive.value == true);
		CHECK(mutableCursor.f32(1.0f).status == ecs::CursorStatus::TypeMismatch);

		CHECK(mutableCursor.parent());
		CHECK(mutableCursor.field(util::str_view("initial")));
		CHECK(mutableCursor.c8('R'));
		const auto initialChar = mutableCursor.c8();
		CHECK(initialChar);
		CHECK(initialChar.value == 'R');

		CHECK(mutableCursor.parent());
		CHECK(mutableCursor.field(util::str_view("name")));
		CHECK(mutableCursor.c8("GaiaECS", 7));
		char nameBuffer[RuntimeNameCount]{};
		const auto nameBytes = mutableCursor.c8(nameBuffer, RuntimeNameCount);
		CHECK(nameBytes);
		CHECK(nameBytes.value == RuntimeNameCount);
		CHECK(memcmp(nameBuffer, "GaiaECS", 7) == 0);
		CHECK(nameBuffer[7] == 0);
		CHECK(mutableCursor.c8("too-long!", 9).status == ecs::CursorStatus::OutOfRange);
		char smallNameBuffer[RuntimeNameCount - 1]{};
		CHECK(mutableCursor.c8(smallNameBuffer, RuntimeNameCount - 1).status == ecs::CursorStatus::OutOfRange);

		CHECK(mutableCursor.parent());
		CHECK(mutableCursor.field(util::str_view("samples")));
		CHECK(mutableCursor.elem(1));
		CHECK(mutableCursor.f32(99.0f));
		const auto sample = mutableCursor.f32();
		CHECK(sample);
		CHECK(sample.value == doctest::Approx(99.0f));
		CHECK_FALSE(mutableCursor.elem(RuntimeArrayCount));

		CHECK(mutableCursor.parent());
		CHECK(mutableCursor.parent());
		CHECK(mutableCursor.field(util::str_view("wide16")));
		CHECK(mutableCursor.c16((char16_t)0x1234));
		const auto wide16 = mutableCursor.c16();
		CHECK(wide16);
		CHECK(wide16.value == (char16_t)0x1234);
		CHECK(mutableCursor.c8('x').status == ecs::CursorStatus::TypeMismatch);

		CHECK(mutableCursor.parent());
		CHECK(mutableCursor.field(util::str_view("wide32")));
		CHECK(mutableCursor.c32((char32_t)0x12345678));
		const auto wide32 = mutableCursor.c32();
		CHECK(wide32);
		CHECK(wide32.value == (char32_t)0x12345678);

		const uint8_t tooSmall[sizeof(char32_t) - 1]{};
		CHECK(mutableCursor.set_raw(tooSmall, sizeof(tooSmall)).status == ecs::CursorStatus::OutOfRange);
		CHECK(setHits == 9);
	}

	SUBCASE("runtime component cursor walks nested struct fields and arrays") {
		TestWorld twld;

		constexpr uint32_t RuntimeVec3Size = 12;
		constexpr uint32_t RuntimeTransformSize = 48;
		constexpr uint32_t RuntimePositionOffset = 0;
		constexpr uint32_t RuntimeVelocityOffset = 12;
		constexpr uint32_t RuntimeSamplesOffset = 24;
		constexpr uint32_t RuntimeSamplesCount = 2;

		const ecs::RuntimeFieldDesc vec3Fields[] = {
				{util::str_view("x"), ecs::F32, RuntimeXOffset, 0}, //
				{util::str_view("y"), ecs::F32, RuntimeYOffset, 0}, //
				{util::str_view("z"), ecs::F32, RuntimeZOffset, 0} //
		};

		constexpr const char* Vec3Name = "Runtime_Type_Vec3";
		ecs::ComponentDesc vec3Desc{};
		vec3Desc.name = runtime_component_name_view(Vec3Name);
		vec3Desc.size = RuntimeVec3Size;
		vec3Desc.alig = RuntimePayloadAlign;
		vec3Desc.storageType = ecs::DataStorageType::Table;
		vec3Desc.typeKind = ecs::RuntimeTypeKind::Struct;
		vec3Desc.fields = vec3Fields;
		vec3Desc.fieldCount = 3;
		auto& vec3Type = wld.add(vec3Desc);

		ecs::ComponentDesc vec3ArrayDesc{};
		vec3ArrayDesc.name = runtime_component_name_view("Runtime_Type_Vec3_Array_2");
		vec3ArrayDesc.size = RuntimeVec3Size * RuntimeSamplesCount;
		vec3ArrayDesc.alig = RuntimePayloadAlign;
		vec3ArrayDesc.storageType = ecs::DataStorageType::Table;
		vec3ArrayDesc.typeKind = ecs::RuntimeTypeKind::Array;
		vec3ArrayDesc.elementType = vec3Type.entity;
		vec3ArrayDesc.elementCount = RuntimeSamplesCount;
		auto& vec3ArrayType = wld.add(vec3ArrayDesc);

		ecs::ComponentDesc vec3GridDesc{};
		vec3GridDesc.name = runtime_component_name_view("Runtime_Type_Vec3_Array_2x2");
		vec3GridDesc.size = RuntimeTransformSize;
		vec3GridDesc.alig = RuntimePayloadAlign;
		vec3GridDesc.storageType = ecs::DataStorageType::Table;
		vec3GridDesc.typeKind = ecs::RuntimeTypeKind::Array;
		vec3GridDesc.elementType = vec3ArrayType.entity;
		vec3GridDesc.elementCount = 2;
		auto& vec3GridType = wld.add(vec3GridDesc);

		const ecs::RuntimeFieldDesc transformFields[] = {
				{util::str_view("position"), vec3Type.entity, RuntimePositionOffset, 0}, //
				{util::str_view("velocity"), vec3Type.entity, RuntimeVelocityOffset, 0}, //
				{util::str_view("samples"), vec3ArrayType.entity, RuntimeSamplesOffset, 0} //
		};

		constexpr const char* TransformName = "Runtime_Component_Nested_Transform";
		ecs::ComponentDesc transformDesc{};
		transformDesc.name = runtime_component_name_view(TransformName);
		transformDesc.size = RuntimeTransformSize;
		transformDesc.alig = RuntimePayloadAlign;
		transformDesc.storageType = ecs::DataStorageType::Table;
		transformDesc.typeKind = ecs::RuntimeTypeKind::Struct;
		transformDesc.fields = transformFields;
		transformDesc.fieldCount = 3;
		auto& transformComp = wld.add(transformDesc);

		CHECK(vec3Type.field_count() == 3);
		CHECK(vec3ArrayType.typeKind == ecs::RuntimeTypeKind::Array);
		CHECK(vec3ArrayType.element_type() == vec3Type.entity);
		CHECK(vec3ArrayType.element_count() == RuntimeSamplesCount);
		CHECK(vec3GridType.typeKind == ecs::RuntimeTypeKind::Array);
		CHECK(vec3GridType.element_type() == vec3ArrayType.entity);
		CHECK(vec3GridType.element_count() == 2);
		CHECK(transformComp.field_count() == 3);
		const auto* positionField = transformComp.field(util::str_view("position"));
		const auto* samplesField = transformComp.field(util::str_view("samples"));
		CHECK(positionField != nullptr);
		CHECK(samplesField != nullptr);
		CHECK(positionField->type == vec3Type.entity);
		CHECK(samplesField->type == vec3ArrayType.entity);
		CHECK(samplesField->count == 0);

		const auto entity = wld.add();
		uint8_t payload[RuntimeTransformSize]{};
		write_xyz(payload + RuntimePositionOffset, 1.0f, 2.0f, 3.0f);
		write_xyz(payload + RuntimeVelocityOffset, 4.0f, 5.0f, 6.0f);
		write_xyz(payload + RuntimeSamplesOffset, 7.0f, 8.0f, 9.0f);
		write_xyz(payload + RuntimeSamplesOffset + RuntimeVec3Size, 10.0f, 11.0f, 12.0f);
		CHECK(wld.add_raw(entity, transformComp.entity, payload, RuntimeTransformSize));

		auto cursor = wld.cursor(entity, transformComp.entity);
		CHECK(cursor.valid());
		CHECK(cursor.type() == transformComp.entity);
		CHECK(cursor.size() == RuntimeTransformSize);
		CHECK(cursor.field_count() == 3);
		CHECK(cursor.set_raw(payload, RuntimeTransformSize).status == ecs::CursorStatus::ReadOnly);
		CHECK_FALSE(cursor.parent());

		CHECK(cursor.field(util::str_view("position")));
		CHECK(cursor.type() == vec3Type.entity);
		CHECK(cursor.size() == RuntimeVec3Size);
		CHECK(cursor.field_count() == 3);
		CHECK(cursor.f32().status == ecs::CursorStatus::TypeMismatch);
		uint8_t positionCopy[RuntimeVec3Size]{};
		const auto positionBytes = cursor.get_raw(positionCopy, RuntimeVec3Size);
		CHECK(positionBytes);
		CHECK(positionBytes.value == RuntimeVec3Size);
		CHECK(read_f32(positionCopy, RuntimeYOffset) == doctest::Approx(2.0f));

		CHECK(cursor.field(util::str_view("y")));
		CHECK(cursor.type() == ecs::F32);
		const auto positionY = cursor.f32();
		CHECK(positionY);
		CHECK(positionY.value == doctest::Approx(2.0f));
		CHECK_FALSE(cursor.field(util::str_view("x")));
		CHECK(cursor.type() == ecs::F32);
		CHECK_FALSE(cursor.elem(0));
		CHECK(cursor.parent());
		CHECK(cursor.type() == vec3Type.entity);
		CHECK(cursor.parent());
		CHECK(cursor.type() == transformComp.entity);

		CHECK(cursor.field(util::str_view("samples")));
		CHECK(cursor.type() == vec3Type.entity);
		CHECK(cursor.size() == RuntimeVec3Size * RuntimeSamplesCount);
		CHECK(cursor.field_count() == 0);
		CHECK_FALSE(cursor.field(util::str_view("z")));
		CHECK(cursor.type() == vec3Type.entity);
		CHECK_FALSE(cursor.elem(RuntimeSamplesCount));
		CHECK(cursor.elem(1));
		CHECK(cursor.size() == RuntimeVec3Size);
		CHECK(cursor.field_count() == 3);
		CHECK(cursor.field(util::str_view("z")));
		const auto sampleZ = cursor.f32();
		CHECK(sampleZ);
		CHECK(sampleZ.value == doctest::Approx(12.0f));

		uint32_t setHits = 0;
		uint8_t observedPayload[RuntimeTransformSize]{};
		const auto onSet = wld.observer()
													 .event(ecs::ObserverEvent::OnSet)
													 .all(transformComp.entity)
													 .on_each([&](ecs::Entity observed) {
														 CHECK(observed == entity);
														 ++setHits;
														 const auto raw = wld.get_raw(observed, transformComp.entity);
														 CHECK(raw.valid());
														 CHECK(raw.size == RuntimeTransformSize);
														 CHECK(raw.data != nullptr);
														 memcpy(observedPayload, raw.data, RuntimeTransformSize);
													 })
													 .entity();
		(void)onSet;

		auto mutableCursor = wld.cursor_mut(entity, transformComp.entity);
		CHECK(mutableCursor.valid());
		CHECK(mutableCursor.field(util::str_view("velocity")));
		CHECK(mutableCursor.field(util::str_view("z")));
		CHECK(mutableCursor.f32(99.0f));
		CHECK(setHits == 1);
		CHECK(read_f32(observedPayload, RuntimeVelocityOffset + RuntimeZOffset) == doctest::Approx(99.0f));

		CHECK(mutableCursor.parent());
		CHECK(mutableCursor.parent());
		CHECK(mutableCursor.field(util::str_view("position")));
		uint8_t replacementPosition[RuntimeVec3Size]{};
		write_xyz(replacementPosition, -1.0f, -2.0f, -3.0f);
		CHECK(mutableCursor.set_raw(replacementPosition, RuntimeVec3Size));
		CHECK(setHits == 2);
		CHECK(read_f32(observedPayload, RuntimePositionOffset + RuntimeXOffset) == doctest::Approx(-1.0f));
		CHECK(read_f32(observedPayload, RuntimePositionOffset + RuntimeYOffset) == doctest::Approx(-2.0f));
		CHECK(mutableCursor.set_raw(replacementPosition, RuntimeVec3Size - 1).status == ecs::CursorStatus::OutOfRange);

		CHECK(mutableCursor.parent());
		CHECK(mutableCursor.field(util::str_view("samples")));
		CHECK(mutableCursor.elem(0));
		CHECK(mutableCursor.field(util::str_view("x")));
		CHECK(mutableCursor.f32(77.0f));
		CHECK(setHits == 3);

		const auto finalPayload = wld.get_raw(entity, transformComp.entity);
		CHECK(finalPayload.valid());
		CHECK(finalPayload.size == RuntimeTransformSize);
		CHECK(finalPayload.data != nullptr);
		CHECK(read_f32(finalPayload.data, RuntimePositionOffset + RuntimeXOffset) == doctest::Approx(-1.0f));
		CHECK(read_f32(finalPayload.data, RuntimePositionOffset + RuntimeYOffset) == doctest::Approx(-2.0f));
		CHECK(read_f32(finalPayload.data, RuntimePositionOffset + RuntimeZOffset) == doctest::Approx(-3.0f));
		CHECK(read_f32(finalPayload.data, RuntimeVelocityOffset + RuntimeZOffset) == doctest::Approx(99.0f));
		CHECK(read_f32(finalPayload.data, RuntimeSamplesOffset + RuntimeXOffset) == doctest::Approx(77.0f));
		CHECK(
				read_f32(finalPayload.data, RuntimeSamplesOffset + RuntimeVec3Size + RuntimeZOffset) == doctest::Approx(12.0f));

		const ecs::RuntimeFieldDesc inlineArrayFields[] = {
				{util::str_view("position"), vec3Type.entity, RuntimePositionOffset, 0}, //
				{util::str_view("velocity"), vec3Type.entity, RuntimeVelocityOffset, 0}, //
				{util::str_view("samples"), vec3Type.entity, RuntimeSamplesOffset, RuntimeSamplesCount} //
		};
		ecs::ComponentDesc inlineArrayDesc{};
		inlineArrayDesc.name = runtime_component_name_view("Runtime_Component_Nested_Transform_Inline_Array");
		inlineArrayDesc.size = RuntimeTransformSize;
		inlineArrayDesc.alig = RuntimePayloadAlign;
		inlineArrayDesc.storageType = ecs::DataStorageType::Table;
		inlineArrayDesc.typeKind = ecs::RuntimeTypeKind::Struct;
		inlineArrayDesc.fields = inlineArrayFields;
		inlineArrayDesc.fieldCount = 3;
		auto& inlineArrayComp = wld.add(inlineArrayDesc);
		const auto inlineEntity = wld.add();
		CHECK(wld.add_raw(inlineEntity, inlineArrayComp.entity, payload, RuntimeTransformSize));
		auto inlineCursor = wld.cursor(inlineEntity, inlineArrayComp.entity);
		CHECK(inlineCursor.field(util::str_view("samples")));
		CHECK(inlineCursor.field_count() == 0);
		CHECK(inlineCursor.elem(1));
		CHECK(inlineCursor.field(util::str_view("z")));
		const auto inlineSampleZ = inlineCursor.f32();
		CHECK(inlineSampleZ);
		CHECK(inlineSampleZ.value == doctest::Approx(12.0f));

		const ecs::RuntimeFieldDesc gridFields[] = {
				{util::str_view("rows"), vec3GridType.entity, 0, 0} //
		};
		ecs::ComponentDesc gridDesc{};
		gridDesc.name = runtime_component_name_view("Runtime_Component_Nested_Array_Grid");
		gridDesc.size = RuntimeTransformSize;
		gridDesc.alig = RuntimePayloadAlign;
		gridDesc.storageType = ecs::DataStorageType::Table;
		gridDesc.typeKind = ecs::RuntimeTypeKind::Struct;
		gridDesc.fields = gridFields;
		gridDesc.fieldCount = 1;
		auto& gridComp = wld.add(gridDesc);
		const auto gridEntity = wld.add();
		CHECK(wld.add_raw(gridEntity, gridComp.entity, payload, RuntimeTransformSize));
		auto gridCursor = wld.cursor(gridEntity, gridComp.entity);
		CHECK(gridCursor.field(util::str_view("rows")));
		CHECK(gridCursor.type() == vec3ArrayType.entity);
		CHECK(gridCursor.size() == RuntimeTransformSize);
		CHECK(gridCursor.field_count() == 0);
		CHECK_FALSE(gridCursor.field(util::str_view("z")));
		CHECK(gridCursor.elem(1));
		CHECK(gridCursor.type() == vec3ArrayType.entity);
		CHECK(gridCursor.size() == RuntimeVec3Size * RuntimeSamplesCount);
		CHECK(gridCursor.field_count() == 0);
		CHECK(gridCursor.elem(0));
		CHECK(gridCursor.type() == vec3Type.entity);
		CHECK(gridCursor.size() == RuntimeVec3Size);
		CHECK(gridCursor.field_count() == 3);
		CHECK(gridCursor.field(util::str_view("y")));
		const auto gridY = gridCursor.f32();
		CHECK(gridY);
		CHECK(gridY.value == doctest::Approx(8.0f));
	}

	SUBCASE("runtime raw payload access supports fragmenting sparse components") {
		TestWorld twld;

		const auto& sparseComp =
				add_runtime_component(wld, "Runtime_Component_Raw_Sparse", 4, ecs::DataStorageType::Sparse, 4);
		const auto entity = wld.add();
		uint32_t value = 42;

		CHECK(wld.add_raw(entity, sparseComp.entity, &value, sizeof(value)));
		const auto payload = wld.get_raw(entity, sparseComp.entity);
		CHECK(payload.valid());
		CHECK(payload.size == sizeof(value));
		if (!payload.valid())
			return;
		CHECK(*(const uint32_t*)payload.data == value);

		value = 84;
		CHECK(wld.set_raw(entity, sparseComp.entity, &value, sizeof(value)));
		CHECK(*(const uint32_t*)wld.get_raw(entity, sparseComp.entity).data == value);

		wld.del(entity, sparseComp.entity);
		CHECK_FALSE(wld.get_raw(entity, sparseComp.entity).valid());
	}

	SUBCASE("raw sparse storage remains compatible with typed component access") {
		SparseTestWorld twld;
		const auto& sparseComp = wld.add<PositionSparse>();
		const auto entity = wld.add();
		const PositionSparse initial{1.0f, 2.0f, 3.0f};

		CHECK(wld.add_raw(entity, sparseComp.entity, &initial, sizeof(initial)));
		const auto& typed = wld.get<PositionSparse>(entity);
		CHECK(typed.x == doctest::Approx(1.0f));
		CHECK(typed.y == doctest::Approx(2.0f));
		CHECK(typed.z == doctest::Approx(3.0f));

		auto& mutableTyped = wld.mut<PositionSparse>(entity);
		mutableTyped.x = 4.0f;
		CHECK(((const PositionSparse*)wld.get_raw(entity, sparseComp.entity).data)->x == doctest::Approx(4.0f));
		wld.modify<PositionSparse, true>(entity);
		wld.set<PositionSparse>(entity) = {5.0f, 6.0f, 7.0f};
		CHECK(((const PositionSparse*)wld.get_raw(entity, sparseComp.entity).data)->x == doctest::Approx(5.0f));

		const auto copy = wld.copy(entity);
		CHECK(wld.get<PositionSparse>(copy).y == doctest::Approx(6.0f));
		wld.del(copy);
		wld.del(entity);
		wld.update();
	}
}

TEST_CASE("Erased pair access preserves compile-time target-owned payload metadata") {
	using PairType = ecs::pair<ErasedPairRelationTag, ErasedPairPayload>;

	TestWorld twld;
	const auto source = wld.add();
	wld.add<PairType>(source, {1.0f, 2.0f});

	const auto relation = wld.add<PairType::rel>().entity;
	const auto target = wld.add<PairType::tgt>().entity;
	const auto pair = (ecs::Entity)ecs::Pair(relation, target);
	CHECK(wld.comp_cache().find_pair_payload(pair)->entity == target);

	const auto initial = wld.get_raw(source, pair);
	CHECK(initial.valid());
	CHECK(initial.size == sizeof(ErasedPairPayload));
	if (initial.size != sizeof(ErasedPairPayload))
		return;
	CHECK(((const ErasedPairPayload*)initial.data)->x == doctest::Approx(1.0f));

	auto mutableView = wld.mut_raw(source, pair);
	CHECK(mutableView.valid());
	CHECK(mutableView.size == sizeof(ErasedPairPayload));
	if (mutableView.size != sizeof(ErasedPairPayload))
		return;
	((ErasedPairPayload*)mutableView.data)->y = 3.0f;
	wld.modify_raw(source, pair);
	CHECK(wld.get<PairType>(source).y == doctest::Approx(3.0f));

	const ErasedPairPayload replacement{4.0f, 5.0f};
	CHECK(wld.set_raw(source, pair, &replacement, sizeof(replacement)));
	CHECK(wld.get<PairType>(source).x == doctest::Approx(4.0f));
	CHECK(wld.get<PairType>(source).y == doctest::Approx(5.0f));

	uint32_t exactSetCalls = 0;
	const auto onExactSet = wld.observer()
															.event(ecs::ObserverEvent::OnSet)
															.all(pair)
															.on_each([&](ecs::Entity entity) {
																CHECK(entity == source);
																++exactSetCalls;
															})
															.entity();
	(void)onExactSet;
	uint32_t relationWildcardSetCalls = 0;
	const auto onRelationWildcardSet = wld.observer()
																				 .event(ecs::ObserverEvent::OnSet)
																				 .all(ecs::Pair(relation, ecs::All))
																				 .on_each([&](ecs::Entity entity) {
																					 CHECK(entity == source);
																					 ++relationWildcardSetCalls;
																				 })
																				 .entity();
	(void)onRelationWildcardSet;
	uint32_t targetWildcardSetCalls = 0;
	const auto onTargetWildcardSet = wld.observer()
																			 .event(ecs::ObserverEvent::OnSet)
																			 .all(ecs::Pair(ecs::All, target))
																			 .on_each([&](ecs::Entity entity) {
																				 CHECK(entity == source);
																				 ++targetWildcardSetCalls;
																			 })
																			 .entity();
	(void)onTargetWildcardSet;
	uint32_t anyPairSetCalls = 0;
	const auto onAnyPairSet = wld.observer()
																.event(ecs::ObserverEvent::OnSet)
																.all(ecs::Pair(ecs::All, ecs::All))
																.on_each([&](ecs::Entity entity) {
																	CHECK(entity == source);
																	++anyPairSetCalls;
																})
																.entity();
	(void)onAnyPairSet;
	wld.modify_raw(source, pair);
	CHECK(exactSetCalls == 1);
	CHECK(relationWildcardSetCalls == 1);
	CHECK(targetWildcardSetCalls == 1);
	CHECK(anyPairSetCalls == 1);

	auto cursor = wld.cursor(source, pair);
	CHECK(cursor.valid());
	CHECK(cursor.type() == pair);
	CHECK(cursor.size() == sizeof(ErasedPairPayload));
	CHECK(((const ErasedPairPayload*)cursor.ptr())->x == doctest::Approx(4.0f));

	auto query = wld.query().all(pair);
	CHECK(query.count() == 1);
	query.each([&](ecs::Iter& it) {
		const auto raw = it.view_raw(0);
		CHECK(raw.size() == it.size());
		if (raw.size() == 0)
			return;
		CHECK(((const ErasedPairPayload*)raw[0].data)->y == doctest::Approx(5.0f));
	});

	const auto rawSource = wld.add();
	const ErasedPairPayload rawInitial{6.0f, 7.0f};
	const bool rawAdded = wld.add_raw(rawSource, pair, &rawInitial, sizeof(rawInitial));
	CHECK(rawAdded);
	if (!rawAdded)
		return;
	CHECK(wld.get<PairType>(rawSource).x == doctest::Approx(6.0f));
	CHECK(wld.get<PairType>(rawSource).y == doctest::Approx(7.0f));

	using UniquePairType = ecs::pair<ErasedPairRelationTag, ecs::uni<ErasedPairPayload>>;
	const auto uniqueRelation = wld.add<UniquePairType::rel>().entity;
	const auto uniqueTarget = wld.add<UniquePairType::tgt>().entity;
	const auto uniquePair = (ecs::Entity)ecs::Pair(uniqueRelation, uniqueTarget);
	CHECK(wld.comp_cache().find_pair_payload(uniquePair)->entity == uniqueTarget);
	const auto uniqueSourceA = wld.add();
	const auto uniqueSourceB = wld.add();
	wld.add<UniquePairType>(uniqueSourceA, {8.0f, 9.0f});
	wld.add<UniquePairType>(uniqueSourceB, {10.0f, 11.0f});
	CHECK(wld.get<UniquePairType>(uniqueSourceA).x == doctest::Approx(10.0f));
	CHECK(wld.get<UniquePairType>(uniqueSourceB).y == doctest::Approx(11.0f));
}

TEST_CASE("Erased pair access preserves compile-time relation-owned payload metadata") {
	using PairType = ecs::pair<ErasedPairPayload, ErasedPairTargetTag>;

	TestWorld twld;
	const auto source = wld.add();
	wld.add<PairType>(source, {1.0f, 2.0f});

	const auto relation = wld.add<PairType::rel>().entity;
	const auto target = wld.add<PairType::tgt>().entity;
	const auto pair = (ecs::Entity)ecs::Pair(relation, target);
	CHECK(wld.comp_cache().find_pair_payload(pair)->entity == relation);

	const auto initial = wld.get_raw(source, pair);
	CHECK(initial.valid());
	CHECK(initial.size == sizeof(ErasedPairPayload));
	if (initial.size != sizeof(ErasedPairPayload))
		return;
	CHECK(((const ErasedPairPayload*)initial.data)->x == doctest::Approx(1.0f));

	auto mutableView = wld.mut_raw(source, pair);
	CHECK(mutableView.valid());
	CHECK(mutableView.size == sizeof(ErasedPairPayload));
	if (mutableView.size != sizeof(ErasedPairPayload))
		return;
	((ErasedPairPayload*)mutableView.data)->y = 3.0f;
	wld.modify_raw(source, pair);
	CHECK(wld.get<PairType>(source).y == doctest::Approx(3.0f));

	const ErasedPairPayload replacement{4.0f, 5.0f};
	CHECK(wld.set_raw(source, pair, &replacement, sizeof(replacement)));
	CHECK(wld.get<PairType>(source).x == doctest::Approx(4.0f));
	CHECK(wld.get<PairType>(source).y == doctest::Approx(5.0f));

	uint32_t exactSetCalls = 0;
	const auto onExactSet = wld.observer()
															.event(ecs::ObserverEvent::OnSet)
															.all(pair)
															.on_each([&](ecs::Entity entity) {
																CHECK(entity == source);
																++exactSetCalls;
															})
															.entity();
	(void)onExactSet;
	uint32_t relationWildcardSetCalls = 0;
	const auto onRelationWildcardSet = wld.observer()
																				 .event(ecs::ObserverEvent::OnSet)
																				 .all(ecs::Pair(relation, ecs::All))
																				 .on_each([&](ecs::Entity entity) {
																					 CHECK(entity == source);
																					 ++relationWildcardSetCalls;
																				 })
																				 .entity();
	(void)onRelationWildcardSet;
	uint32_t targetWildcardSetCalls = 0;
	const auto onTargetWildcardSet = wld.observer()
																			 .event(ecs::ObserverEvent::OnSet)
																			 .all(ecs::Pair(ecs::All, target))
																			 .on_each([&](ecs::Entity entity) {
																				 CHECK(entity == source);
																				 ++targetWildcardSetCalls;
																			 })
																			 .entity();
	(void)onTargetWildcardSet;
	uint32_t anyPairSetCalls = 0;
	const auto onAnyPairSet = wld.observer()
																.event(ecs::ObserverEvent::OnSet)
																.all(ecs::Pair(ecs::All, ecs::All))
																.on_each([&](ecs::Entity entity) {
																	CHECK(entity == source);
																	++anyPairSetCalls;
																})
																.entity();
	(void)onAnyPairSet;
	wld.modify_raw(source, pair);
	CHECK(exactSetCalls == 1);
	CHECK(relationWildcardSetCalls == 1);
	CHECK(targetWildcardSetCalls == 1);
	CHECK(anyPairSetCalls == 1);

	auto query = wld.query().all(pair);
	CHECK(query.count() == 1);
	query.each([&](ecs::Iter& it) {
		const auto raw = it.view_raw(0);
		CHECK(raw.size() == it.size());
		if (raw.size() == 0)
			return;
		CHECK(((const ErasedPairPayload*)raw[0].data)->x == doctest::Approx(4.0f));
	});

	using UniquePairType = ecs::pair<ecs::uni<ErasedPairPayload>, ErasedPairTargetTag>;
	const auto uniqueRelation = wld.add<UniquePairType::rel>().entity;
	const auto uniqueTarget = wld.add<UniquePairType::tgt>().entity;
	const auto uniquePair = (ecs::Entity)ecs::Pair(uniqueRelation, uniqueTarget);
	CHECK(wld.comp_cache().find_pair_payload(uniquePair)->entity == uniqueRelation);
	const auto uniqueSourceA = wld.add();
	const auto uniqueSourceB = wld.add();
	wld.add<UniquePairType>(uniqueSourceA, {8.0f, 9.0f});
	wld.add<UniquePairType>(uniqueSourceB, {10.0f, 11.0f});
	CHECK(wld.get<UniquePairType>(uniqueSourceA).x == doctest::Approx(10.0f));
	CHECK(wld.get<UniquePairType>(uniqueSourceB).y == doctest::Approx(11.0f));
}

TEST_CASE("Runtime pair payloads use target metadata across erased APIs") {
	constexpr uint32_t PayloadSize = sizeof(float) * 2;
	constexpr uint32_t XOffset = 0;
	constexpr uint32_t YOffset = sizeof(float);
	const ecs::RuntimeFieldDesc fields[] = {
			{util::str_view("x"), ecs::F32, XOffset, 0}, {util::str_view("y"), ecs::F32, YOffset, 0}};

	auto write_f32 = [](void* data, uint32_t offset, float value) {
		memcpy((uint8_t*)data + offset, &value, sizeof(value));
	};
	auto read_f32 = [](const void* data, uint32_t offset) {
		float value = 0.0f;
		memcpy(&value, (const uint8_t*)data + offset, sizeof(value));
		return value;
	};

	TestWorld twld;
	ecs::ComponentDesc desc{};
	desc.name = util::str_view("Runtime_Pair_Target");
	desc.size = PayloadSize;
	desc.alig = alignof(float);
	desc.storageType = ecs::DataStorageType::Table;
	desc.typeKind = ecs::RuntimeTypeKind::Struct;
	desc.fields = fields;
	desc.fieldCount = 2;
	auto& targetItem = wld.add(desc);

	const auto relation = wld.add();
	const auto source = wld.add();
	const auto pair = (ecs::Entity)ecs::Pair(relation, targetItem.entity);
	CHECK(wld.comp_cache().find(pair) == nullptr);
	CHECK(wld.comp_cache().find_pair_payload(pair) == &targetItem);

	ecs::ComponentDesc relationTagDesc{};
	relationTagDesc.name = util::str_view("Runtime_Pair_Relation_Tag_Target_Payload");
	relationTagDesc.size = 0;
	relationTagDesc.alig = 1;
	relationTagDesc.storageType = ecs::DataStorageType::Table;
	auto& relationTag = wld.add(relationTagDesc);
	const auto taggedSource = wld.add();
	const auto taggedPair = (ecs::Entity)ecs::Pair(relationTag.entity, targetItem.entity);
	CHECK(wld.comp_cache().find_pair_payload(taggedPair) == &targetItem);
	float taggedInitial[] = {7.0f, 8.0f};
	CHECK(wld.add_raw(taggedSource, taggedPair, taggedInitial, sizeof(taggedInitial)));
	const auto taggedView = wld.get_raw(taggedSource, taggedPair);
	CHECK(taggedView.valid());
	if (taggedView.valid())
		CHECK(read_f32(taggedView.data, YOffset) == doctest::Approx(8.0f));

	float initial[] = {1.0f, 2.0f};
	CHECK(wld.add_raw(source, pair, initial, sizeof(initial)));

	const auto initialView = wld.get_raw(source, pair);
	CHECK(initialView.valid());
	CHECK(initialView.size == PayloadSize);
	if (!initialView.valid())
		return;
	CHECK(read_f32(initialView.data, XOffset) == doctest::Approx(1.0f));

	uint32_t setObserverCalls = 0;
	const auto onSet = wld.observer()
												 .event(ecs::ObserverEvent::OnSet)
												 .all(pair)
												 .on_each([&](ecs::Entity entity) {
													 CHECK(entity == source);
													 ++setObserverCalls;
												 })
												 .entity();
	(void)onSet;

	auto mutableView = wld.mut_raw(source, pair);
	CHECK(mutableView.valid());
	CHECK(mutableView.size == PayloadSize);
	if (!mutableView.valid())
		return;
	write_f32(mutableView.data, YOffset, 3.0f);
	CHECK(setObserverCalls == 0);
	wld.modify_raw(source, pair);
	CHECK(setObserverCalls == 1);

	auto cursor = wld.cursor_mut(source, pair);
	CHECK(cursor.valid());
	CHECK(cursor.type() == pair);
	CHECK(cursor.field(util::str_view("x")));
	CHECK(cursor.f32(4.0f));
	CHECK(setObserverCalls == 2);

	float replacement[] = {5.0f, 6.0f};
	CHECK(wld.set_raw(source, pair, replacement, sizeof(replacement)));
	CHECK(setObserverCalls == 3);

	auto readCursor = wld.cursor(source, pair);
	CHECK(readCursor.valid());
	CHECK(readCursor.type() == pair);
	CHECK(readCursor.field(util::str_view("y")));
	const auto y = readCursor.f32();
	CHECK(y);
	if (y)
		CHECK(y.value == doctest::Approx(6.0f));

	auto exactQuery = wld.query().all(pair);
	CHECK(exactQuery.count() == 1);
	exactQuery.each([&](ecs::Iter& it) {
		const auto raw = it.view_raw(0);
		CHECK(raw.size() == it.size());
		if (raw.size() != it.size() || raw.size() == 0)
			return;
		CHECK(raw[0].valid());
		CHECK(read_f32(raw[0].data, YOffset) == doctest::Approx(6.0f));
	});

	uint32_t relationWildcardRows = 0;
	wld.query().all(ecs::Pair(relation, ecs::All)).each([&](ecs::Entity entity) {
		CHECK(entity == source);
		++relationWildcardRows;
	});
	CHECK(relationWildcardRows == 1);

	uint32_t targetWildcardRows = 0;
	wld.query().all(ecs::Pair(ecs::All, targetItem.entity)).each([&](ecs::Entity entity) {
		const bool expectedEntity = entity == source || entity == taggedSource;
		CHECK(expectedEntity);
		++targetWildcardRows;
	});
	CHECK(targetWildcardRows == 2);

	uint32_t targetWildcardSetRows = 0;
	const auto onTargetWildcardSet = wld.observer()
																			 .event(ecs::ObserverEvent::OnSet)
																			 .all(ecs::Pair(ecs::All, targetItem.entity))
																			 .on_each([&](ecs::Entity entity) {
																				 const bool expectedEntity = entity == source || entity == taggedSource;
																				 CHECK(expectedEntity);
																				 ++targetWildcardSetRows;
																			 })
																			 .entity();
	(void)onTargetWildcardSet;
	wld.modify_raw(source, pair);
	wld.modify_raw(taggedSource, taggedPair);
	CHECK(targetWildcardSetRows == 2);
	CHECK(setObserverCalls == 4);

	uint32_t relationWildcardSetRows = 0;
	const auto onRelationWildcardSet = wld.observer()
																				 .event(ecs::ObserverEvent::OnSet)
																				 .all(ecs::Pair(relation, ecs::All))
																				 .on_each([&](ecs::Entity entity) {
																					 CHECK(entity == source);
																					 ++relationWildcardSetRows;
																				 })
																				 .entity();
	(void)onRelationWildcardSet;
	wld.modify_raw(source, pair);
	CHECK(relationWildcardSetRows == 1);
	CHECK(targetWildcardSetRows == 3);
	CHECK(setObserverCalls == 5);

	uint32_t anyPairSetRows = 0;
	const auto onAnyPairSet = wld.observer()
																.event(ecs::ObserverEvent::OnSet)
																.all(ecs::Pair(ecs::All, ecs::All))
																.on_each([&](ecs::Entity entity) {
																	const bool expectedEntity = entity == source || entity == taggedSource;
																	CHECK(expectedEntity);
																	++anyPairSetRows;
																})
																.entity();
	(void)onAnyPairSet;
	wld.modify_raw(source, pair);
	wld.modify_raw(taggedSource, taggedPair);
	CHECK(anyPairSetRows == 2);
	CHECK(relationWildcardSetRows == 2);
	CHECK(targetWildcardSetRows == 5);
	CHECK(setObserverCalls == 6);

	auto& uniqueTargetItem = add_runtime_component(
			wld, "Runtime_Pair_Target_Unique", PayloadSize, ecs::DataStorageType::Table, alignof(float), 0, nullptr,
			ecs::ComponentLookupHash{}, ecs::EntityKind::EK_Uni, fields, 2);
	const auto uniqueRelation = wld.add();
	const auto uniquePair = (ecs::Entity)ecs::Pair(uniqueRelation, uniqueTargetItem.entity);
	const auto uniqueSourceA = wld.add();
	const auto uniqueSourceB = wld.add();
	float uniqueInitialA[] = {10.0f, 11.0f};
	float uniqueInitialB[] = {12.0f, 13.0f};
	CHECK(wld.comp_cache().find_pair_payload(uniquePair) == &uniqueTargetItem);
	CHECK(wld.add_raw(uniqueSourceA, uniquePair, uniqueInitialA, sizeof(uniqueInitialA)));
	CHECK(wld.add_raw(uniqueSourceB, uniquePair, uniqueInitialB, sizeof(uniqueInitialB)));
	const auto uniqueViewA = wld.get_raw(uniqueSourceA, uniquePair);
	const auto uniqueViewB = wld.get_raw(uniqueSourceB, uniquePair);
	CHECK(uniqueViewA.valid());
	CHECK(uniqueViewB.valid());
	CHECK(read_f32(uniqueViewA.data, XOffset) == doctest::Approx(12.0f));
	CHECK(read_f32(uniqueViewB.data, YOffset) == doctest::Approx(13.0f));
}

TEST_CASE("Runtime pair payloads use relation metadata across erased APIs") {
	constexpr uint32_t PayloadSize = sizeof(float) * 3;
	constexpr uint32_t XOffset = 0;
	constexpr uint32_t YOffset = sizeof(float);
	constexpr uint32_t ZOffset = sizeof(float) * 2;
	const ecs::RuntimeFieldDesc fields[] = {
			{util::str_view("x"), ecs::F32, XOffset, 0},
			{util::str_view("y"), ecs::F32, YOffset, 0},
			{util::str_view("z"), ecs::F32, ZOffset, 0}};

	auto write_f32 = [](void* data, uint32_t offset, float value) {
		memcpy((uint8_t*)data + offset, &value, sizeof(value));
	};
	auto read_f32 = [](const void* data, uint32_t offset) {
		float value = 0.0f;
		memcpy(&value, (const uint8_t*)data + offset, sizeof(value));
		return value;
	};

	static ecs::Entity expectedRelation = ecs::EntityBad;
	static uint32_t ctorCalls = 0;
	static uint32_t dtorCalls = 0;
	static uint32_t addHookCalls = 0;
	static uint32_t delHookCalls = 0;
	static uint32_t setHookCalls = 0;
	ctorCalls = dtorCalls = addHookCalls = delHookCalls = setHookCalls = 0;

	TestWorld twld;
	ecs::ComponentDesc desc{};
	desc.name = util::str_view("Runtime_Pair_Relation");
	desc.size = PayloadSize;
	desc.alig = alignof(float);
	desc.storageType = ecs::DataStorageType::Table;
	desc.typeKind = ecs::RuntimeTypeKind::Struct;
	desc.fields = fields;
	desc.fieldCount = 3;
	desc.funcCtor = [](void* data, uint32_t count) {
		++ctorCalls;
		memset(data, 0, (uintptr_t)PayloadSize * count);
	};
	desc.funcDtor = [](void*, uint32_t) {
		++dtorCalls;
	};
	auto& relation = wld.add(desc);
	expectedRelation = relation.entity;

	ecs::ComponentDesc targetDesc{};
	targetDesc.name = util::str_view("Runtime_Pair_Competing_Target_Metadata");
	targetDesc.size = sizeof(uint64_t);
	targetDesc.alig = alignof(uint64_t);
	targetDesc.storageType = ecs::DataStorageType::Table;
	auto& targetItem = wld.add(targetDesc);
	const auto target = targetItem.entity;
	const auto otherTarget = wld.add();
	const auto pair = (ecs::Entity)ecs::Pair(relation.entity, target);
	const auto otherPair = (ecs::Entity)ecs::Pair(relation.entity, otherTarget);
	const auto source = wld.add();
	const auto otherSource = wld.add();

	CHECK(wld.comp_cache().find(pair) == nullptr);
	CHECK(wld.comp_cache().find_pair_payload(pair) == &relation);
	CHECK(wld.comp_cache().find_pair_payload(pair) != &targetItem);
	CHECK(wld.comp_cache().find_pair_payload(otherPair) == &relation);

#if GAIA_ENABLE_ADD_DEL_HOOKS
	auto& hooks = ecs::ComponentCache::hooks(relation);
	hooks.func_add = [](const ecs::World&, const ecs::ComponentCacheItem& item, ecs::Entity entity) {
		CHECK(item.entity == expectedRelation);
		CHECK(entity != ecs::EntityBad);
		++addHookCalls;
	};
	hooks.func_del = [](const ecs::World&, const ecs::ComponentCacheItem& item, ecs::Entity entity) {
		CHECK(item.entity == expectedRelation);
		CHECK(entity != ecs::EntityBad);
		++delHookCalls;
	};
#endif
#if GAIA_ENABLE_SET_HOOKS
	ecs::ComponentCache::hooks(relation).func_set = [](const ecs::World&, const ecs::ComponentRecord& rec, ecs::Chunk&) {
		CHECK(rec.pItem != nullptr);
		if (rec.pItem != nullptr)
			CHECK(rec.pItem->entity == expectedRelation);
		++setHookCalls;
	};
#endif

	uint32_t setObserverCalls = 0;
	const auto onSet = wld.observer()
												 .event(ecs::ObserverEvent::OnSet)
												 .all(pair)
												 .on_each([&](ecs::Entity entity) {
													 CHECK(entity == source);
													 ++setObserverCalls;
												 })
												 .entity();
	(void)onSet;

	float initial[] = {1.0f, 2.0f, 3.0f};
	const bool added = wld.add_raw(source, pair, initial, sizeof(initial));
	CHECK(added);
	if (!added)
		return;
	CHECK(ctorCalls == 1);
#if GAIA_ENABLE_ADD_DEL_HOOKS
	CHECK(addHookCalls == 1);
#endif

	const auto initialView = wld.get_raw(source, pair);
	CHECK(initialView.valid());
	if (!initialView.valid())
		return;
	CHECK(initialView.size == PayloadSize);
	CHECK(read_f32(initialView.data, XOffset) == doctest::Approx(1.0f));

	auto mutableView = wld.mut_raw(source, pair);
	CHECK(mutableView.valid());
	if (!mutableView.valid())
		return;
	write_f32(mutableView.data, YOffset, 4.0f);
	CHECK(setObserverCalls == 0);
	wld.modify_raw(source, pair);
	CHECK(setObserverCalls == 1);
#if GAIA_ENABLE_SET_HOOKS
	CHECK(setHookCalls == 1);
#endif

	float replacement[] = {5.0f, 6.0f, 7.0f};
	CHECK(wld.set_raw(source, pair, replacement, sizeof(replacement)));
	CHECK(setObserverCalls == 2);

	auto readCursor = wld.cursor(source, pair);
	CHECK(readCursor.valid());
	CHECK(readCursor.type() == pair);
	CHECK(readCursor.field(util::str_view("x")));
	const auto x = readCursor.f32();
	CHECK(x);
	CHECK(x.value == doctest::Approx(5.0f));

	auto cursor = wld.cursor_mut(source, pair);
	CHECK(cursor.valid());
	if (!cursor.valid())
		return;
	CHECK(cursor.type() == pair);
	CHECK(cursor.field(util::str_view("z")));
	CHECK(cursor.f32(8.0f));
	CHECK(setObserverCalls == 3);
#if GAIA_ENABLE_SET_HOOKS
	CHECK(setHookCalls == 3);
#endif

	uint32_t relationWildcardSetCalls = 0;
	const auto onRelationWildcardSet = wld.observer()
																				 .event(ecs::ObserverEvent::OnSet)
																				 .all(ecs::Pair(relation.entity, ecs::All))
																				 .on_each([&](ecs::Entity entity) {
																					 CHECK(entity == source);
																					 ++relationWildcardSetCalls;
																				 })
																				 .entity();
	(void)onRelationWildcardSet;
	uint32_t targetWildcardSetCalls = 0;
	const auto onTargetWildcardSet = wld.observer()
																			 .event(ecs::ObserverEvent::OnSet)
																			 .all(ecs::Pair(ecs::All, target))
																			 .on_each([&](ecs::Entity entity) {
																				 CHECK(entity == source);
																				 ++targetWildcardSetCalls;
																			 })
																			 .entity();
	(void)onTargetWildcardSet;
	uint32_t anyPairSetCalls = 0;
	const auto onAnyPairSet = wld.observer()
																.event(ecs::ObserverEvent::OnSet)
																.all(ecs::Pair(ecs::All, ecs::All))
																.on_each([&](ecs::Entity entity) {
																	CHECK(entity == source);
																	++anyPairSetCalls;
																})
																.entity();
	(void)onAnyPairSet;
	wld.modify_raw(source, pair);
	CHECK(relationWildcardSetCalls == 1);
	CHECK(targetWildcardSetCalls == 1);
	CHECK(anyPairSetCalls == 1);
	CHECK(setObserverCalls == 4);
#if GAIA_ENABLE_SET_HOOKS
	CHECK(setHookCalls == 4);
#endif

	float otherInitial[] = {10.0f, 11.0f, 12.0f};
	const bool otherAdded = wld.add_raw(otherSource, otherPair, otherInitial, sizeof(otherInitial));
	CHECK(otherAdded);
	if (!otherAdded)
		return;
	auto fixedQuery = wld.query().all(pair);
	CHECK(fixedQuery.count() == 1);
	fixedQuery.each([&](ecs::Iter& it) {
		const auto raw = it.view_raw(0);
		CHECK(raw.size() == it.size());
		if (raw.size() != it.size())
			return;
		GAIA_FOR(it.size()) CHECK(raw[i].valid());
	});

	uint32_t wildcardRows = 0;
	auto wildcardQuery = wld.query().all(ecs::Pair(relation.entity, ecs::All));
	wildcardQuery.each([&](ecs::Entity) {
		++wildcardRows;
	});
	CHECK(wildcardRows == 2);
	CHECK(setObserverCalls == 4);

	const auto uniqueTarget = wld.add<ecs::uni<Position>>().entity;
	const auto uniquePair = (ecs::Entity)ecs::Pair(relation.entity, uniqueTarget);
	const auto uniqueSourceA = wld.add();
	const auto uniqueSourceB = wld.add();
	float uniqueInitialA[] = {30.0f, 31.0f, 32.0f};
	float uniqueInitialB[] = {40.0f, 41.0f, 42.0f};
	CHECK(wld.add_raw(uniqueSourceA, uniquePair, uniqueInitialA, sizeof(uniqueInitialA)));
	CHECK(wld.add_raw(uniqueSourceB, uniquePair, uniqueInitialB, sizeof(uniqueInitialB)));
	const auto uniqueViewA = wld.get_raw(uniqueSourceA, uniquePair);
	const auto uniqueViewB = wld.get_raw(uniqueSourceB, uniquePair);
	CHECK(uniqueViewA.valid());
	CHECK(uniqueViewB.valid());
	CHECK(read_f32(uniqueViewA.data, XOffset) == doctest::Approx(40.0f));
	CHECK(read_f32(uniqueViewB.data, XOffset) == doctest::Approx(40.0f));

	const auto finalView = wld.get_raw(source, pair);
	CHECK(finalView.valid());
	if (!finalView.valid())
		return;
	CHECK(read_f32(finalView.data, ZOffset) == doctest::Approx(8.0f));

	wld.del(source, pair);
	CHECK_FALSE(wld.has(source, pair));
#if GAIA_ENABLE_ADD_DEL_HOOKS
	CHECK(delHookCalls == 1);
#endif
	CHECK(dtorCalls >= 1);
}

TEST_CASE("Runtime pair SoA fields expose direct field views and cursors") {
	constexpr uint32_t XOffset = 0;
	constexpr uint32_t CountOffset = sizeof(float);
	const uint8_t soaSizes[] = {sizeof(float), sizeof(uint32_t)};
	const ecs::RuntimeFieldDesc fields[] = {
			{util::str_view("x"), ecs::F32, XOffset, 0}, {util::str_view("count"), ecs::U32, CountOffset, 0}};

	TestWorld twld;
	ecs::ComponentDesc desc{};
	desc.name = util::str_view("Runtime_Pair_SoA_Relation");
	desc.size = sizeof(float) + sizeof(uint32_t);
	desc.alig = alignof(float);
	desc.storageType = ecs::DataStorageType::Table;
	desc.soa = 2;
	desc.pSoaSizes = soaSizes;
	desc.typeKind = ecs::RuntimeTypeKind::Struct;
	desc.fields = fields;
	desc.fieldCount = 2;
	auto& relation = wld.add(desc);

	const auto source = wld.add();
	const auto target = wld.add();
	const auto pairDesc = ecs::Pair(relation.entity, target);
	const auto pair = (ecs::Entity)pairDesc;
	wld.add(source, pairDesc);

	CHECK_FALSE(wld.get_raw(source, pair).valid());
	CHECK_FALSE(wld.mut_raw(source, pair).valid());

	auto xMut = wld.mut_raw_field(source, pair, 0);
	CHECK(xMut.valid());
	CHECK(xMut.size == sizeof(float));
	if (!xMut.valid())
		return;
	*(float*)xMut.data = 1.25f;

	auto countMut = wld.mut_raw_field(source, pair, 1);
	CHECK(countMut.valid());
	CHECK(countMut.size == sizeof(uint32_t));
	if (!countMut.valid())
		return;
	*(uint32_t*)countMut.data = 7;

	uint32_t setObserverCalls = 0;
	const auto onSet = wld.observer()
												 .event(ecs::ObserverEvent::OnSet)
												 .all(pair)
												 .on_each([&](ecs::Entity entity) {
													 CHECK(entity == source);
													 ++setObserverCalls;
												 })
												 .entity();
	(void)onSet;

	wld.modify_raw(source, pair);
	CHECK(setObserverCalls == 1);

	const auto xRead = wld.get_raw_field(source, pair, 0);
	CHECK(xRead.valid());
	CHECK(xRead.size == sizeof(float));
	if (!xRead.valid())
		return;
	CHECK(*(const float*)xRead.data == doctest::Approx(1.25f));

	const auto countRead = wld.get_raw_field(source, pair, 1);
	CHECK(countRead.valid());
	CHECK(countRead.size == sizeof(uint32_t));
	if (!countRead.valid())
		return;
	CHECK(*(const uint32_t*)countRead.data == 7);
	CHECK_FALSE(wld.get_raw_field(source, pair, 2).valid());

	auto cursor = wld.cursor_mut(source, pair);
	CHECK(cursor.valid());
	CHECK(cursor.type() == pair);
	CHECK(cursor.field(util::str_view("x")));
	CHECK(cursor.f32(2.5f));
	CHECK(setObserverCalls == 2);

	auto readCursor = wld.cursor(source, pair);
	CHECK(readCursor.valid());
	CHECK(readCursor.type() == pair);
	CHECK(readCursor.field(util::str_view("x")));
	const auto x = readCursor.f32();
	CHECK(x);
	if (x)
		CHECK(x.value == doctest::Approx(2.5f));

	CHECK(wld.acc(source).get_raw_field(pair, 1).valid());
	CHECK(wld.acc_mut(source).mut_raw_field(pair, 1).valid());

	ecs::ComponentDesc targetDesc = desc;
	targetDesc.name = util::str_view("Runtime_Pair_SoA_Target");
	auto& targetItem = wld.add(targetDesc);
	const auto targetRelation = wld.add();
	const auto targetSource = wld.add();
	const auto targetOwnedPairDesc = ecs::Pair(targetRelation, targetItem.entity);
	const auto targetOwnedPair = (ecs::Entity)targetOwnedPairDesc;
	CHECK(wld.comp_cache().find_pair_payload(targetOwnedPair) == &targetItem);
	wld.add(targetSource, targetOwnedPairDesc);

	uint32_t targetOwnedExactSetCalls = 0;
	const auto targetOwnedExactSet = wld.observer()
																			 .event(ecs::ObserverEvent::OnSet)
																			 .all(targetOwnedPair)
																			 .on_each([&](ecs::Entity entity) {
																				 CHECK(entity == targetSource);
																				 ++targetOwnedExactSetCalls;
																			 })
																			 .entity();
	(void)targetOwnedExactSet;
	uint32_t targetOwnedRelationWildcardSetCalls = 0;
	const auto targetOwnedRelationWildcardSet = wld.observer()
																									.event(ecs::ObserverEvent::OnSet)
																									.all(ecs::Pair(targetRelation, ecs::All))
																									.on_each([&](ecs::Entity entity) {
																										CHECK(entity == targetSource);
																										++targetOwnedRelationWildcardSetCalls;
																									})
																									.entity();
	(void)targetOwnedRelationWildcardSet;
	uint32_t targetOwnedTargetWildcardSetCalls = 0;
	const auto targetOwnedTargetWildcardSet = wld.observer()
																								.event(ecs::ObserverEvent::OnSet)
																								.all(ecs::Pair(ecs::All, targetItem.entity))
																								.on_each([&](ecs::Entity entity) {
																									CHECK(entity == targetSource);
																									++targetOwnedTargetWildcardSetCalls;
																								})
																								.entity();
	(void)targetOwnedTargetWildcardSet;
	uint32_t targetOwnedAnyPairSetCalls = 0;
	const auto targetOwnedAnyPairSet = wld.observer()
																				 .event(ecs::ObserverEvent::OnSet)
																				 .all(ecs::Pair(ecs::All, ecs::All))
																				 .on_each([&](ecs::Entity entity) {
																					 CHECK(entity == targetSource);
																					 ++targetOwnedAnyPairSetCalls;
																				 })
																				 .entity();
	(void)targetOwnedAnyPairSet;

	CHECK_FALSE(wld.get_raw(targetSource, targetOwnedPair).valid());
	CHECK_FALSE(wld.mut_raw(targetSource, targetOwnedPair).valid());

	auto targetXMut = wld.mut_raw_field(targetSource, targetOwnedPair, 0);
	CHECK(targetXMut.valid());
	CHECK(targetXMut.size == sizeof(float));
	if (!targetXMut.valid())
		return;
	*(float*)targetXMut.data = 4.5f;
	CHECK(targetOwnedExactSetCalls == 0);
	CHECK(targetOwnedRelationWildcardSetCalls == 0);
	CHECK(targetOwnedTargetWildcardSetCalls == 0);
	CHECK(targetOwnedAnyPairSetCalls == 0);
	wld.modify_raw(targetSource, targetOwnedPair);
	CHECK(targetOwnedExactSetCalls == 1);
	CHECK(targetOwnedRelationWildcardSetCalls == 1);
	CHECK(targetOwnedTargetWildcardSetCalls == 1);
	CHECK(targetOwnedAnyPairSetCalls == 1);

	const auto targetXRead = wld.get_raw_field(targetSource, targetOwnedPair, 0);
	CHECK(targetXRead.valid());
	CHECK(targetXRead.size == sizeof(float));
	if (!targetXRead.valid())
		return;
	CHECK(*(const float*)targetXRead.data == doctest::Approx(4.5f));

	auto targetCursor = wld.cursor_mut(targetSource, targetOwnedPair);
	CHECK(targetCursor.valid());
	CHECK(targetCursor.type() == targetOwnedPair);
	CHECK(targetCursor.field(util::str_view("count")));
	CHECK(targetCursor.u32(42));
	CHECK(targetOwnedExactSetCalls == 2);
	CHECK(targetOwnedRelationWildcardSetCalls == 2);
	CHECK(targetOwnedTargetWildcardSetCalls == 2);
	CHECK(targetOwnedAnyPairSetCalls == 2);
	const auto targetCountRead = wld.get_raw_field(targetSource, targetOwnedPair, 1);
	CHECK(targetCountRead.valid());
	if (targetCountRead.valid())
		CHECK(*(const uint32_t*)targetCountRead.data == 42);
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
	CHECK(containers.pair_record_try_add(pairHandle, GAIA_MOV(pairRecord)));
	CHECK(containers.pair_record_contains(pairHandle));
	const auto* pPairRecord = containers.pair_record_find(pairHandle);
	CHECK(pPairRecord != nullptr);
	if (pPairRecord != nullptr)
		CHECK(pPairRecord->row == 22);

	auto duplicatePairRecord = ecs::EntityContainer::create(pairHandle.id(), pairHandle.gen(), &pairCtx);
	duplicatePairRecord.row = 99;
	CHECK_FALSE(containers.pair_record_try_add(pairHandle, GAIA_MOV(duplicatePairRecord)));
	CHECK(containers[pairHandle].row == 22);

	CHECK(containers[entityHandle].row == 11);
	CHECK(containers[pairHandle].row == 22);

	const auto& containersConst = containers;
	CHECK(containersConst[entityHandle].row == 11);
	CHECK(containersConst[pairHandle].row == 22);

	ecs::EntityContainer removedPairRecord{};
	CHECK(containers.pair_record_remove(pairHandle, removedPairRecord));
	CHECK(removedPairRecord.row == 22);
	CHECK_FALSE(containers.pair_record_contains(pairHandle));
	CHECK_FALSE(containers.pair_record_remove(pairHandle, removedPairRecord));

	pairRecord = ecs::EntityContainer::create(pairHandle.id(), pairHandle.gen(), &pairCtx);
	pairRecord.row = 33;
	CHECK(containers.pair_record_try_add(pairHandle, GAIA_MOV(pairRecord)));
	CHECK(containers[pairHandle].row == 33);

	containers.pair_records_clear();
	CHECK_FALSE(containers.pair_record_contains(pairHandle));
}

TEST_CASE("Component helpers") {
	const ecs::Component denseComp(
			11, 0, (uint32_t)sizeof(Position), (uint32_t)alignof(Position), ecs::DataStorageType::Table);
	const ecs::Component sparseAosComp(
			12, 0, (uint32_t)sizeof(Position), (uint32_t)alignof(Position), ecs::DataStorageType::Sparse);
	const ecs::Component sparseSoaComp(
			13, 2, (uint32_t)sizeof(Position), (uint32_t)alignof(Position), ecs::DataStorageType::Sparse);
	const ecs::Component tagComp(14, 0, 0, 1, ecs::DataStorageType::Table);

	CHECK(ecs::component_uses_table_storage(denseComp));
	CHECK_FALSE(ecs::component_uses_sparse_storage(denseComp));
	CHECK_FALSE(ecs::component_uses_table_storage(sparseAosComp));
	CHECK(ecs::component_uses_sparse_storage(sparseAosComp));
	CHECK(ecs::component_uses_table_storage(sparseSoaComp));
	CHECK_FALSE(ecs::component_uses_sparse_storage(sparseSoaComp));
	CHECK_FALSE(ecs::component_uses_table_storage(tagComp));
	CHECK_FALSE(ecs::component_uses_sparse_storage(tagComp));

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

#include "test_common.h"

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

TEST_CASE("Serialization - json runtime fields") {
	struct JsonRuntimeComp {
		int32_t a;
		float b;
		bool c;
		char name[8];
	};
	constexpr uint32_t RuntimeJsonPayloadSize = 12;
	constexpr uint32_t RuntimeJsonXOffset = 0;
	constexpr uint32_t RuntimeJsonYOffset = 4;
	constexpr uint32_t RuntimeJsonZOffset = 8;
	auto write_runtime_json_xyz = [&](void* data, float x, float y, float z) {
		auto* bytes = (uint8_t*)data;
		memcpy(bytes + RuntimeJsonXOffset, &x, sizeof(x));
		memcpy(bytes + RuntimeJsonYOffset, &y, sizeof(y));
		memcpy(bytes + RuntimeJsonZOffset, &z, sizeof(z));
	};
	auto read_runtime_json_f32 = [](const void* data, uint32_t offset) {
		float value = 0.0f;
		memcpy(&value, (const uint8_t*)data + offset, sizeof(value));
		return value;
	};

	SUBCASE("runtime fields are emitted and loaded through reflected metadata") {
		TestWorld twld;
		const ecs::RuntimeFieldDesc fields[] = {
				{util::str_view("x"), ecs::F32, RuntimeJsonXOffset, 0}, //
				{util::str_view("y"), ecs::F32, RuntimeJsonYOffset, 0}, //
				{util::str_view("z"), ecs::F32, RuntimeJsonZOffset, 0} //
		};
		auto& item = add_runtime_component_with_fields(
				wld, "Runtime_Component_Json_Runtime_Fields", RuntimeJsonPayloadSize, ecs::DataStorageType::Table, 4, fields,
				3);

		uint8_t value[RuntimeJsonPayloadSize]{};
		write_runtime_json_xyz(value, 1.25f, -2.5f, 3.0f);

		ser::ser_json writer;
		const bool ok = ecs::component_to_json(item, value, writer);
		CHECK(ok);
		CHECK(writer.str() == "{\"x\":1.25,\"y\":-2.5,\"z\":3}");

		uint8_t out[RuntimeJsonPayloadSize]{};
		ser::ser_json reader("{\"x\":4.5,\"y\":5.25,\"z\":-6.75}");
		bool okRead = false;
		CHECK(ecs::json_to_component(item, out, reader, okRead));
		CHECK(okRead);
		CHECK(read_runtime_json_f32(out, RuntimeJsonXOffset) == doctest::Approx(4.5f));
		CHECK(read_runtime_json_f32(out, RuntimeJsonYOffset) == doctest::Approx(5.25f));
		CHECK(read_runtime_json_f32(out, RuntimeJsonZOffset) == doctest::Approx(-6.75f));
	}

	SUBCASE("runtime fields are emitted as json object") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		const auto offB = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.b) - pBase);
		const auto offC = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.c) - pBase);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);

		RuntimeFieldSet fields;
		CHECK(add_runtime_field(fields, "a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));
		CHECK(add_runtime_field(fields, "b", 0, ser::serialization_type_id::f32, offB, (uint32_t)sizeof(layout.b)));
		CHECK(add_runtime_field(fields, "c", 0, ser::serialization_type_id::b, offC, (uint32_t)sizeof(layout.c)));
		CHECK(add_runtime_field(fields, "name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));
		(void)add_runtime_component_with_fields(
				cc, entity, "Runtime_Component_Json", (uint32_t)sizeof(JsonRuntimeComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRuntimeComp), fields.fields, fields.count);
		auto& item = cc.get(entity);

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

	SUBCASE("nested struct fields are emitted using runtime field names and offsets") {
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

		TransformLike layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offId = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.id) - pBase);
		const auto offPosX = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.position.x) - pBase);
		const auto offPosY = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.position.y) - pBase);
		const auto offPosZ = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.position.z) - pBase);
		const auto offVelX = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.velocity.x) - pBase);
		const auto offVelY = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.velocity.y) - pBase);
		const auto offVelZ = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.velocity.z) - pBase);

		RuntimeFieldSet fields;
		CHECK(add_runtime_field(fields, "id", 0, ser::serialization_type_id::s32, offId, (uint32_t)sizeof(layout.id)));
		CHECK(add_runtime_field(
				fields, "position.x", 0, ser::serialization_type_id::f32, offPosX, (uint32_t)sizeof(layout.position.x)));
		CHECK(add_runtime_field(
				fields, "position.y", 0, ser::serialization_type_id::f32, offPosY, (uint32_t)sizeof(layout.position.y)));
		CHECK(add_runtime_field(
				fields, "position.z", 0, ser::serialization_type_id::f32, offPosZ, (uint32_t)sizeof(layout.position.z)));
		CHECK(add_runtime_field(
				fields, "velocity.x", 0, ser::serialization_type_id::f32, offVelX, (uint32_t)sizeof(layout.velocity.x)));
		CHECK(add_runtime_field(
				fields, "velocity.y", 0, ser::serialization_type_id::f32, offVelY, (uint32_t)sizeof(layout.velocity.y)));
		CHECK(add_runtime_field(
				fields, "velocity.z", 0, ser::serialization_type_id::f32, offVelZ, (uint32_t)sizeof(layout.velocity.z)));
		(void)add_runtime_component_with_fields(
				cc, entity, "Runtime_Component_Json_Nested", (uint32_t)sizeof(TransformLike), ecs::DataStorageType::Table,
				(uint32_t)alignof(TransformLike), fields.fields, fields.count);
		auto& item = cc.get(entity);

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

	SUBCASE("array and array-of-struct fields are emitted using runtime field names and offsets") {
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

		RuntimeFieldSet fields;
		CHECK(add_runtime_field(
				fields, "weights[0]", 0, ser::serialization_type_id::f32, offW0, (uint32_t)sizeof(layout.weights[0])));
		CHECK(add_runtime_field(
				fields, "weights[1]", 0, ser::serialization_type_id::f32, offW1, (uint32_t)sizeof(layout.weights[1])));
		CHECK(add_runtime_field(
				fields, "weights[2]", 0, ser::serialization_type_id::f32, offW2, (uint32_t)sizeof(layout.weights[2])));
		CHECK(add_runtime_field(
				fields, "inventory[0].id", 0, ser::serialization_type_id::u16, offInv0Id,
				(uint32_t)sizeof(layout.inventory[0].id)));
		CHECK(add_runtime_field(
				fields, "inventory[0].count", 0, ser::serialization_type_id::u8, offInv0Count,
				(uint32_t)sizeof(layout.inventory[0].count)));
		CHECK(add_runtime_field(
				fields, "inventory[1].id", 0, ser::serialization_type_id::u16, offInv1Id,
				(uint32_t)sizeof(layout.inventory[1].id)));
		CHECK(add_runtime_field(
				fields, "inventory[1].count", 0, ser::serialization_type_id::u8, offInv1Count,
				(uint32_t)sizeof(layout.inventory[1].count)));
		CHECK(add_runtime_field(
				fields, "active[0]", 0, ser::serialization_type_id::b, offActive0, (uint32_t)sizeof(layout.active[0])));
		CHECK(add_runtime_field(
				fields, "active[1]", 0, ser::serialization_type_id::b, offActive1, (uint32_t)sizeof(layout.active[1])));
		(void)add_runtime_component_with_fields(
				cc, entity, "Runtime_Component_Json_Arrays", (uint32_t)sizeof(RuntimeArraysComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(RuntimeArraysComp), fields.fields, fields.count);
		auto& item = cc.get(entity);

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

	SUBCASE("nested runtime metadata is emitted and loaded as semantic objects and arrays") {
		struct Vec3 {
			float x, y, z;
		};
		struct RuntimeTransformComp {
			int32_t id;
			Vec3 position;
			Vec3 samples[2];
			uint32_t mode;
		};

		TestWorld twld;
		RuntimeTransformComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offId = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.id) - pBase);
		const auto offPosition = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.position) - pBase);
		const auto offSamples = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.samples) - pBase);
		const auto offMode = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.mode) - pBase);

		const ecs::RuntimeFieldDesc vec3Fields[] = {
				{util::str_view("x"), ecs::F32, 0, 0}, //
				{util::str_view("y"), ecs::F32, 4, 0}, //
				{util::str_view("z"), ecs::F32, 8, 0} //
		};
		ecs::ComponentDesc vec3Desc{};
		vec3Desc.name = runtime_component_name_view("Runtime_Type_Json_Vec3");
		vec3Desc.size = (uint32_t)sizeof(Vec3);
		vec3Desc.alig = (uint32_t)alignof(Vec3);
		vec3Desc.typeKind = ecs::RuntimeTypeKind::Struct;
		vec3Desc.fields = vec3Fields;
		vec3Desc.fieldCount = 3;
		auto& vec3Type = wld.add(vec3Desc);

		ecs::ComponentDesc vec3ArrayDesc{};
		vec3ArrayDesc.name = runtime_component_name_view("Runtime_Type_Json_Vec3_Array_2");
		vec3ArrayDesc.size = (uint32_t)sizeof(Vec3) * 2;
		vec3ArrayDesc.alig = (uint32_t)alignof(Vec3);
		vec3ArrayDesc.typeKind = ecs::RuntimeTypeKind::Array;
		vec3ArrayDesc.elementType = vec3Type.entity;
		vec3ArrayDesc.elementCount = 2;
		auto& vec3ArrayType = wld.add(vec3ArrayDesc);

		const ecs::RuntimeConstantDesc modeConstants[] = {
				{util::str_view("Idle"), 0}, //
				{util::str_view("Move"), 1}, //
				{util::str_view("Jump"), 2} //
		};
		ecs::ComponentDesc modeDesc{};
		modeDesc.name = runtime_component_name_view("Runtime_Type_Json_Mode");
		modeDesc.size = (uint32_t)sizeof(uint32_t);
		modeDesc.alig = (uint32_t)alignof(uint32_t);
		modeDesc.typeKind = ecs::RuntimeTypeKind::Enum;
		modeDesc.underlyingType = ecs::U32;
		modeDesc.constants = modeConstants;
		modeDesc.constantCount = 3;
		auto& modeType = wld.add(modeDesc);

		const ecs::RuntimeFieldDesc fields[] = {
				{util::str_view("id"), ecs::S32, offId, 0}, //
				{util::str_view("position"), vec3Type.entity, offPosition, 0}, //
				{util::str_view("samples"), vec3ArrayType.entity, offSamples, 0}, //
				{util::str_view("mode"), modeType.entity, offMode, 0} //
		};
		auto& item = add_runtime_component_with_fields(
				wld, "Runtime_Component_Json_Semantic_Nested", (uint32_t)sizeof(RuntimeTransformComp),
				ecs::DataStorageType::Table, (uint32_t)alignof(RuntimeTransformComp), fields, 4);

		RuntimeTransformComp value{};
		value.id = 7;
		value.position = {1.25f, -2.5f, 3.0f};
		value.samples[0] = {4.0f, 5.5f, 6.25f};
		value.samples[1] = {-7.0f, 8.0f, 9.5f};
		value.mode = 2;

		ser::ser_json writer;
		const bool ok = ecs::component_to_json(item, &value, writer);
		CHECK(ok);
		CHECK(
				writer.str() ==
				"{\"id\":7,\"position\":{\"x\":1.25,\"y\":-2.5,\"z\":3},\"samples\":[{\"x\":4,\"y\":5.5,\"z\":6.25},"
				"{\"x\":-7,\"y\":8,\"z\":9.5}],\"mode\":2}");

		RuntimeTransformComp out{};
		ser::ser_json reader(writer.str().data(), (uint32_t)writer.str().size());
		bool okRead = false;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK(okRead);
		reader.ws();
		CHECK(reader.eof());
		CHECK(out.id == value.id);
		CHECK(out.position.x == value.position.x);
		CHECK(out.position.y == value.position.y);
		CHECK(out.position.z == value.position.z);
		CHECK(out.samples[0].x == value.samples[0].x);
		CHECK(out.samples[0].y == value.samples[0].y);
		CHECK(out.samples[0].z == value.samples[0].z);
		CHECK(out.samples[1].x == value.samples[1].x);
		CHECK(out.samples[1].y == value.samples[1].y);
		CHECK(out.samples[1].z == value.samples[1].z);
		CHECK(out.mode == value.mode);
	}

	SUBCASE("recursive named runtime arrays are emitted and loaded as nested arrays") {
		struct Vec3 {
			float x, y, z;
		};
		struct RuntimeGridComp {
			Vec3 grid[2][2];
		};

		TestWorld twld;
		RuntimeGridComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offGrid = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.grid) - pBase);

		const ecs::RuntimeFieldDesc vec3Fields[] = {
				{util::str_view("x"), ecs::F32, 0, 0}, //
				{util::str_view("y"), ecs::F32, 4, 0}, //
				{util::str_view("z"), ecs::F32, 8, 0} //
		};
		ecs::ComponentDesc vec3Desc{};
		vec3Desc.name = runtime_component_name_view("Runtime_Type_Json_Recursive_Vec3");
		vec3Desc.size = (uint32_t)sizeof(Vec3);
		vec3Desc.alig = (uint32_t)alignof(Vec3);
		vec3Desc.typeKind = ecs::RuntimeTypeKind::Struct;
		vec3Desc.fields = vec3Fields;
		vec3Desc.fieldCount = 3;
		auto& vec3Type = wld.add(vec3Desc);

		ecs::ComponentDesc rowDesc{};
		rowDesc.name = runtime_component_name_view("Runtime_Type_Json_Recursive_Vec3_Row_2");
		rowDesc.size = (uint32_t)sizeof(Vec3) * 2;
		rowDesc.alig = (uint32_t)alignof(Vec3);
		rowDesc.typeKind = ecs::RuntimeTypeKind::Array;
		rowDesc.elementType = vec3Type.entity;
		rowDesc.elementCount = 2;
		auto& rowType = wld.add(rowDesc);

		ecs::ComponentDesc gridTypeDesc{};
		gridTypeDesc.name = runtime_component_name_view("Runtime_Type_Json_Recursive_Vec3_Grid_2x2");
		gridTypeDesc.size = (uint32_t)sizeof(RuntimeGridComp);
		gridTypeDesc.alig = (uint32_t)alignof(Vec3);
		gridTypeDesc.typeKind = ecs::RuntimeTypeKind::Array;
		gridTypeDesc.elementType = rowType.entity;
		gridTypeDesc.elementCount = 2;
		auto& gridType = wld.add(gridTypeDesc);

		const ecs::RuntimeFieldDesc fields[] = {
				{util::str_view("grid"), gridType.entity, offGrid, 0} //
		};
		auto& item = add_runtime_component_with_fields(
				wld, "Runtime_Component_Json_Recursive_Array", (uint32_t)sizeof(RuntimeGridComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(RuntimeGridComp), fields, 1);

		RuntimeGridComp value{};
		value.grid[0][0] = {1.0f, 2.0f, 3.0f};
		value.grid[0][1] = {4.0f, 5.0f, 6.0f};
		value.grid[1][0] = {7.0f, 8.0f, 9.0f};
		value.grid[1][1] = {10.0f, 11.0f, 12.0f};

		ser::ser_json writer;
		const bool ok = ecs::component_to_json(item, &value, writer);
		CHECK(ok);
		CHECK(
				writer.str() == "{\"grid\":[[{\"x\":1,\"y\":2,\"z\":3},{\"x\":4,\"y\":5,\"z\":6}],"
												"[{\"x\":7,\"y\":8,\"z\":9},{\"x\":10,\"y\":11,\"z\":12}]]}");

		RuntimeGridComp out{};
		ser::ser_json reader(writer.str().data(), (uint32_t)writer.str().size());
		bool okRead = false;
		CHECK(ecs::json_to_component(item, &out, reader, okRead));
		CHECK(okRead);
		reader.ws();
		CHECK(reader.eof());
		CHECK(out.grid[0][0].x == value.grid[0][0].x);
		CHECK(out.grid[0][1].z == value.grid[0][1].z);
		CHECK(out.grid[1][0].y == value.grid[1][0].y);
		CHECK(out.grid[1][1].z == value.grid[1][1].z);
	}

	SUBCASE("json_to_component loads runtime field payload") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		const auto offB = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.b) - pBase);
		const auto offC = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.c) - pBase);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);
		RuntimeFieldSet fields;
		CHECK(add_runtime_field(fields, "a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));
		CHECK(add_runtime_field(fields, "b", 0, ser::serialization_type_id::f32, offB, (uint32_t)sizeof(layout.b)));
		CHECK(add_runtime_field(fields, "c", 0, ser::serialization_type_id::b, offC, (uint32_t)sizeof(layout.c)));
		CHECK(add_runtime_field(fields, "name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));
		(void)add_runtime_component_with_fields(
				cc, entity, "Runtime_Component_Json_Read", (uint32_t)sizeof(JsonRuntimeComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRuntimeComp), fields.fields, fields.count);
		auto& item = cc.get(entity);

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
		(void)add_runtime_component(
				cc, entity, "Runtime_Component_Json_Raw_Read", (uint32_t)sizeof(JsonRawComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRawComp));
		auto& item = cc.get(entity);

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

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		const auto offB = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.b) - pBase);
		const auto offC = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.c) - pBase);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);
		RuntimeFieldSet fields;
		CHECK(add_runtime_field(fields, "a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));
		CHECK(add_runtime_field(fields, "b", 0, ser::serialization_type_id::f32, offB, (uint32_t)sizeof(layout.b)));
		CHECK(add_runtime_field(fields, "c", 0, ser::serialization_type_id::b, offC, (uint32_t)sizeof(layout.c)));
		CHECK(add_runtime_field(fields, "name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));
		(void)add_runtime_component_with_fields(
				cc, entity, "Runtime_Component_Json_Unknown_Fields", (uint32_t)sizeof(JsonRuntimeComp),
				ecs::DataStorageType::Table, (uint32_t)alignof(JsonRuntimeComp), fields.fields, fields.count);
		auto& item = cc.get(entity);

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

	SUBCASE("json_to_component fails on malformed runtime field payload type") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		RuntimeFieldSet fields;
		CHECK(add_runtime_field(fields, "a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));
		(void)add_runtime_component_with_fields(
				cc, entity, "Runtime_Component_Json_Bad_Type", (uint32_t)sizeof(JsonRuntimeComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRuntimeComp), fields.fields, fields.count);
		auto& item = cc.get(entity);

		JsonRuntimeComp out{};
		ser::ser_json reader("{\"a\":\"bad\"}");
		bool okRead = true;
		CHECK_FALSE(ecs::json_to_component(item, &out, reader, okRead));
	}

	SUBCASE("json_to_component treats null field values as best-effort failures") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();

		JsonRuntimeComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offA = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.a) - pBase);
		RuntimeFieldSet fields;
		CHECK(add_runtime_field(fields, "a", 0, ser::serialization_type_id::s32, offA, (uint32_t)sizeof(layout.a)));
		(void)add_runtime_component_with_fields(
				cc, entity, "Runtime_Component_Json_Null_Field", (uint32_t)sizeof(JsonRuntimeComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRuntimeComp), fields.fields, fields.count);
		auto& item = cc.get(entity);

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

		JsonIntsComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offU8 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.u8) - pBase);
		const auto offU16 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.u16) - pBase);
		const auto offS8 = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.s8) - pBase);
		RuntimeFieldSet fields;
		CHECK(add_runtime_field(fields, "u8", 0, ser::serialization_type_id::u8, offU8, (uint32_t)sizeof(layout.u8)));
		CHECK(add_runtime_field(fields, "u16", 0, ser::serialization_type_id::u16, offU16, (uint32_t)sizeof(layout.u16)));
		CHECK(add_runtime_field(fields, "s8", 0, ser::serialization_type_id::s8, offS8, (uint32_t)sizeof(layout.s8)));
		(void)add_runtime_component_with_fields(
				cc, entity, "Runtime_Component_Json_Ints", (uint32_t)sizeof(JsonIntsComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonIntsComp), fields.fields, fields.count);
		auto& item = cc.get(entity);

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

		JsonNameComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);
		RuntimeFieldSet fields;
		CHECK(add_runtime_field(fields, "name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));
		(void)add_runtime_component_with_fields(
				cc, entity, "Runtime_Component_Json_String_Truncate", (uint32_t)sizeof(JsonNameComp),
				ecs::DataStorageType::Table, (uint32_t)alignof(JsonNameComp), fields.fields, fields.count);
		auto& item = cc.get(entity);

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
		(void)add_runtime_component(
				cc, entity, "Runtime_Component_Json_Raw_Malformed", (uint32_t)sizeof(JsonRawComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRawComp));
		auto& item = cc.get(entity);

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

	SUBCASE("json_to_component marks payloads without runtime fields or raw data as best-effort failures") {
		struct JsonRawComp {
			uint32_t a;
		};

		TestWorld twld;
		auto& cc = wld.comp_cache_mut();
		const auto entity = wld.add();
		(void)add_runtime_component(
				cc, entity, "Runtime_Component_Json_No_Data", (uint32_t)sizeof(JsonRawComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonRawComp));
		auto& item = cc.get(entity);

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

		JsonDiagComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offValue = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.value) - pBase);
		const auto offName = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.name) - pBase);
		RuntimeFieldSet fields;
		CHECK(add_runtime_field(
				fields, "value", 0, ser::serialization_type_id::u8, offValue, (uint32_t)sizeof(layout.value)));
		CHECK(add_runtime_field(fields, "name", 0, ser::serialization_type_id::c8, offName, (uint32_t)sizeof(layout.name)));
		(void)add_runtime_component_with_fields(
				cc, entity, "Runtime_Component_Json_Diagnostics", (uint32_t)sizeof(JsonDiagComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonDiagComp), fields.fields, fields.count);
		auto& item = cc.get(entity);

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

TEST_CASE("Serialization - world preserves Parent non-fragmenting relations") {
	ecs::World in;

	const auto root = in.add();
	const auto child = in.add();
	in.name(root, "Root");
	in.name(child, "Child");
	in.parent(child, root);

	CHECK(in.has(ecs::Pair(ecs::Parent, root)));
	CHECK(in.has(child, ecs::Pair(ecs::Parent, root)));
	CHECK(in.target(child, ecs::Parent) == root);

	ser::bin_stream buffer;
	in.set_serializer(buffer);
	in.save();

	TestWorld twld;
	CHECK(wld.load(buffer));

	const auto loadedRoot = wld.get("Root");
	const auto loadedChild = wld.get("Child");
	CHECK(loadedRoot == root);
	CHECK(loadedChild == child);
	CHECK(wld.has(ecs::Pair(ecs::Parent, loadedRoot)));
	CHECK(wld.has(loadedChild, ecs::Pair(ecs::Parent, loadedRoot)));
	CHECK(wld.target(loadedChild, ecs::Parent) == loadedRoot);

	cnt::darray<ecs::Entity> sources;
	wld.sources(ecs::Parent, loadedRoot, [&](ecs::Entity source) {
		sources.push_back(source);
	});
	CHECK(sources.size() == 1);
	if (!sources.empty())
		CHECK(sources[0] == loadedChild);
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
	CHECK(json.find("\"Position\":{\"$raw\":[") != BadIndex);
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

TEST_CASE("Serialization - world json runtime fields nested arrays") {
	struct Vec3 {
		float x, y, z;
	};
	struct JsonComplexComp {
		Vec3 transform[2];
		uint16_t itemCounts[3];
		bool active[2];
		char label[12];
	};

	auto register_component = [](ecs::World& world) -> ecs::ComponentCacheItem& {
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

		RuntimeFieldSet fields;
		CHECK(add_runtime_field(
				fields, "transform[0].x", 0, ser::serialization_type_id::f32, offT0X, (uint32_t)sizeof(layout.transform[0].x)));
		CHECK(add_runtime_field(
				fields, "transform[0].y", 0, ser::serialization_type_id::f32, offT0Y, (uint32_t)sizeof(layout.transform[0].y)));
		CHECK(add_runtime_field(
				fields, "transform[0].z", 0, ser::serialization_type_id::f32, offT0Z, (uint32_t)sizeof(layout.transform[0].z)));
		CHECK(add_runtime_field(
				fields, "transform[1].x", 0, ser::serialization_type_id::f32, offT1X, (uint32_t)sizeof(layout.transform[1].x)));
		CHECK(add_runtime_field(
				fields, "transform[1].y", 0, ser::serialization_type_id::f32, offT1Y, (uint32_t)sizeof(layout.transform[1].y)));
		CHECK(add_runtime_field(
				fields, "transform[1].z", 0, ser::serialization_type_id::f32, offT1Z, (uint32_t)sizeof(layout.transform[1].z)));
		CHECK(add_runtime_field(
				fields, "itemCounts[0]", 0, ser::serialization_type_id::u16, offCount0,
				(uint32_t)sizeof(layout.itemCounts[0])));
		CHECK(add_runtime_field(
				fields, "itemCounts[1]", 0, ser::serialization_type_id::u16, offCount1,
				(uint32_t)sizeof(layout.itemCounts[1])));
		CHECK(add_runtime_field(
				fields, "itemCounts[2]", 0, ser::serialization_type_id::u16, offCount2,
				(uint32_t)sizeof(layout.itemCounts[2])));
		CHECK(add_runtime_field(
				fields, "active[0]", 0, ser::serialization_type_id::b, offActive0, (uint32_t)sizeof(layout.active[0])));
		CHECK(add_runtime_field(
				fields, "active[1]", 0, ser::serialization_type_id::b, offActive1, (uint32_t)sizeof(layout.active[1])));
		CHECK(add_runtime_field(
				fields, "label", 0, ser::serialization_type_id::c8, offLabel, (uint32_t)sizeof(layout.label)));

		return add_runtime_component_with_fields(
				world, "Runtime_Component_Json_Complex", (uint32_t)sizeof(JsonComplexComp), ecs::DataStorageType::Table,
				(uint32_t)alignof(JsonComplexComp), fields.fields, fields.count);
	};

	TestWorld twld;
	auto& comp = register_component(wld);

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
	CHECK(wld.add_raw(e, comp.entity, &value, (uint32_t)sizeof(value)));
	wld.name(e, "ComplexEntity");

	ser::ser_json writer;
	CHECK(wld.save_json(writer));
	const auto& json = writer.str();
	const auto compSymbol = wld.symbol(comp.entity);
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
	auto& compOut = register_component(twldOut.m_w);
	CHECK(twldOut.m_w.load_json(json));

	const auto loaded = twldOut.m_w.get("ComplexEntity");
	CHECK(loaded != ecs::EntityBad);
	const auto loadedPayload = twldOut.m_w.get_raw(loaded, compOut.entity);
	CHECK(loadedPayload.valid());
	CHECK(loadedPayload.size == sizeof(JsonComplexComp));
	CHECK(loadedPayload.data != nullptr);
	JsonComplexComp loadedComp{};
	memcpy(&loadedComp, loadedPayload.data, sizeof(loadedComp));
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
}

TEST_CASE("Serialization - world json runtime semantic nested metadata") {
	struct Vec3 {
		float x, y, z;
	};
	struct RuntimeTransformComp {
		int32_t id;
		Vec3 position;
		Vec3 samples[2];
		uint32_t mode;
	};

	auto register_component = [](ecs::World& world) -> ecs::ComponentCacheItem& {
		RuntimeTransformComp layout{};
		const auto* pBase = reinterpret_cast<const uint8_t*>(&layout);
		const auto offId = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.id) - pBase);
		const auto offPosition = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.position) - pBase);
		const auto offSamples = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.samples) - pBase);
		const auto offMode = (uint32_t)(reinterpret_cast<const uint8_t*>(&layout.mode) - pBase);

		const ecs::RuntimeFieldDesc vec3Fields[] = {
				{util::str_view("x"), ecs::F32, 0, 0}, //
				{util::str_view("y"), ecs::F32, 4, 0}, //
				{util::str_view("z"), ecs::F32, 8, 0} //
		};
		ecs::ComponentDesc vec3Desc{};
		vec3Desc.name = runtime_component_name_view("Runtime_Type_World_Json_Vec3");
		vec3Desc.size = (uint32_t)sizeof(Vec3);
		vec3Desc.alig = (uint32_t)alignof(Vec3);
		vec3Desc.typeKind = ecs::RuntimeTypeKind::Struct;
		vec3Desc.fields = vec3Fields;
		vec3Desc.fieldCount = 3;
		auto& vec3Type = world.add(vec3Desc);

		ecs::ComponentDesc vec3ArrayDesc{};
		vec3ArrayDesc.name = runtime_component_name_view("Runtime_Type_World_Json_Vec3_Array_2");
		vec3ArrayDesc.size = (uint32_t)sizeof(Vec3) * 2;
		vec3ArrayDesc.alig = (uint32_t)alignof(Vec3);
		vec3ArrayDesc.typeKind = ecs::RuntimeTypeKind::Array;
		vec3ArrayDesc.elementType = vec3Type.entity;
		vec3ArrayDesc.elementCount = 2;
		auto& vec3ArrayType = world.add(vec3ArrayDesc);

		const ecs::RuntimeConstantDesc modeConstants[] = {
				{util::str_view("Idle"), 0}, //
				{util::str_view("Move"), 1}, //
				{util::str_view("Jump"), 2} //
		};
		ecs::ComponentDesc modeDesc{};
		modeDesc.name = runtime_component_name_view("Runtime_Type_World_Json_Mode");
		modeDesc.size = (uint32_t)sizeof(uint32_t);
		modeDesc.alig = (uint32_t)alignof(uint32_t);
		modeDesc.typeKind = ecs::RuntimeTypeKind::Enum;
		modeDesc.underlyingType = ecs::U32;
		modeDesc.constants = modeConstants;
		modeDesc.constantCount = 3;
		auto& modeType = world.add(modeDesc);

		const ecs::RuntimeFieldDesc fields[] = {
				{util::str_view("id"), ecs::S32, offId, 0}, //
				{util::str_view("position"), vec3Type.entity, offPosition, 0}, //
				{util::str_view("samples"), vec3ArrayType.entity, offSamples, 0}, //
				{util::str_view("mode"), modeType.entity, offMode, 0} //
		};
		return add_runtime_component_with_fields(
				world, "Runtime_Component_World_Json_Semantic_Nested", (uint32_t)sizeof(RuntimeTransformComp),
				ecs::DataStorageType::Table, (uint32_t)alignof(RuntimeTransformComp), fields, 4);
	};

	TestWorld twld;
	auto& comp = register_component(wld);
	const auto e = wld.add();
	wld.name(e, "SemanticNestedEntity");
	RuntimeTransformComp value{};
	value.id = 7;
	value.position = {1.25f, -2.5f, 3.0f};
	value.samples[0] = {4.0f, 5.5f, 6.25f};
	value.samples[1] = {-7.0f, 8.0f, 9.5f};
	value.mode = 2;
	CHECK(wld.add_raw(e, comp.entity, &value, (uint32_t)sizeof(value)));

	ser::ser_json writer;
	CHECK(wld.save_json(writer, ser::JsonSaveFlags::RawFallback));
	const auto& json = writer.str();
	CHECK(json.find("\"binary\":[") == BadIndex);
	CHECK(json.find("\"SemanticNestedEntity\"") != BadIndex);
	CHECK(json.find("\"position\":{\"x\":1.25,\"y\":-2.5,\"z\":3}") != BadIndex);
	CHECK(json.find("\"samples\":[{\"x\":4,\"y\":5.5,\"z\":6.25},{\"x\":-7,\"y\":8,\"z\":9.5}]") != BadIndex);
	CHECK(json.find("\"mode\":2") != BadIndex);

	TestWorld twldOut;
	auto& compOut = register_component(twldOut.m_w);
	CHECK(twldOut.m_w.load_json(json));

	const auto loaded = twldOut.m_w.get("SemanticNestedEntity");
	CHECK(loaded != ecs::EntityBad);
	const auto loadedPayload = twldOut.m_w.get_raw(loaded, compOut.entity);
	CHECK(loadedPayload.valid());
	CHECK(loadedPayload.size == sizeof(RuntimeTransformComp));
	CHECK(loadedPayload.data != nullptr);
	RuntimeTransformComp loadedComp{};
	memcpy(&loadedComp, loadedPayload.data, sizeof(loadedComp));
	CHECK(loadedComp.id == value.id);
	CHECK(loadedComp.position.x == value.position.x);
	CHECK(loadedComp.position.y == value.position.y);
	CHECK(loadedComp.position.z == value.position.z);
	CHECK(loadedComp.samples[0].x == value.samples[0].x);
	CHECK(loadedComp.samples[0].y == value.samples[0].y);
	CHECK(loadedComp.samples[0].z == value.samples[0].z);
	CHECK(loadedComp.samples[1].x == value.samples[1].x);
	CHECK(loadedComp.samples[1].y == value.samples[1].y);
	CHECK(loadedComp.samples[1].z == value.samples[1].z);
	CHECK(loadedComp.mode == value.mode);
}

TEST_CASE("Serialization - world json runtime-created components") {
	constexpr const char* RuntimeJsonComponentName = "Runtime_Component_World_Json";
	constexpr uint32_t RuntimeJsonPayloadSize = 12;
	constexpr uint32_t RuntimeJsonXOffset = 0;
	constexpr uint32_t RuntimeJsonYOffset = 4;
	constexpr uint32_t RuntimeJsonZOffset = 8;

	auto write_xyz = [&](void* data, float x, float y, float z) {
		auto* bytes = (uint8_t*)data;
		memcpy(bytes + RuntimeJsonXOffset, &x, sizeof(x));
		memcpy(bytes + RuntimeJsonYOffset, &y, sizeof(y));
		memcpy(bytes + RuntimeJsonZOffset, &z, sizeof(z));
	};

	auto read_f32 = [](const void* data, uint32_t offset) {
		float value = 0.0f;
		memcpy(&value, (const uint8_t*)data + offset, sizeof(value));
		return value;
	};

	auto register_runtime_component = [&](ecs::World& world) -> ecs::ComponentCacheItem& {
		const ecs::RuntimeFieldDesc fields[] = {
				{util::str_view("x"), ecs::F32, RuntimeJsonXOffset, 0}, //
				{util::str_view("y"), ecs::F32, RuntimeJsonYOffset, 0}, //
				{util::str_view("z"), ecs::F32, RuntimeJsonZOffset, 0} //
		};
		return add_runtime_component_with_fields(
				world, RuntimeJsonComponentName, RuntimeJsonPayloadSize, ecs::DataStorageType::Table, 4, fields, 3);
	};

	TestWorld twld;
	auto& runtimeComp = register_runtime_component(wld);
	const auto entity = wld.add();
	wld.name(entity, "RuntimeJsonEntity");

	uint8_t payload[RuntimeJsonPayloadSize]{};
	write_xyz(payload, 1.25f, -2.5f, 3.0f);
	CHECK(wld.add_raw(entity, runtimeComp.entity, payload, RuntimeJsonPayloadSize));

	ser::ser_json writer;
	CHECK(wld.save_json(writer, ser::JsonSaveFlags::RawFallback));
	const auto& json = writer.str();
	CHECK(json.find("\"binary\":[") == BadIndex);
	CHECK(json.find("\"RuntimeJsonEntity\"") != BadIndex);
	CHECK(json.find("\"Runtime_Component_World_Json\":{\"x\":1.25,\"y\":-2.5,\"z\":3}") != BadIndex);

	TestWorld twldOut;
	auto& runtimeCompOut = register_runtime_component(twldOut.m_w);
	CHECK(twldOut.m_w.load_json(json));

	const auto loaded = twldOut.m_w.get("RuntimeJsonEntity");
	CHECK(loaded != ecs::EntityBad);

	const auto loadedPayload = twldOut.m_w.get_raw(loaded, runtimeCompOut.entity);
	CHECK(loadedPayload.valid());
	CHECK(loadedPayload.size == RuntimeJsonPayloadSize);
	CHECK(loadedPayload.data != nullptr);
	CHECK(read_f32(loadedPayload.data, RuntimeJsonXOffset) == doctest::Approx(1.25f));
	CHECK(read_f32(loadedPayload.data, RuntimeJsonYOffset) == doctest::Approx(-2.5f));
	CHECK(read_f32(loadedPayload.data, RuntimeJsonZOffset) == doctest::Approx(3.0f));

	uint32_t hits = 0;
	auto q = twldOut.m_w.query().all(runtimeCompOut.entity);
	q.each([&](ecs::Entity matched) {
		CHECK(matched == loaded);
		const auto queriedPayload = twldOut.m_w.get_raw(matched, runtimeCompOut.entity);
		CHECK(queriedPayload.valid());
		CHECK(queriedPayload.size == RuntimeJsonPayloadSize);
		CHECK(queriedPayload.data != nullptr);
		CHECK(read_f32(queriedPayload.data, RuntimeJsonXOffset) == doctest::Approx(1.25f));
		++hits;
	});
	CHECK(hits == 1);
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

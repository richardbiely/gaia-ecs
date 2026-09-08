#include "test_common.h"

#include <thread>

namespace {
	//! Marker type used by tests to request World::uquery().
	struct QueryUncached {};

	struct PagedAllocatorProbe {
		uint32_t value = 0;
	};

	struct CmdBufRelPayload {
		float x;
		float y;
	};
	struct CmdBufRelTarget {};
	using CmdBufRelPair = ecs::pair<CmdBufRelPayload, CmdBufRelTarget>;

	struct CmdBufCtorTag {
		inline static const void* instance = nullptr;
		CmdBufCtorTag() {
			instance = this;
		}
	};

	//! Counts sparse-store initialization during table archetype transitions.
	struct SparseCtorProbe {
		GAIA_STORAGE(Sparse);
		inline static uint32_t ctorCalls = 0;
		uint32_t value;

		SparseCtorProbe(): value(17) {
			++ctorCalls;
		}
	};

	struct CmdBufPartialPayload {
		int serialized = 0;
		int omitted = 7;

		template <typename Serializer>
		void save(Serializer& serializer) const {
			serializer.save(serialized);
		}
		template <typename Serializer>
		void load(Serializer& serializer) {
			serializer.load(serialized);
		}
	};

	struct SmallFuncLargeCallable {
		uint32_t* pValue = nullptr;
		uint8_t payload[128]{};

		void operator()() {
			GAIA_ASSERT(pValue != nullptr);
			++(*pValue);
		}
	};

	struct SmallFuncTrackedSmallCallable {
		uint32_t* pCalls = nullptr;
		uint32_t* pDtors = nullptr;

		SmallFuncTrackedSmallCallable(uint32_t& calls, uint32_t& dtors): pCalls(&calls), pDtors(&dtors) {}
		SmallFuncTrackedSmallCallable(SmallFuncTrackedSmallCallable&& other) noexcept:
				pCalls(other.pCalls), pDtors(other.pDtors) {
			other.pCalls = nullptr;
			other.pDtors = nullptr;
		}
		SmallFuncTrackedSmallCallable(const SmallFuncTrackedSmallCallable&) = delete;
		SmallFuncTrackedSmallCallable& operator=(SmallFuncTrackedSmallCallable&&) = delete;
		SmallFuncTrackedSmallCallable& operator=(const SmallFuncTrackedSmallCallable&) = delete;

		~SmallFuncTrackedSmallCallable() {
			if (pDtors != nullptr)
				++(*pDtors);
		}

		void operator()() {
			GAIA_ASSERT(pCalls != nullptr);
			++(*pCalls);
		}
	};

	struct SmallFuncTrackedLargeCallable {
		uint32_t* pCalls = nullptr;
		uint32_t* pDtors = nullptr;
		uint8_t payload[128]{};

		SmallFuncTrackedLargeCallable(uint32_t& calls, uint32_t& dtors): pCalls(&calls), pDtors(&dtors) {}
		SmallFuncTrackedLargeCallable(SmallFuncTrackedLargeCallable&& other) noexcept:
				pCalls(other.pCalls), pDtors(other.pDtors) {
			other.pCalls = nullptr;
			other.pDtors = nullptr;
		}
		SmallFuncTrackedLargeCallable(const SmallFuncTrackedLargeCallable&) = delete;
		SmallFuncTrackedLargeCallable& operator=(SmallFuncTrackedLargeCallable&&) = delete;
		SmallFuncTrackedLargeCallable& operator=(const SmallFuncTrackedLargeCallable&) = delete;

		~SmallFuncTrackedLargeCallable() {
			if (pDtors != nullptr)
				++(*pDtors);
		}

		void operator()() {
			GAIA_ASSERT(pCalls != nullptr);
			++(*pCalls);
		}
	};

	struct SmallFuncTrackedHeapCallable {
		uint32_t* pCalls = nullptr;
		uint32_t* pDtors = nullptr;
		uint8_t payload[640]{};

		SmallFuncTrackedHeapCallable(uint32_t& calls, uint32_t& dtors): pCalls(&calls), pDtors(&dtors) {}
		SmallFuncTrackedHeapCallable(SmallFuncTrackedHeapCallable&& other) noexcept:
				pCalls(other.pCalls), pDtors(other.pDtors) {
			other.pCalls = nullptr;
			other.pDtors = nullptr;
		}
		SmallFuncTrackedHeapCallable(const SmallFuncTrackedHeapCallable&) = delete;
		SmallFuncTrackedHeapCallable& operator=(SmallFuncTrackedHeapCallable&&) = delete;
		SmallFuncTrackedHeapCallable& operator=(const SmallFuncTrackedHeapCallable&) = delete;

		~SmallFuncTrackedHeapCallable() {
			if (pDtors != nullptr)
				++(*pDtors);
		}

		void operator()() {
			GAIA_ASSERT(pCalls != nullptr);
			++(*pCalls);
		}
	};

	struct MoveFuncTrackedSmallCallable {
		int32_t* pCalls = nullptr;
		int32_t* pDtors = nullptr;
		int32_t bias = 0;

		MoveFuncTrackedSmallCallable(int32_t& calls, int32_t& dtors, int32_t value):
				pCalls(&calls), pDtors(&dtors), bias(value) {}
		MoveFuncTrackedSmallCallable(MoveFuncTrackedSmallCallable&& other) noexcept:
				pCalls(other.pCalls), pDtors(other.pDtors), bias(other.bias) {
			other.pCalls = nullptr;
			other.pDtors = nullptr;
		}
		MoveFuncTrackedSmallCallable(const MoveFuncTrackedSmallCallable&) = delete;
		MoveFuncTrackedSmallCallable& operator=(MoveFuncTrackedSmallCallable&&) = delete;
		MoveFuncTrackedSmallCallable& operator=(const MoveFuncTrackedSmallCallable&) = delete;

		~MoveFuncTrackedSmallCallable() {
			if (pDtors != nullptr)
				++(*pDtors);
		}

		int32_t operator()(int32_t lhs, int32_t rhs) {
			GAIA_ASSERT(pCalls != nullptr);
			++(*pCalls);
			return lhs + rhs + bias;
		}
	};

	struct MoveFuncTrackedLargeCallable {
		int32_t* pCalls = nullptr;
		int32_t* pDtors = nullptr;
		int32_t bias = 0;
		uint8_t payload[128]{};

		MoveFuncTrackedLargeCallable(int32_t& calls, int32_t& dtors, int32_t value):
				pCalls(&calls), pDtors(&dtors), bias(value) {}
		MoveFuncTrackedLargeCallable(MoveFuncTrackedLargeCallable&& other) noexcept:
				pCalls(other.pCalls), pDtors(other.pDtors), bias(other.bias) {
			other.pCalls = nullptr;
			other.pDtors = nullptr;
		}
		MoveFuncTrackedLargeCallable(const MoveFuncTrackedLargeCallable&) = delete;
		MoveFuncTrackedLargeCallable& operator=(MoveFuncTrackedLargeCallable&&) = delete;
		MoveFuncTrackedLargeCallable& operator=(const MoveFuncTrackedLargeCallable&) = delete;

		~MoveFuncTrackedLargeCallable() {
			if (pDtors != nullptr)
				++(*pDtors);
		}

		int32_t operator()(int32_t lhs, int32_t rhs) {
			GAIA_ASSERT(pCalls != nullptr);
			++(*pCalls);
			return lhs + rhs + bias;
		}
	};

	struct MoveFuncTrackedHeapCallable {
		int32_t* pCalls = nullptr;
		int32_t* pDtors = nullptr;
		int32_t bias = 0;
		uint8_t payload[640]{};

		MoveFuncTrackedHeapCallable(int32_t& calls, int32_t& dtors, int32_t value):
				pCalls(&calls), pDtors(&dtors), bias(value) {}
		MoveFuncTrackedHeapCallable(MoveFuncTrackedHeapCallable&& other) noexcept:
				pCalls(other.pCalls), pDtors(other.pDtors), bias(other.bias) {
			other.pCalls = nullptr;
			other.pDtors = nullptr;
		}
		MoveFuncTrackedHeapCallable(const MoveFuncTrackedHeapCallable&) = delete;
		MoveFuncTrackedHeapCallable& operator=(MoveFuncTrackedHeapCallable&&) = delete;
		MoveFuncTrackedHeapCallable& operator=(const MoveFuncTrackedHeapCallable&) = delete;

		~MoveFuncTrackedHeapCallable() {
			if (pDtors != nullptr)
				++(*pDtors);
		}

		int32_t operator()(int32_t lhs, int32_t rhs) {
			GAIA_ASSERT(pCalls != nullptr);
			++(*pCalls);
			return lhs + rhs + bias;
		}
	};

	struct SmallBlockMacroSmallObject {
		uint32_t value = 0;
		uint32_t* pDtors = nullptr;
		uint8_t payload[64]{};

		GAIA_USE_SMALLBLOCK(SmallBlockMacroSmallObject)

		SmallBlockMacroSmallObject() = default;
		SmallBlockMacroSmallObject(uint32_t v, uint32_t& dtors): value(v), pDtors(&dtors) {}

		~SmallBlockMacroSmallObject() {
			if (pDtors != nullptr)
				++(*pDtors);
		}
	};
} // namespace

TEST_CASE("add_n") {
	TestWorld twld;

	auto qa = wld.query().all<Acceleration>();
	auto qp = wld.query().all<Position>();

	auto e = wld.add();
	wld.add<Acceleration>(e, {1.f, 1.f, 1.f});
	wld.add<Position>(e, {2.f, 2.f, 2.f});

	constexpr uint32_t N = 1000;

	wld.add_n(e, N);

	{
		uint32_t cnt = 0;
		qa.each([&](ecs::Entity ent, const Acceleration& a) {
			++cnt;

			if (ent == e) {
				CHECK(a.x == 1.f);
				CHECK(a.y == 1.f);
				CHECK(a.z == 1.f);
			} else {
				// CHECK(a.x == 0.f);
				// CHECK(a.y == 0.f);
				// CHECK(a.z == 0.f);
			}
		});
		CHECK(cnt == N + 1);
	}
	{
		uint32_t cnt = 0;
		qp.each([&](ecs::Entity ent, const Position& p) {
			++cnt;

			if (ent == e) {
				CHECK(p.x == 2.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 2.f);
			} else {
				// CHECK(p.x == 0.f);
				// CHECK(p.y == 0.f);
				// CHECK(p.z == 0.f);
			}
		});
		CHECK(cnt == N + 1);
	}
}

TEST_CASE("copy_n") {
	TestWorld twld;

	auto qa = wld.query().all<Acceleration>();
	auto qp = wld.query().all<Position>();

	auto e = wld.add();
	wld.add<Acceleration>(e, {1.f, 1.f, 1.f});
	wld.add<Position>(e, {2.f, 2.f, 2.f});

	{
		const auto& a = wld.get<Acceleration>(e);
		CHECK(a.x == 1.f);
		CHECK(a.y == 1.f);
		CHECK(a.z == 1.f);
	}
	{
		const auto& p = wld.get<Position>(e);
		CHECK(p.x == 2.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 2.f);
	}

	constexpr uint32_t N = 1000;

	wld.copy_n(e, N);

	{
		uint32_t cnt = 0;
		qa.each([&](const Acceleration& a) {
			++cnt;

			CHECK(a.x == 1.f);
			CHECK(a.y == 1.f);
			CHECK(a.z == 1.f);
		});
		CHECK(cnt == N + 1);
	}
	{
		uint32_t cnt = 0;
		qp.each([&](const Position& p) {
			++cnt;

			CHECK(p.x == 2.f);
			CHECK(p.y == 2.f);
			CHECK(p.z == 2.f);
		});
		CHECK(cnt == N + 1);
	}
}

TEST_CASE("Set") {
	TestWorld twld;

	constexpr uint32_t N = 100;
	constexpr uint32_t NE = 10;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);
	cnt::darr<ecs::Entity> arre;
	arr.reserve(NE);

	GAIA_FOR(NE) {
		auto e = wld.add();
		arre.push_back(e);
	}

	GAIA_FOR(N) {
		const auto ent = wld.add();
		arr.push_back(ent);
		wld.add<Position>(ent, {});
		wld.add<Rotation>(ent, {});
		wld.add<Scale>(ent, {});
		wld.add<Acceleration>(ent, {});
		wld.add<Else>(ent, {});
		GAIA_FOR_(NE, j) {
			auto e = arre[j];
			wld.add(ent, e);
		}
	}

	// Default values
	for (const auto ent: arr) {
		auto r = wld.get<Rotation>(ent);
		CHECK(r.x == 0.f);
		CHECK(r.y == 0.f);
		CHECK(r.z == 0.f);
		CHECK(r.w == 0.f);

		auto s = wld.get<Scale>(ent);
		CHECK(s.x == 0.f);
		CHECK(s.y == 0.f);
		CHECK(s.z == 0.f);

		auto e = wld.get<Else>(ent);
		CHECK(e.value == false);
	}

	// Modify values
	{
		ecs::Query q = wld.query().all<Rotation&>().all<Scale&>().all<Else&>();

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view_mut<Rotation>();
			auto scaleView = it.view_mut<Scale>();
			auto elseView = it.view_mut<Else>();

			GAIA_EACH(it) {
				rotationView[i] = {1, 2, 3, 4};
				scaleView[i] = {11, 22, 33};
				elseView[i] = {true};
			}
		});

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view<Rotation>();
			auto scaleView = it.view<Scale>();
			auto elseView = it.view<Else>();

			GAIA_EACH(it) {
				auto r = rotationView[i];
				CHECK(r.x == 1.f);
				CHECK(r.y == 2.f);
				CHECK(r.z == 3.f);
				CHECK(r.w == 4.f);

				auto s = scaleView[i];
				CHECK(s.x == 11.f);
				CHECK(s.y == 22.f);
				CHECK(s.z == 33.f);

				auto e = elseView[i];
				CHECK(e.value == true);
			}
		});

		{
			uint32_t entIdx = 0;
			q.each([&](ecs::Entity ent) {
				CHECK(ent == arr[entIdx++]);
			});
			entIdx = 0;
			q.each([&](ecs::Iter& it) {
				auto entityView = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					CHECK(entityView[i] == arr[entIdx++]);
				}
			});
		}

		for (const auto ent: arr) {
			auto r = wld.get<Rotation>(ent);
			CHECK(r.x == 1.f);
			CHECK(r.y == 2.f);
			CHECK(r.z == 3.f);
			CHECK(r.w == 4.f);

			auto s = wld.get<Scale>(ent);
			CHECK(s.x == 11.f);
			CHECK(s.y == 22.f);
			CHECK(s.z == 33.f);

			auto e = wld.get<Else>(ent);
			CHECK(e.value == true);
		}
	}

	// Modify values + view idx
	{
		ecs::Query q = wld.query().all<Rotation&>().all<Scale&>().all<Else&>();

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view_mut<Rotation>(0);
			auto scaleView = it.view_mut<Scale>(1);
			auto elseView = it.view_mut<Else>(2);

			GAIA_EACH(it) {
				rotationView[i] = {1, 2, 3, 4};
				scaleView[i] = {11, 22, 33};
				elseView[i] = {true};
			}
		});

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view<Rotation>(0);
			auto scaleView = it.view<Scale>(1);
			auto elseView = it.view<Else>(2);

			GAIA_EACH(it) {
				auto r = rotationView[i];
				CHECK(r.x == 1.f);
				CHECK(r.y == 2.f);
				CHECK(r.z == 3.f);
				CHECK(r.w == 4.f);

				auto s = scaleView[i];
				CHECK(s.x == 11.f);
				CHECK(s.y == 22.f);
				CHECK(s.z == 33.f);

				auto e = elseView[i];
				CHECK(e.value == true);
			}
		});

		for (const auto ent: arr) {
			auto r = wld.get<Rotation>(ent);
			CHECK(r.x == 1.f);
			CHECK(r.y == 2.f);
			CHECK(r.z == 3.f);
			CHECK(r.w == 4.f);

			auto s = wld.get<Scale>(ent);
			CHECK(s.x == 11.f);
			CHECK(s.y == 22.f);
			CHECK(s.z == 33.f);

			auto e = wld.get<Else>(ent);
			CHECK(e.value == true);
		}
	}

	// Add one more component and check if the values are still fine after creating a new archetype
	{
		auto ent = wld.copy(arr[0]);
		wld.add<Position>(ent, {5, 6, 7});

		auto r = wld.get<Rotation>(ent);
		CHECK(r.x == 1.f);
		CHECK(r.y == 2.f);
		CHECK(r.z == 3.f);
		CHECK(r.w == 4.f);

		auto s = wld.get<Scale>(ent);
		CHECK(s.x == 11.f);
		CHECK(s.y == 22.f);
		CHECK(s.z == 33.f);

		auto e = wld.get<Else>(ent);
		CHECK(e.value == true);
	}
}

TEST_CASE("Set") {
	TestWorld twld;

	constexpr uint32_t N = 100;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	GAIA_FOR(N) {
		arr.push_back(wld.add());
		wld.add<Rotation>(arr.back(), {});
		wld.add<Scale>(arr.back(), {});
		wld.add<Else>(arr.back(), {});
	}

	// Default values
	for (const auto ent: arr) {
		auto r = wld.get<Rotation>(ent);
		CHECK(r.x == 0.f);
		CHECK(r.y == 0.f);
		CHECK(r.z == 0.f);
		CHECK(r.w == 0.f);

		auto s = wld.get<Scale>(ent);
		CHECK(s.x == 0.f);
		CHECK(s.y == 0.f);
		CHECK(s.z == 0.f);

		auto e = wld.get<Else>(ent);
		CHECK(e.value == false);
	}

	// Modify values
	{
		ecs::Query q = wld.query().all<Rotation&>().all<Scale&>().all<Else&>();

		q.each([&](ecs::Iter& it) {
			auto rotationView = it.view_mut<Rotation>();
			auto scaleView = it.view_mut<Scale>();
			auto elseView = it.view_mut<Else>();

			GAIA_EACH(it) {
				rotationView[i] = {1, 2, 3, 4};
				scaleView[i] = {11, 22, 33};
				elseView[i] = {true};
			}
		});

		{
			for (const auto ent: arr) {
				auto r = wld.get<Rotation>(ent);
				CHECK(r.x == 1.f);
				CHECK(r.y == 2.f);
				CHECK(r.z == 3.f);
				CHECK(r.w == 4.f);

				auto s = wld.get<Scale>(ent);
				CHECK(s.x == 11.f);
				CHECK(s.y == 22.f);
				CHECK(s.z == 33.f);

				auto e = wld.get<Else>(ent);
				CHECK(e.value == true);
			}
		}
	}
}

TEST_CASE("Components - non trivial") {
	TestWorld twld;

	constexpr uint32_t N = 100;
	cnt::darr<ecs::Entity> arr;
	arr.reserve(N);

	GAIA_FOR(N) {
		arr.push_back(wld.add());
		wld.add<StringComponent>(arr.back(), {});
		wld.add<StringComponent2>(arr.back(), {});
		wld.add<PositionNonTrivial>(arr.back(), {});
	}

	// Default values
	for (const auto ent: arr) {
		const auto& s1 = wld.get<StringComponent>(ent);
		CHECK(s1.value.empty());

		{
			auto s2 = wld.get<StringComponent2>(ent);
			CHECK(s2.value == StringComponent2DefaultValue);
		}
		{
			const auto& s2 = wld.get<StringComponent2>(ent);
			CHECK(s2.value == StringComponent2DefaultValue);
		}

		const auto& p = wld.get<PositionNonTrivial>(ent);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);
	}

	// Modify values
	{
		ecs::Query q = wld.query().all<StringComponent&>().all<StringComponent2&>().all<PositionNonTrivial&>();

		q.each([&](ecs::Iter& it) {
			auto strView = it.view_mut<StringComponent>();
			auto str2View = it.view_mut<StringComponent2>();
			auto posView = it.view_mut<PositionNonTrivial>();

			GAIA_EACH(it) {
				strView[i] = {StringComponentDefaultValue};
				str2View[i].value.assign(StringComponent2DefaultValue_2);
				posView[i] = {111, 222, 333};
			}
		});

		q.each([&](ecs::Iter& it) {
			auto strView = it.view_mut<StringComponent>(0);
			auto str2View = it.view_mut<StringComponent2>(1);
			auto posView = it.view_mut<PositionNonTrivial>(2);

			GAIA_EACH(it) {
				strView[i] = {StringComponentDefaultValue};
				str2View[i].value.assign(StringComponent2DefaultValue_2);
				posView[i] = {111, 222, 333};
			}
		});

		for (const auto ent: arr) {
			const auto& s1 = wld.get<StringComponent>(ent);
			CHECK(s1.value == StringComponentDefaultValue);

			const auto& s2 = wld.get<StringComponent2>(ent);
			CHECK(s2.value == StringComponent2DefaultValue_2);

			const auto& p = wld.get<PositionNonTrivial>(ent);
			CHECK(p.x == 111.f);
			CHECK(p.y == 222.f);
			CHECK(p.z == 333.f);
		}
	}

	// Add one more component and check if the values are still fine after creating a new archetype
	{
		auto ent = wld.copy(arr[0]);
		wld.add<Position>(ent, {5, 6, 7});

		const auto& s1 = wld.get<StringComponent>(ent);
		CHECK(s1.value == StringComponentDefaultValue);

		const auto& s2 = wld.get<StringComponent2>(ent);
		CHECK(s2.value == StringComponent2DefaultValue_2);

		const auto& p = wld.get<PositionNonTrivial>(ent);
		CHECK(p.x == 111.f);
		CHECK(p.y == 222.f);
		CHECK(p.z == 333.f);
	}
}

#if GAIA_ECS_CHUNK_ALLOCATOR
TEST_CASE("ChunkAllocator") {
	SUBCASE("size class thresholds") {
		CHECK(ecs::mem_block_size_type(1) == 0);
		CHECK(ecs::mem_block_size_type(ecs::MinMemoryBlockSize) == 0);
		CHECK(ecs::mem_block_size_type(ecs::MinMemoryBlockSize + 1) == 1);
		CHECK(ecs::mem_block_size_type(ecs::MinMemoryBlockSize * 2) == 1);
		CHECK(ecs::mem_block_size_type(ecs::MinMemoryBlockSize * 2 + 1) == 2);
		CHECK(ecs::mem_block_size_type(ecs::MinMemoryBlockSize * 4) == 2);
		CHECK(ecs::mem_block_size_type(ecs::MinMemoryBlockSize * 4 + 1) == 3);
		CHECK(ecs::mem_block_size_type(ecs::MaxMemoryBlockSize) == 3);
	}

	SUBCASE("stats report used memory per size class") {
		auto& alloc = ecs::ChunkAllocator::get();
		alloc.flush(true);
		alloc.verify();

		constexpr auto NBlocks = ecs::detail::MemoryPage::NBlocks;

		void* p8k = alloc.alloc(ecs::MinMemoryBlockSize);
		void* p16k = alloc.alloc(ecs::MinMemoryBlockSize * 2);
		void* p32k = alloc.alloc(ecs::MinMemoryBlockSize * 4);
		void* p64k = alloc.alloc(ecs::MaxMemoryBlockSize);

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[0].num_pages == 1);
			CHECK(stats.stats[1].num_pages == 1);
			CHECK(stats.stats[2].num_pages == 1);
			CHECK(stats.stats[3].num_pages == 1);
			CHECK(stats.stats[0].num_pages_free == 1);
			CHECK(stats.stats[1].num_pages_free == 1);
			CHECK(stats.stats[2].num_pages_free == 1);
	#if GAIA_DEBUG
			CHECK(stats.stats[0].num_pages_empty == 0);
			CHECK(stats.stats[1].num_pages_empty == 0);
			CHECK(stats.stats[2].num_pages_empty == 0);
	#endif

			CHECK(stats.stats[0].mem_total == (uint64_t)ecs::mem_block_size(0) * NBlocks);
			CHECK(stats.stats[1].mem_total == (uint64_t)ecs::mem_block_size(1) * NBlocks);
			CHECK(stats.stats[2].mem_total == (uint64_t)ecs::mem_block_size(2) * NBlocks);
			CHECK(stats.stats[3].mem_total == (uint64_t)ecs::mem_block_size(3) * NBlocks);

			CHECK(stats.stats[0].mem_used == ecs::mem_block_size(0));
			CHECK(stats.stats[1].mem_used == ecs::mem_block_size(1));
			CHECK(stats.stats[2].mem_used == ecs::mem_block_size(2));
	#if GAIA_DEBUG
			CHECK(stats.stats[0].mem_requested == ecs::mem_block_size(0));
			CHECK(stats.stats[1].mem_requested == ecs::mem_block_size(1));
			CHECK(stats.stats[2].mem_requested == ecs::mem_block_size(2));
	#endif
		}

		alloc.free(p8k);
		alloc.free(p16k);
		alloc.free(p32k);
		alloc.free(p64k);
		alloc.verify();
		alloc.flush();

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[0].num_pages == 1);
			CHECK(stats.stats[1].num_pages == 1);
			CHECK(stats.stats[2].num_pages == 0);
			CHECK(stats.stats[3].num_pages == 0);
			CHECK(stats.stats[0].num_pages_free == 1);
			CHECK(stats.stats[1].num_pages_free == 1);
			CHECK(stats.stats[2].num_pages_free == 0);
	#if GAIA_DEBUG
			CHECK(stats.stats[0].num_pages_empty == 1);
			CHECK(stats.stats[1].num_pages_empty == 1);
			CHECK(stats.stats[2].num_pages_empty == 0);
	#endif
			CHECK(stats.stats[0].mem_total == (uint64_t)ecs::mem_block_size(0) * NBlocks);
			CHECK(stats.stats[1].mem_total == (uint64_t)ecs::mem_block_size(1) * NBlocks);
			CHECK(stats.stats[2].mem_total == 0);
			CHECK(stats.stats[3].mem_total == 0);
			CHECK(stats.stats[0].mem_used == 0);
			CHECK(stats.stats[1].mem_used == 0);
			CHECK(stats.stats[2].mem_used == 0);
	#if GAIA_DEBUG
			CHECK(stats.stats[0].mem_requested == 0);
			CHECK(stats.stats[1].mem_requested == 0);
			CHECK(stats.stats[2].mem_requested == 0);
	#endif
		}

		alloc.flush(true);
		alloc.verify();
		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[0].num_pages == 0);
			CHECK(stats.stats[1].num_pages == 0);
			CHECK(stats.stats[2].num_pages == 0);
			CHECK(stats.stats[3].num_pages == 0);
			CHECK(stats.stats[0].num_pages_free == 0);
			CHECK(stats.stats[1].num_pages_free == 0);
			CHECK(stats.stats[2].num_pages_free == 0);
			CHECK(stats.stats[3].num_pages_free == 0);
			CHECK(stats.stats[0].mem_total == 0);
			CHECK(stats.stats[1].mem_total == 0);
			CHECK(stats.stats[2].mem_total == 0);
			CHECK(stats.stats[3].mem_total == 0);
			CHECK(stats.stats[0].mem_used == 0);
			CHECK(stats.stats[1].mem_used == 0);
			CHECK(stats.stats[2].mem_used == 0);
	#if GAIA_DEBUG
			CHECK(stats.stats[0].mem_requested == 0);
			CHECK(stats.stats[1].mem_requested == 0);
			CHECK(stats.stats[2].mem_requested == 0);
			CHECK(stats.stats[3].mem_requested == 0);
			CHECK(stats.stats[0].num_pages_empty == 0);
			CHECK(stats.stats[1].num_pages_empty == 0);
			CHECK(stats.stats[2].num_pages_empty == 0);
	#endif
		}
	}

	SUBCASE("stats track full and spill pages") {
		auto& alloc = ecs::ChunkAllocator::get();
		alloc.flush(true);
		alloc.verify();

		constexpr auto NBlocks = ecs::detail::MemoryPage::NBlocks;

		void* blocks[NBlocks + 1]{};
		GAIA_FOR(NBlocks) {
			blocks[i] = alloc.alloc(ecs::MaxMemoryBlockSize);
		}

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[3].num_pages == 1);
			CHECK(stats.stats[3].num_pages_free == 0);
	#if GAIA_DEBUG
			CHECK(stats.stats[3].num_pages_empty == 0);
	#endif
			CHECK(stats.stats[3].mem_total == (uint64_t)ecs::mem_block_size(3) * NBlocks);
			CHECK(stats.stats[3].mem_used == (uint64_t)ecs::mem_block_size(3) * NBlocks);
	#if GAIA_DEBUG
			CHECK(stats.stats[3].mem_requested == (uint64_t)ecs::mem_block_size(3) * NBlocks);
	#endif
		}

		blocks[NBlocks] = alloc.alloc(ecs::MaxMemoryBlockSize);
		alloc.verify();
		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[3].num_pages == 2);
			CHECK(stats.stats[3].num_pages_free == 1);
	#if GAIA_DEBUG
			CHECK(stats.stats[3].num_pages_empty == 0);
	#endif
			CHECK(stats.stats[3].mem_total == (uint64_t)ecs::mem_block_size(3) * NBlocks * 2);
			CHECK(stats.stats[3].mem_used == (uint64_t)ecs::mem_block_size(3) * (NBlocks + 1));
	#if GAIA_DEBUG
			CHECK(stats.stats[3].mem_requested == (uint64_t)ecs::mem_block_size(3) * (NBlocks + 1));
	#endif
		}

		// Freeing a block from a full page should move it back to the free list.
		alloc.free(blocks[0]);
		blocks[0] = nullptr;
		alloc.verify();
		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[3].num_pages == 2);
			CHECK(stats.stats[3].num_pages_free == 2);
	#if GAIA_DEBUG
			CHECK(stats.stats[3].num_pages_empty == 0);
	#endif
			CHECK(stats.stats[3].mem_used == (uint64_t)ecs::mem_block_size(3) * NBlocks);
	#if GAIA_DEBUG
			CHECK(stats.stats[3].mem_requested == (uint64_t)ecs::mem_block_size(3) * NBlocks);
	#endif
		}

		for (void* p: blocks) {
			if (p != nullptr)
				alloc.free(p);
		}
		alloc.verify();
		alloc.flush();

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[3].num_pages == 0);
			CHECK(stats.stats[3].num_pages_free == 0);
	#if GAIA_DEBUG
			CHECK(stats.stats[3].num_pages_empty == 0);
	#endif
			CHECK(stats.stats[3].mem_total == 0);
			CHECK(stats.stats[3].mem_used == 0);
	#if GAIA_DEBUG
			CHECK(stats.stats[3].mem_requested == 0);
	#endif
		}

		alloc.flush(true);
		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[3].num_pages == 0);
			CHECK(stats.stats[3].num_pages_free == 0);
	#if GAIA_DEBUG
			CHECK(stats.stats[3].num_pages_empty == 0);
	#endif
			CHECK(stats.stats[3].mem_total == 0);
			CHECK(stats.stats[3].mem_used == 0);
	#if GAIA_DEBUG
			CHECK(stats.stats[3].mem_requested == 0);
	#endif
		}
	}

	#if GAIA_DEBUG
	SUBCASE("stats track requested bytes separately from size classes") {
		auto& alloc = ecs::ChunkAllocator::get();
		alloc.flush(true);

		void* p8k = alloc.alloc(64);
		void* p16k = alloc.alloc(ecs::MinMemoryBlockSize + 64);
		void* p32k = alloc.alloc(ecs::MinMemoryBlockSize * 2 + 64);

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[0].mem_used == ecs::mem_block_size(0));
			CHECK(stats.stats[1].mem_used == ecs::mem_block_size(1));
			CHECK(stats.stats[2].mem_used == ecs::mem_block_size(2));
			CHECK(stats.stats[0].mem_requested == 64);
			CHECK(stats.stats[1].mem_requested == ecs::MinMemoryBlockSize + 64);
			CHECK(stats.stats[2].mem_requested == ecs::MinMemoryBlockSize * 2 + 64);
		}

		alloc.free(p8k);
		alloc.free(p16k);
		alloc.free(p32k);
		alloc.flush(true);
	}
	#endif

	SUBCASE("chunk data starts at the requested alignment") {
		auto& alloc = ecs::ChunkAllocator::get();
		alloc.flush(true);
		alloc.verify();

		void* pChunkMem = alloc.alloc(ecs::MinMemoryBlockSize);
		const auto dataAddr = (uintptr_t)pChunkMem + ecs::Chunk::chunk_data_area_offset();
		CHECK(dataAddr % ecs::MemoryBlockAlignment == 0);

		alloc.free(pChunkMem);
		alloc.verify();
		alloc.flush(true);
	}

	SUBCASE("allocator prefers partial pages over empty warm pages") {
		auto& alloc = ecs::ChunkAllocator::get();
		alloc.flush(true);
		alloc.verify();

		constexpr auto NBlocks = ecs::detail::MemoryPage::NBlocks;
		void* blocks[NBlocks + 1]{};
		GAIA_FOR(NBlocks) {
			blocks[i] = alloc.alloc(ecs::MaxMemoryBlockSize);
		}
		blocks[NBlocks] = alloc.alloc(ecs::MaxMemoryBlockSize);

		GAIA_FOR(NBlocks) {
			alloc.free(blocks[i]);
			blocks[i] = nullptr;
		}
		alloc.verify();

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[3].num_pages == 2);
			CHECK(stats.stats[3].num_pages_free == 2);
	#if GAIA_DEBUG
			CHECK(stats.stats[3].num_pages_empty == 1);
	#endif
		}

		void* extra = alloc.alloc(ecs::MaxMemoryBlockSize);
		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[3].num_pages == 2);
			CHECK(stats.stats[3].num_pages_free == 2);
	#if GAIA_DEBUG
			CHECK(stats.stats[3].num_pages_empty == 1);
	#endif
			CHECK(stats.stats[3].mem_used == (uint64_t)ecs::mem_block_size(3) * 2);
		}

		alloc.free(blocks[NBlocks]);
		alloc.free(extra);
		alloc.verify();
		alloc.flush(true);
	}

	#if GAIA_DEBUG
	SUBCASE("freed blocks are poisoned and allocator verifies invariants") {
		auto& alloc = ecs::ChunkAllocator::get();
		alloc.flush(true);
		alloc.verify();

		void* p = alloc.alloc(128);
		auto* bytes = (const uint8_t*)p;
		alloc.free(p);
		alloc.verify();

		GAIA_FOR(32) {
			CHECK(bytes[i] == ecs::detail::MemoryPage::FreedBlockPattern);
		}

		alloc.flush(true);
		alloc.verify();
	}
	#endif

	// We do this mostly for code coverage
	{
		TestWorld twld;
		ecs::CommandBufferST cb(wld);
		auto mainEntity = wld.add();

		wld.add<Position>(mainEntity, {1, 2, 3});

		constexpr uint32_t M = 100000;
		(void)wld.copy_n(mainEntity, M);

		// delete all created entities
		auto q = wld.query().all<Position>();
		CHECK(q.count() == M + 1);
		q.each([&](ecs::Entity e) {
			cb.del(e);
		});

		cb.commit();
		wld.update();
		ecs::ChunkAllocator::get().flush();
	}

	// We do this just for code coverage.
	// Hide logging so it does not spam the results of unit testing.
	const auto logLevelBackup = util::g_logLevelMask;
	util::g_logLevelMask = 0;
	ecs::ChunkAllocator::get().diag();
	util::g_logLevelMask = logLevelBackup;
}
#endif

TEST_CASE("PagedAllocator") {
	using Alloc = mem::PagedAllocator<PagedAllocatorProbe, 64>;
	using Page = mem::MemoryPage<PagedAllocatorProbe, 64>;
	static_assert(Page::MemoryBlockBytes >= 64 + mem::MemoryBlockUsableOffset);
	auto& alloc = Alloc::get();
	alloc.flush();
	alloc.verify();

	void* p = alloc.alloc(0);
	CHECK(p != nullptr);
	CHECK((uintptr_t)p % mem::MemoryBlockAlignment == 0);

#if GAIA_DEBUG
	const auto* bytes = (const uint8_t*)p;
	auto* rawBlock = (uint8_t*)p - mem::MemoryBlockUsableOffset;
#endif

	alloc.free(p);
	alloc.verify();

#if GAIA_DEBUG
	GAIA_FOR(32) {
		CHECK(bytes[i] == mem::MemoryPage<PagedAllocatorProbe, 64>::FreedBlockPattern);
	}
	CHECK(
			(uintptr_t)mem::unaligned_ref<uintptr_t>{rawBlock} == mem::MemoryPage<PagedAllocatorProbe, 64>::FreedPageMarker);
#endif

	alloc.flush();
	alloc.verify();
}

TEST_CASE("SmallBlockAllocator") {
	SUBCASE("size class helpers cover the full range") {
		CHECK(mem::small_block_size_type(1) == 0);
		CHECK(mem::small_block_size_type(mem::SmallBlockAlignment) == 0);
		CHECK(mem::small_block_size_type(mem::SmallBlockAlignment + 1) == 1);
		CHECK(mem::small_block_size_type(mem::SmallBlockMaxSize) == mem::SmallBlockSizeTypeCount - 1);

		GAIA_FOR(mem::SmallBlockSizeTypeCount) {
			const auto expectedSize = (i + 1) * mem::SmallBlockGranularity;
			const auto classMin = i == 0 ? 1U : i * mem::SmallBlockGranularity + 1;
			CHECK(mem::small_block_size(i) == expectedSize);
			CHECK(mem::small_block_size_type(classMin) == i);
			CHECK(mem::small_block_size_type(expectedSize) == i);
		}
	}

	SUBCASE("allocations are aligned and same-class frees are reused") {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);
		alloc.verify();

		void* pSmall = alloc.alloc(1);
		void* pMid = alloc.alloc(128);
		void* pMax = alloc.alloc(mem::SmallBlockMaxSize);

		CHECK(pSmall != nullptr);
		CHECK(pMid != nullptr);
		CHECK(pMax != nullptr);
		CHECK((uintptr_t)pSmall % alignof(std::max_align_t) == 0);
		CHECK((uintptr_t)pMid % alignof(std::max_align_t) == 0);
		CHECK((uintptr_t)pMax % alignof(std::max_align_t) == 0);

		alloc.free(pSmall);
		alloc.free(pMax);
		alloc.free(pMid);
		alloc.verify();

		void* pMidReused = alloc.alloc(128);
		CHECK(pMidReused == pMid);
		alloc.free(pMidReused);

		alloc.flush(true);
		alloc.verify();
	}

	SUBCASE("stats report usage per size class") {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);
		alloc.verify();

		constexpr uint32_t NBlocks = mem::detail::SmallBlockPage::NBlocks;
		constexpr uint32_t SizeTypeSmall = mem::small_block_size_type(64);
		constexpr uint32_t SizeTypeLarge = mem::small_block_size_type(128);
		const uint64_t strideSmall = mem::detail::small_block_stride(SizeTypeSmall);
		const uint64_t strideLarge = mem::detail::small_block_stride(SizeTypeLarge);

		void* pSmall = alloc.alloc(64);
		void* pLarge = alloc.alloc(128);

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[SizeTypeSmall].num_pages == 1);
			CHECK(stats.stats[SizeTypeSmall].num_pages_free == 1);
			CHECK(stats.stats[SizeTypeSmall].mem_total == strideSmall * NBlocks);
			CHECK(stats.stats[SizeTypeSmall].mem_used == strideSmall);

			CHECK(stats.stats[SizeTypeLarge].num_pages == 1);
			CHECK(stats.stats[SizeTypeLarge].num_pages_free == 1);
			CHECK(stats.stats[SizeTypeLarge].mem_total == strideLarge * NBlocks);
			CHECK(stats.stats[SizeTypeLarge].mem_used == strideLarge);
#if GAIA_DEBUG
			CHECK(stats.stats[SizeTypeSmall].mem_requested == 64);
			CHECK(stats.stats[SizeTypeLarge].mem_requested == 128);
			CHECK(stats.stats[SizeTypeSmall].num_pages_empty == 0);
			CHECK(stats.stats[SizeTypeLarge].num_pages_empty == 0);
#endif
		}

		alloc.free(pSmall);
		alloc.free(pLarge);
		alloc.flush();
		alloc.verify();

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[SizeTypeSmall].num_pages == 0);
			CHECK(stats.stats[SizeTypeLarge].num_pages == 0);
			CHECK(stats.stats[SizeTypeSmall].num_pages_free == 0);
			CHECK(stats.stats[SizeTypeLarge].num_pages_free == 0);
			CHECK(stats.stats[SizeTypeSmall].mem_used == 0);
			CHECK(stats.stats[SizeTypeLarge].mem_used == 0);
#if GAIA_DEBUG
			CHECK(stats.stats[SizeTypeSmall].mem_requested == 0);
			CHECK(stats.stats[SizeTypeLarge].mem_requested == 0);
			CHECK(stats.stats[SizeTypeSmall].num_pages_empty == 0);
			CHECK(stats.stats[SizeTypeLarge].num_pages_empty == 0);
#endif
		}

		alloc.flush(true);
		alloc.verify();
	}

	SUBCASE("allocator spills to another page and prefers recycled blocks") {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);
		alloc.verify();

		constexpr uint32_t NBlocks = mem::detail::SmallBlockPage::NBlocks;
		void* blocks[NBlocks + 1]{};

		GAIA_FOR(NBlocks) {
			blocks[i] = alloc.alloc(128);
		}
		blocks[NBlocks] = alloc.alloc(128);
		alloc.verify();

		alloc.free(blocks[0]);
		const void* recycledPtr = blocks[0];
		blocks[0] = nullptr;
		alloc.verify();

		void* recycled = alloc.alloc(128);
		CHECK(recycled == recycledPtr);

		for (void*& p: blocks) {
			if (p != nullptr) {
				alloc.free(p);
				p = nullptr;
			}
		}
		alloc.free(recycled);
		alloc.flush(true);
		alloc.verify();
	}

	SUBCASE("flush releases empty pages by default") {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);
		alloc.verify();

		void* p = alloc.alloc(64);
		alloc.free(p);
		alloc.flush();
		alloc.verify();

		const auto stats = alloc.stats();
		constexpr auto sizeType = mem::small_block_size_type(64);
		CHECK(stats.stats[sizeType].num_pages == 0);
		CHECK(stats.stats[sizeType].mem_used == 0);

		void* pNew = alloc.alloc(64);
		CHECK(pNew != nullptr);
		alloc.free(pNew);

		alloc.flush(true);
		alloc.verify();
	}

	SUBCASE("diag is callable") {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);
		void* p = alloc.alloc(32);

		const auto logLevelBackup = util::g_logLevelMask;
		util::g_logLevelMask = 0;
		alloc.diag();
		util::g_logLevelMask = logLevelBackup;

		alloc.free(p);
		alloc.flush(true);
		alloc.verify();
	}

	SUBCASE("GAIA_USE_SMALLBLOCK routes supported objects through the allocator") {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);
		alloc.verify();

		constexpr auto SmallSizeType = mem::small_block_size_type((uint32_t)sizeof(SmallBlockMacroSmallObject));

		uint32_t smallDtors = 0;
		{
			auto* pObj = new SmallBlockMacroSmallObject(77, smallDtors);
			CHECK(pObj != nullptr);
			CHECK(pObj->value == 77);

			const auto stats = alloc.stats();
			CHECK(stats.stats[SmallSizeType].num_pages == 1);
			CHECK(stats.stats[SmallSizeType].mem_used != 0);

			delete pObj;
		}
		CHECK(smallDtors == 1);

		alloc.flush(true);
		alloc.verify();

		const auto emptyStats = alloc.stats();
		GAIA_FOR(mem::SmallBlockSizeTypeCount) {
			CHECK(emptyStats.stats[i].num_pages == 0);
			CHECK(emptyStats.stats[i].mem_used == 0);
		}
	}

	SUBCASE("ComponentCacheItem uses SmallBlockAllocator") {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);
		alloc.verify();

		ecs::ComponentDesc desc{};
		desc.name = util::str_view("TestComponent", 13);
		desc.size = 4;
		desc.alig = 4;

		constexpr auto sizeType = mem::small_block_size_type((uint32_t)sizeof(ecs::ComponentCacheItem));

		auto* pItem = ecs::ComponentCacheItem::create(ecs::Entity(1, 0), desc);
		CHECK(pItem != nullptr);

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[sizeType].num_pages == 1);
			CHECK(stats.stats[sizeType].mem_used != 0);
		}

		ecs::ComponentCacheItem::destroy(pItem);
		alloc.flush(true);
		alloc.verify();

		{
			const auto stats = alloc.stats();
			CHECK(stats.stats[sizeType].num_pages == 0);
			CHECK(stats.stats[sizeType].mem_used == 0);
		}
	}

	SUBCASE("GAIA_USE_SMALLBLOCK leaves array allocation on the generic allocator") {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);
		alloc.verify();

		SmallBlockMacroSmallObject* pObjs = new SmallBlockMacroSmallObject[4];
		CHECK(pObjs != nullptr);

		pObjs[0].value = 10;
		pObjs[1].value = 11;
		pObjs[2].value = 12;
		pObjs[3].value = 13;

		CHECK(pObjs[0].value == 10);
		CHECK(pObjs[3].value == 13);

		const auto stats = alloc.stats();
		GAIA_FOR(mem::SmallBlockSizeTypeCount) {
			CHECK(stats.stats[i].num_pages == 0);
			CHECK(stats.stats[i].mem_used == 0);
		}

		delete[] pObjs;
		alloc.flush(true);
		alloc.verify();
	}

#if GAIA_DEBUG
	SUBCASE("freed blocks are poisoned") {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);
		alloc.verify();

		void* p = alloc.alloc(64);
		const auto* bytes = (const uint8_t*)p;
		alloc.free(p);
		alloc.verify();

		GAIA_FOR(32) {
			CHECK(bytes[i] == mem::detail::SmallBlockPage::FreedBlockPattern);
		}

		alloc.flush(true);
		alloc.verify();
	}
#endif
}

#if GAIA_ALLOC_ARENA_LOCK
// Independent Worlds may mutate concurrently when the arena spinlock is on.
// Each thread still owns its World.
TEST_CASE("Independent Worlds can mutate concurrently") {
	constexpr int kThreads = 3;
	constexpr int kEntities = 256;
	std::atomic<int> ok{0};

	cnt::darray<std::thread> threads;
	threads.reserve(kThreads);
	for (int t = 0; t < kThreads; ++t) {
		threads.emplace_back([&, t] {
			ecs::World world;
			world.add<Position>();
			world.add<PositionSparse>();

			float sum = 0.f;
			for (int i = 0; i < kEntities; ++i) {
				auto e = world.add();
				world.add<Position>(e, {(float)i, (float)t, 0.f});
				world.add<PositionSparse>(e, {(float)i, 1.f, 2.f});
				sum += world.get<Position>(e).x;
			}

			CHECK(world.query().all<Position>().count() == (uint32_t)kEntities);
			CHECK(world.query().all<PositionSparse>().count() == (uint32_t)kEntities);
			CHECK(sum > 0.f);
			ok.fetch_add(1, std::memory_order_relaxed);
		});
	}

	for (auto& th: threads)
		th.join();

	CHECK(ok.load() == kThreads);
}
#elif GAIA_ASSERT_ENABLED && GAIA_ECS_TEST_HOOKS
// Default (no arena lock): two threads inside an arena at once must be recorded.
// TEST_HOOKS counts instead of aborting so the suite can assert the detector.
TEST_CASE("Shared allocation arenas detect concurrent multi-thread use") {
	const auto before = mem::detail::ArenaLock::test_violations();

	std::atomic<bool> entered{false};
	std::atomic<bool> release{false};

	std::thread holder([&] {
		const mem::detail::ArenaLock guard;
		entered.store(true, std::memory_order_release);
		while (!release.load(std::memory_order_acquire)) {
		}
	});

	while (!entered.load(std::memory_order_acquire)) {
	}
	{
		const mem::detail::ArenaLock overlapping;
	}

	release.store(true, std::memory_order_release);
	holder.join();

	CHECK(mem::detail::ArenaLock::test_violations() > before);
}
#endif

TEST_CASE("SmallFunc") {
	SUBCASE("stores small callables inline") {
		uint32_t value = 0;
		util::SmallFunc func([&]() {
			value = 42;
		});

		CHECK((bool)func);
		func();
		CHECK(value == 42);
	}

	SUBCASE("default construction and reset clear the wrapper") {
		util::SmallFunc func;
		CHECK(!(bool)func);

		uint32_t value = 0;
		func = [&]() {
			++value;
		};
		CHECK((bool)func);
		func();
		CHECK(value == 1);

		func.reset();
		CHECK(!(bool)func);
	}

	SUBCASE("reset destroys an inline callable exactly once") {
		uint32_t calls = 0;
		uint32_t dtors = 0;

		util::SmallFunc func(SmallFuncTrackedSmallCallable(calls, dtors));
		CHECK(dtors == 0);

		func();
		CHECK(calls == 1);
		func.reset();
		CHECK(dtors == 1);
		CHECK(!(bool)func);
	}

	SUBCASE("supports move-only callables") {
		uint32_t value = 0;
		util::SmallFunc func([ptr = std::make_unique<uint32_t>(7), &value]() mutable {
			value = *ptr;
			ptr.reset();
		});

		util::SmallFunc moved = GAIA_MOV(func);
		CHECK(!(bool)func);
		CHECK((bool)moved);

		moved();
		CHECK(value == 7);
	}

	SUBCASE("move assignment transfers inline callable ownership") {
		uint32_t calls = 0;
		uint32_t dtors = 0;

		util::SmallFunc src(SmallFuncTrackedSmallCallable(calls, dtors));
		util::SmallFunc dst;
		dst = GAIA_MOV(src);

		CHECK(!(bool)src);
		CHECK((bool)dst);

		dst();
		CHECK(calls == 1);

		dst.reset();
		CHECK(dtors == 1);
	}

	SUBCASE("spills larger callables to the small block allocator") {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);
		alloc.verify();

		uint32_t value = 0;
		util::SmallFunc func(SmallFuncLargeCallable{&value, {}});
#if GAIA_FUNC_WRAPPER_SMALLBLOCK
		{
			const auto stats = alloc.stats();
			const auto sizeType = mem::small_block_size_type(sizeof(SmallFuncLargeCallable));
			CHECK(stats.stats[sizeType].num_pages == 1);
		}
#endif

		func();
		CHECK(value == 1);

		func.reset();
		alloc.verify();

		void* p = alloc.alloc(sizeof(SmallFuncLargeCallable));
		CHECK(p != nullptr);
		alloc.free(p);
		alloc.flush(true);
		alloc.verify();
	}

	SUBCASE("destroying a spilled callable releases allocator storage exactly once") {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);
		alloc.verify();

		uint32_t calls = 0;
		uint32_t dtors = 0;
		{
			util::SmallFunc func(SmallFuncTrackedLargeCallable(calls, dtors));
#if GAIA_FUNC_WRAPPER_SMALLBLOCK
			const auto stats = alloc.stats();
			const auto sizeType = mem::small_block_size_type(sizeof(SmallFuncTrackedLargeCallable));
			CHECK(stats.stats[sizeType].num_pages == 1);
#endif

			func();
			CHECK(calls == 1);
			CHECK(dtors == 0);
		}
		CHECK(dtors == 1);

		void* p = alloc.alloc(sizeof(SmallFuncTrackedLargeCallable));
		CHECK(p != nullptr);
		alloc.free(p);
		alloc.flush(true);
		alloc.verify();
	}

	SUBCASE("oversized callables fall back to the default heap") {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);
		alloc.verify();

		uint32_t calls = 0;
		uint32_t dtors = 0;
		{
			util::SmallFunc func(SmallFuncTrackedHeapCallable(calls, dtors));
			func();
			CHECK(calls == 1);
			CHECK(dtors == 0);

			util::SmallFunc moved = GAIA_MOV(func);
			CHECK(!(bool)func);
			CHECK((bool)moved);

			moved();
			CHECK(calls == 2);
		}
		CHECK(dtors == 1);

		alloc.flush(true);
		alloc.verify();
	}

	SUBCASE("reassignment destroys the previous callable across inline and spilled storage") {
		uint32_t smallCalls = 0;
		uint32_t smallDtors = 0;
		uint32_t largeCalls = 0;
		uint32_t largeDtors = 0;

		util::SmallFunc func(SmallFuncTrackedSmallCallable(smallCalls, smallDtors));
		func = SmallFuncTrackedLargeCallable(largeCalls, largeDtors);
		CHECK(smallDtors == 1);
		CHECK(largeDtors == 0);

		func();
		CHECK(largeCalls == 1);

		func = []() {};
		CHECK(largeDtors == 1);
	}

	SUBCASE("CreateSmallFunc supports move-only lambdas in C++17 mode") {
		uint32_t value = 0;
		auto lambda = [ptr = std::make_unique<uint32_t>(11), &value]() {
			value = *ptr;
		};

		util::SmallFunc func = CreateSmallFunc(GAIA_MOV(lambda));
		CHECK((bool)func);
		func();
		CHECK(value == 11);
	}
}

TEST_CASE("MoveFunc") {
	SUBCASE("stores argument-bearing callables inline") {
		util::MoveFunc<int32_t(int32_t, int32_t)> func([](int32_t lhs, int32_t rhs) {
			return lhs + rhs + 4;
		});

		CHECK((bool)func);
		CHECK(func(3, 5) == 12);
	}

	SUBCASE("reset destroys an inline callable exactly once") {
		int32_t calls = 0;
		int32_t dtors = 0;

		util::MoveFunc<int32_t(int32_t, int32_t)> func(MoveFuncTrackedSmallCallable(calls, dtors, 3));
		CHECK(dtors == 0);
		CHECK(func(4, 5) == 12);
		CHECK(calls == 1);

		func.reset();
		CHECK(dtors == 1);
		CHECK(!(bool)func);
	}

	SUBCASE("supports move-only argument-bearing callables") {
		int32_t value = 0;
		util::MoveFunc<int32_t(int32_t)> func([ptr = std::make_unique<int32_t>(7), &value](int32_t arg) mutable {
			value = *ptr + arg;
			ptr.reset();
			return value;
		});

		util::MoveFunc<int32_t(int32_t)> moved = GAIA_MOV(func);
		CHECK(!(bool)func);
		CHECK((bool)moved);
		CHECK(moved(5) == 12);
		CHECK(value == 12);
	}

	SUBCASE("move assignment transfers inline callable ownership") {
		int32_t calls = 0;
		int32_t dtors = 0;

		util::MoveFunc<int32_t(int32_t, int32_t)> src(MoveFuncTrackedSmallCallable(calls, dtors, 1));
		util::MoveFunc<int32_t(int32_t, int32_t)> dst;
		dst = GAIA_MOV(src);

		CHECK(!(bool)src);
		CHECK((bool)dst);
		CHECK(dst(6, 2) == 9);
		CHECK(calls == 1);

		dst.reset();
		CHECK(dtors == 1);
	}

	SUBCASE("spills larger argument-bearing callables to the small block allocator") {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);
		alloc.verify();

		int32_t calls = 0;
		int32_t dtors = 0;
		{
			util::MoveFunc<int32_t(int32_t, int32_t)> func(MoveFuncTrackedLargeCallable(calls, dtors, 2));
#if GAIA_FUNC_WRAPPER_SMALLBLOCK
			const auto stats = alloc.stats();
			const auto sizeType = mem::small_block_size_type(sizeof(MoveFuncTrackedLargeCallable));
			CHECK(stats.stats[sizeType].num_pages == 1);
#endif

			CHECK(func(8, 1) == 11);
			CHECK(calls == 1);
			CHECK(dtors == 0);
		}
		CHECK(dtors == 1);

		void* p = alloc.alloc(sizeof(MoveFuncTrackedLargeCallable));
		CHECK(p != nullptr);
		alloc.free(p);
		alloc.flush(true);
		alloc.verify();
	}

	SUBCASE("oversized argument-bearing callables fall back to the default heap") {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);
		alloc.verify();

		int32_t calls = 0;
		int32_t dtors = 0;
		{
			util::MoveFunc<int32_t(int32_t, int32_t)> func(MoveFuncTrackedHeapCallable(calls, dtors, 5));
			CHECK(func(1, 2) == 8);
			CHECK(calls == 1);
			CHECK(dtors == 0);

			util::MoveFunc<int32_t(int32_t, int32_t)> moved = GAIA_MOV(func);
			CHECK(!(bool)func);
			CHECK((bool)moved);
			CHECK(moved(2, 3) == 10);
			CHECK(calls == 2);
		}
		CHECK(dtors == 1);

		alloc.flush(true);
		alloc.verify();
	}

	SUBCASE("reassignment destroys the previous callable across inline and spilled storage") {
		int32_t smallCalls = 0;
		int32_t smallDtors = 0;
		int32_t largeCalls = 0;
		int32_t largeDtors = 0;

		util::MoveFunc<int32_t(int32_t, int32_t)> func(MoveFuncTrackedSmallCallable(smallCalls, smallDtors, 1));
		func = MoveFuncTrackedLargeCallable(largeCalls, largeDtors, 2);
		CHECK(smallDtors == 1);
		CHECK(largeDtors == 0);

		CHECK(func(2, 6) == 10);
		CHECK(largeCalls == 1);

		func = [](int32_t lhs, int32_t rhs) {
			return lhs - rhs;
		};
		CHECK(largeDtors == 1);
		CHECK(func(9, 4) == 5);
	}
}

TEST_CASE("JobArgsFunc") {
	SUBCASE("stores range callbacks inline") {
		mt::JobArgsFunc func([](const mt::JobArgs& args) {
			CHECK(args.idxStart == 2);
			CHECK(args.idxEnd == 7);
		});

		CHECK((bool)func);
		func({2, 7});
	}

	SUBCASE("supports move-only range callbacks") {
		uint32_t value = 0;
		mt::JobArgsFunc func([ptr = std::make_unique<uint32_t>(9), &value](const mt::JobArgs& args) mutable {
			value = *ptr + args.idxStart + args.idxEnd;
			ptr.reset();
		});

		mt::JobArgsFunc moved = GAIA_MOV(func);
		CHECK(!(bool)func);
		CHECK((bool)moved);

		moved({2, 3});
		CHECK(value == 14);
	}

	SUBCASE("destroys spilled range callbacks exactly once") {
		struct TrackedLargeJobArgsCallable {
			uint32_t* pCalls = nullptr;
			uint32_t* pDtors = nullptr;
			uint8_t payload[128]{};

			TrackedLargeJobArgsCallable(uint32_t& calls, uint32_t& dtors): pCalls(&calls), pDtors(&dtors) {}
			TrackedLargeJobArgsCallable(TrackedLargeJobArgsCallable&& other) noexcept:
					pCalls(other.pCalls), pDtors(other.pDtors) {
				other.pCalls = nullptr;
				other.pDtors = nullptr;
			}
			TrackedLargeJobArgsCallable(const TrackedLargeJobArgsCallable&) = delete;
			TrackedLargeJobArgsCallable& operator=(TrackedLargeJobArgsCallable&&) = delete;
			TrackedLargeJobArgsCallable& operator=(const TrackedLargeJobArgsCallable&) = delete;
			~TrackedLargeJobArgsCallable() {
				if (pDtors != nullptr)
					++*pDtors;
			}

			void operator()(const mt::JobArgs& args) const {
				++*pCalls;
				CHECK(args.idxStart == 4);
				CHECK(args.idxEnd == 8);
			}
		};

		uint32_t calls = 0;
		uint32_t dtors = 0;
		{
			mt::JobArgsFunc func(TrackedLargeJobArgsCallable(calls, dtors));
			func({4, 8});
			CHECK(calls == 1);
			CHECK(dtors == 0);
		}
		CHECK(dtors == 1);
	}
}

TEST_CASE("CommandBuffer") {
	SUBCASE("Entity creation") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const uint32_t N = 100;
		GAIA_FOR(N) {
			[[maybe_unused]] auto tmp = cb.add();
		}

		cb.commit();

		CHECK(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1 + N);
	}

	SUBCASE("Entity creation from a query") {
		TestWorld twld;
		auto mainEntity = wld.add();
		wld.add<Position>(mainEntity, {1, 2, 3});

		uint32_t cnt = 0;
		auto q = wld.query().all<Position>();
		q.each([&](ecs::Iter& it) {
			auto& cb = it.cmd_buffer_st();
			auto e = cb.add();
			cb.add<Position>(e, {4, 5, 6});
			++cnt;
		});
		CHECK(cnt == 1);

		cnt = 0;
		q.each([&](ecs::Iter& it) {
			auto ev = it.view<ecs::Entity>();
			auto pv = it.view<Position>();
			GAIA_EACH(it) {
				const auto& p = pv[i];
				if (ev[i] == mainEntity) {
					CHECK(p.x == 1.f);
					CHECK(p.y == 2.f);
					CHECK(p.z == 3.f);
				} else {
					CHECK(p.x == 4.f);
					CHECK(p.y == 5.f);
					CHECK(p.z == 6.f);
				}
				++cnt;
			}
		});
		CHECK(cnt == 2);
	}

	SUBCASE("Adding a component to matched entities from a retained query") {
		TestWorld twld;

		auto e1 = wld.add();
		wld.add<Position>(e1, {1, 2, 3});
		auto e2 = wld.add();
		wld.add<Position>(e2, {4, 5, 6});
		(void)wld.add<Rotation>();

		auto q = wld.query().all<Position>();
		uint32_t queued = 0;
		q.each([&](ecs::Iter& it) {
			auto ev = it.view<ecs::Entity>();
			auto& cb = it.cmd_buffer_st();
			GAIA_EACH(it) {
				cb.add<Rotation>(ev[i], {1, 0, 0, 1});
				++queued;
			}
		});

		CHECK(queued == 2);
		CHECK(wld.has<Rotation>(e1));
		CHECK(wld.has<Rotation>(e2));
		CHECK(q.count() == 2);
		CHECK(wld.query().all<Position>().all<Rotation>().count() == 2);
	}

	SUBCASE("Entity creation from another entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto mainEntity = wld.add();

		const uint32_t N = 100;
		GAIA_FOR(N) {
			[[maybe_unused]] auto tmp = cb.copy(mainEntity);
		}

		cb.commit();

		CHECK(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1 + 1 + N); // core + mainEntity + N others
	}

	SUBCASE("Entity creation from a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto mainEntity = cb.add();

		const uint32_t N = 100;
		GAIA_FOR(N) {
			[[maybe_unused]] auto tmp = cb.copy(mainEntity);
		}

		cb.commit();

		CHECK(wld.size() == ecs::GAIA_ID(LastCoreComponent).id() + 1 + 1 + N); // core + mainEntity + N others
	}

	SUBCASE("Entity creation from another entity with a component") {
		{
			TestWorld twld;
			ecs::CommandBufferST cb(wld);
			auto mainEntity = wld.add();

			wld.add<Position>(mainEntity, {1, 2, 3});

			auto q = wld.query().all<Position>();
			CHECK(q.count() == 1);
			uint32_t i = 0;
			q.each([&](const Position& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 1);

			(void)wld.copy(mainEntity);
			CHECK(q.count() == 2);
			i = 0;
			q.each([&](const Position& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 2);
		}

		{
			TestWorld twld;
			ecs::CommandBufferST cb(wld);
			auto mainEntity = wld.add();

			wld.add<Position>(mainEntity, {1, 2, 3});

			[[maybe_unused]] auto tmp = cb.copy(mainEntity);
			cb.commit();

			auto q = wld.query().all<Position>();
			CHECK(q.count() == 2);
			uint32_t i = 0;
			q.each([&](const Position& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 2);
		}
	}

	SUBCASE("Entity creation from another entity with a SoA component") {
		{
			TestWorld twld;
			ecs::CommandBufferST cb(wld);
			auto mainEntity = wld.add();

			wld.add<PositionSoA>(mainEntity, {1, 2, 3});

			auto q = wld.query().all<PositionSoA>();
			CHECK(q.count() == 1);
			uint32_t i = 0;
			q.each([&](const PositionSoA& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 1);

			(void)wld.copy(mainEntity);
			CHECK(q.count() == 2);
			i = 0;
			q.each([&](const PositionSoA& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 2);
		}

		{
			TestWorld twld;
			ecs::CommandBufferST cb(wld);
			auto mainEntity = wld.add();

			wld.add<PositionSoA>(mainEntity, {1, 2, 3});

			(void)cb.copy(mainEntity);
			cb.commit();

			auto q = wld.query().all<PositionSoA>();
			CHECK(q.count() == 2);
			uint32_t i = 0;
			q.each([&](const PositionSoA& p) {
				CHECK(p.x == 1.f);
				CHECK(p.y == 2.f);
				CHECK(p.z == 3.f);
				++i;
			});
			CHECK(i == 2);
		}
	}

	SUBCASE("Delayed component addition to an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();
		cb.add<Position>(e);
		CHECK_FALSE(wld.has<Position>(e));
		cb.commit();
		CHECK(wld.has<Position>(e));
	}

	SUBCASE("Delayed component addition (via entity) to an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto p = wld.add<Position>().entity;

		auto e = wld.add();
		cb.add(e, p);
		CHECK_FALSE(wld.has(e, p));
		cb.commit();
		CHECK(wld.has(e, p));
	}

	SUBCASE("Delayed relation pair addition and removal") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto rel = wld.add();
		auto tgt = wld.add();
		auto e = wld.add();
		ecs::Pair pair(rel, tgt);

		cb.add(e, pair);
		CHECK_FALSE(wld.has(e, pair));
		cb.commit();
		CHECK(wld.has(e, pair));

		cb.del(e, pair);
		CHECK(wld.has(e, pair));
		cb.commit();
		CHECK_FALSE(wld.has(e, pair));

		cb.add(e, static_cast<ecs::Entity>(pair));
		CHECK_FALSE(wld.has(e, pair));
		cb.commit();
		CHECK(wld.has(e, pair));

		cb.del(e, static_cast<ecs::Entity>(pair));
		CHECK(wld.has(e, pair));
		cb.commit();
		CHECK_FALSE(wld.has(e, pair));
	}

	SUBCASE("Delayed relationship payload add and set") {
		TestWorld twld;
		(void)wld.add<CmdBufRelPair::rel>();
		(void)wld.add<CmdBufRelPair::tgt>();
		const auto relation = wld.add<CmdBufRelPair::rel>().entity;
		const auto target = wld.add<CmdBufRelPair::tgt>().entity;
		const auto pair = ecs::Pair(relation, target);
		const auto source = wld.add();
		const auto other = wld.add();

		wld.add(source, pair, CmdBufRelPayload{1.0f, 2.0f});
		CHECK(wld.get<CmdBufRelPair>(source).x == doctest::Approx(1.0f));
		CHECK(wld.get<CmdBufRelPair>(source).y == doctest::Approx(2.0f));
		CHECK_FALSE(wld.has<CmdBufRelPayload>(relation));

		ecs::CommandBufferST cb(wld);
		cb.add(other, pair, CmdBufRelPayload{3.0f, 4.0f});
		CHECK_FALSE(wld.has(other, pair));
		cb.commit();
		CHECK(wld.get<CmdBufRelPair>(other).x == doctest::Approx(3.0f));
		CHECK(wld.get<CmdBufRelPair>(other).y == doctest::Approx(4.0f));
		CHECK_FALSE(wld.has<CmdBufRelPayload>(relation));

		const ecs::Entity pairEntity = pair;
		const auto packed = wld.add();
		cb.add(packed, pairEntity, CmdBufRelPayload{5.0f, 6.0f});
		cb.commit();
		CHECK(wld.get<CmdBufRelPair>(packed).x == doctest::Approx(5.0f));

		cb.set(other, pair, CmdBufRelPayload{7.0f, 8.0f});
		cb.commit();
		CHECK(wld.get<CmdBufRelPair>(other).x == doctest::Approx(7.0f));
		CHECK(wld.get<CmdBufRelPair>(other).y == doctest::Approx(8.0f));

		const auto reduced = wld.add();
		cb.add(reduced, pair, CmdBufRelPayload{9.0f, 10.0f});
		cb.set(reduced, pair, CmdBufRelPayload{11.0f, 12.0f});
		cb.commit();
		CHECK(wld.get<CmdBufRelPair>(reduced).x == doctest::Approx(11.0f));
		CHECK(wld.get<CmdBufRelPair>(reduced).y == doctest::Approx(12.0f));
	}

	SUBCASE("Delayed pair addition with temporary relation and target") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto source = wld.add();
		const auto firstDeferredId = wld.size();
		const auto relationTmp = cb.add();
		const auto targetTmp = cb.add();
		cb.add(source, ecs::Pair(relationTmp, targetTmp));
		cb.commit();

		const auto relation = wld.get(firstDeferredId);
		const auto target = wld.get(firstDeferredId + 1);
		CHECK(wld.has(source, ecs::Pair(relation, target)));
	}

	SUBCASE("Delayed exact pair addition followed by wildcard removal") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto relation = wld.add();
		const auto target = wld.add();
		const auto source = wld.add();
		wld.add(relation, ecs::Exclusive);
		wld.add(relation, ecs::DontFragment);

		cb.add(source, ecs::Pair(relation, target));
		cb.del(source, ecs::Pair(relation, ecs::All));
		cb.commit();

		CHECK_FALSE(wld.has(source, ecs::Pair(relation, ecs::All)));

		cb.del(source, ecs::Pair(relation, ecs::All));
		cb.add(source, ecs::Pair(relation, target));
		cb.commit();

		CHECK(wld.has(source, ecs::Pair(relation, target)));
	}

	SUBCASE("Delayed exclusive pair additions preserve target order") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto relation = wld.add();
		const auto targetA = wld.add();
		const auto targetB = wld.add();
		const auto source = wld.add();
		wld.add(relation, ecs::Exclusive);
		wld.add(relation, ecs::DontFragment);

		cb.add(source, ecs::Pair(relation, targetB));
		cb.add(source, ecs::Pair(relation, targetA));
		cb.commit();

		CHECK(wld.target(source, relation) == targetA);
	}

	SUBCASE("Delayed entity addition to an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add(); // core + 1
		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s); // no new added entity
		cb.add(e, tmp);
		cb.commit();
		CHECK(wld.size() == s + 1); // new entity added

		auto e2 = wld.get(s); // core + e + new entity
		CHECK(wld.has(e, e2));
	}

	SUBCASE("Delayed component addition to a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();

		auto tmp = cb.add(); // no new entity created yet
		CHECK(wld.size() == s);
		cb.add<Position>(tmp); // component entity created
		CHECK(wld.size() == s);
		cb.commit();
		CHECK(wld.size() == s + 1);

		auto e = wld.get(s); // + new entity
		CHECK(wld.has<Position>(e));
	}

	SUBCASE("Delayed component addition (via entity) to a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto p = wld.add<Position>().entity;
		CHECK(wld.size() == s);

		auto tmp = cb.add(); // no new entity created yet
		CHECK(wld.size() == s);
		cb.add(tmp, p);
		CHECK(wld.size() == s);
		cb.commit();
		CHECK(wld.size() == s + 1);

		auto e = wld.get(s); // + new entity
		CHECK(wld.has(e, p));
	}

	SUBCASE("Delayed entity addition to a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto s = wld.size();
		auto tmpA = cb.add();
		auto tmpB = cb.add(); // core + 0 (no new entity created yet)
		CHECK(wld.size() == s);
		cb.add(tmpA, tmpB);
		CHECK(wld.size() == s);
		cb.commit();
		CHECK(wld.size() == s + 2);

		auto e1 = wld.get(s);
		auto e2 = wld.get(s + 1);
		CHECK(wld.has(e1, e2));
	}

	SUBCASE("Delayed component setting of an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();

		cb.add<Position>(e);
		cb.set<Position>(e, {1, 2, 3});
		CHECK_FALSE(wld.has<Position>(e));

		cb.commit();
		CHECK(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1);
		CHECK(p.y == 2);
		CHECK(p.z == 3);
	}

	SUBCASE("Interleaved add and set initialize before OnAdd") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto entity = wld.add();
		uint32_t addHits = 0;
		Position observed{};
		(void)wld.observer()
				.event(ecs::ObserverEvent::OnAdd)
				.all<Position>()
				.on_each([&](const Position& value) {
					++addHits;
					observed = value;
				})
				.entity();

		cb.add<Position>(entity);
		cb.add<Acceleration>(entity);
		cb.set<Position>(entity, {1, 2, 3});
		cb.commit();

		CHECK(addHits == 1);
		CHECK(observed.x == 1);
		CHECK(observed.y == 2);
		CHECK(observed.z == 3);
	}

	SUBCASE("Delayed 2 components setting of an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();

		cb.add<Position>(e);
		cb.set<Position>(e, {1, 2, 3});
		cb.add<Acceleration>(e);
		cb.set<Acceleration>(e, {4, 5, 6});
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));

		cb.commit();
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);

		auto a = wld.get<Acceleration>(e);
		CHECK(a.x == 4.f);
		CHECK(a.y == 5.f);
		CHECK(a.z == 6.f);
	}

	SUBCASE("Delayed component setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		CHECK(wld.size() == s);
		cb.set<Position>(tmp, {1, 2, 3});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);
	}

	SUBCASE("Delayed component add+delete of a temporary entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		CHECK(wld.size() == s);
		cb.del<Position>(tmp);
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK_FALSE(wld.has<Position>(e));
	}

	SUBCASE("Delayed component add+set+set") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		cb.set<Position>(tmp, {1, 2, 3});
		CHECK(wld.size() == s);
		cb.set<Position>(tmp, {4, 5, 6});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 4.f);
		CHECK(p.y == 5.f);
		CHECK(p.z == 6.f);
	}

	SUBCASE("Delayed component add+set+set+del") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		cb.set<Position>(tmp, {1, 2, 3});
		CHECK(wld.size() == s);
		cb.set<Position>(tmp, {4, 5, 6});
		cb.del(tmp);
		cb.commit();
		CHECK(wld.size() == s);
	}

	SUBCASE("Delayed 2 components setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();
		(void)wld.add<Acceleration>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp);
		cb.add<Acceleration>(tmp);
		cb.set<Position>(tmp, {1, 2, 3});
		cb.set<Acceleration>(tmp, {4, 5, 6});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);

		auto a = wld.get<Acceleration>(e);
		CHECK(a.x == 4.f);
		CHECK(a.y == 5.f);
		CHECK(a.z == 6.f);
	}

	SUBCASE("Delayed component add with setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp, {1, 2, 3});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);
	}

	SUBCASE("Delayed 2 components add with setting of a to-be-created entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		(void)wld.add<Position>();
		(void)wld.add<Acceleration>();

		const auto s = wld.size();
		auto tmp = cb.add();
		CHECK(wld.size() == s);

		cb.add<Position>(tmp, {1, 2, 3});
		cb.add<Acceleration>(tmp, {4, 5, 6});
		cb.commit();
		CHECK(wld.size() == s + 1); // + new entity

		auto e = wld.get(s);
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));

		auto p = wld.get<Position>(e);
		CHECK(p.x == 1.f);
		CHECK(p.y == 2.f);
		CHECK(p.z == 3.f);

		auto a = wld.get<Acceleration>(e);
		CHECK(a.x == 4.f);
		CHECK(a.y == 5.f);
		CHECK(a.z == 6.f);
	}

	SUBCASE("Delayed component removal from an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});

		cb.del<Position>(e);
		CHECK(wld.has<Position>(e));
		{
			auto p = wld.get<Position>(e);
			CHECK(p.x == 1.f);
			CHECK(p.y == 2.f);
			CHECK(p.z == 3.f);
		}

		cb.commit();
		CHECK_FALSE(wld.has<Position>(e));
	}

	SUBCASE("Delayed 2 component removal from an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});
		wld.add<Acceleration>(e, {4, 5, 6});

		cb.del<Position>(e);
		cb.del<Acceleration>(e);
		CHECK(wld.has<Position>(e));
		CHECK(wld.has<Acceleration>(e));
		{
			auto p = wld.get<Position>(e);
			CHECK(p.x == 1.f);
			CHECK(p.y == 2.f);
			CHECK(p.z == 3.f);

			auto a = wld.get<Acceleration>(e);
			CHECK(a.x == 4.f);
			CHECK(a.y == 5.f);
			CHECK(a.z == 6.f);
		}

		cb.commit();
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));
	}

	SUBCASE("Delayed non-trivial component setting of an existing entity") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();

		cb.add<StringComponent>(e);
		cb.set<StringComponent>(e, {StringComponentDefaultValue});
		cb.add<StringComponent2>(e);
		CHECK_FALSE(wld.has<StringComponent>(e));
		CHECK_FALSE(wld.has<StringComponent2>(e));

		cb.commit();
		CHECK(wld.has<StringComponent>(e));
		CHECK(wld.has<StringComponent2>(e));

		auto s1 = wld.get<StringComponent>(e);
		CHECK(s1.value == StringComponentDefaultValue);
		auto s2 = wld.get<StringComponent2>(e);
		CHECK(s2.value == StringComponent2DefaultValue);
	}

	SUBCASE("Delayed entity deletion") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		auto e = wld.add();

		cb.del(e);
		CHECK(wld.has(e));

		cb.commit();
		CHECK_FALSE(wld.has(e));
	}
}

TEST_CASE_TEMPLATE(
		"CommandBuffer - recorded component order", CmdBuffer, ecs::CommandBufferST, ecs::CommandBufferMT) {
	TestWorld twld;
	(void)wld.add<PositionNonTrivial>();
	CmdBuffer cb(wld);
	const auto marker = wld.add();

	for (uint32_t scenario = 0; scenario < 32; ++scenario) {
		const bool present = (scenario & 1) != 0;
		const bool deleteFirst = (scenario & 2) != 0;
		const bool withData = (scenario & 4) != 0;
		const bool withSet = (scenario & 8) != 0;
		const bool interleaved = (scenario & 16) != 0;
		CAPTURE(scenario);
		const auto e = wld.add();
		if (present)
			wld.add<PositionNonTrivial>(e, {1, 1, 1});

		if (deleteFirst)
			cb.template del<PositionNonTrivial>(e);
		if (withData)
			cb.template add<PositionNonTrivial>(e, {2, 2, 2});
		else
			cb.template add<PositionNonTrivial>(e);
		if (interleaved)
			cb.template add<PositionNonTrivial>(marker, {4, 4, 4});
		if (withSet)
			cb.template set<PositionNonTrivial>(e, {3, 3, 3});
		if (!deleteFirst)
			cb.template del<PositionNonTrivial>(e);
		cb.commit();

		CHECK(wld.has<PositionNonTrivial>(e) == deleteFirst);
		if (deleteFirst) {
			const auto p = wld.get<PositionNonTrivial>(e);
			CHECK(p.x == (withSet ? 3.0f : (withData ? 2.0f : 1.0f)));
			CHECK(p.y == (withSet ? 3.0f : 2.0f));
			CHECK(p.z == (withSet ? 3.0f : (withData ? 2.0f : 3.0f)));
		}
		if (interleaved)
			CHECK(wld.get<PositionNonTrivial>(marker).x == 4);
	}

	SUBCASE("Pair requests preserve delete and add order") {
		const auto relation = wld.add<CmdBufRelPayload>().entity;
		const auto target = wld.add<CmdBufRelTarget>().entity;
		const ecs::Pair pair(relation, target);
		for (uint32_t scenario = 0; scenario < 4; ++scenario) {
			const bool present = (scenario & 1) != 0;
			const bool deleteFirst = (scenario & 2) != 0;
			CAPTURE(scenario);
			const auto e = wld.add();
			if (present)
				wld.add(e, pair, CmdBufRelPayload{1, 1});
			if (deleteFirst)
				cb.del(e, pair);
			cb.add(e, pair, CmdBufRelPayload{2, 2});
			if (!deleteFirst)
				cb.del(e, pair);
			cb.commit();
			CHECK(wld.has(e, pair) == deleteFirst);
			if (deleteFirst)
				CHECK(wld.get<CmdBufRelPair>(e).x == 2);
		}

		const auto e = wld.add();
		wld.add(e, pair, CmdBufRelPayload{1, 1});
		cb.del(e, pair);
		cb.add(e, ecs::Pair(relation, marker));
		cb.add(e, pair, CmdBufRelPayload{2, 2});
		cb.commit();
		CHECK(wld.get<CmdBufRelPair>(e).x == 2);
	}

	SUBCASE("Separate commits preserve delete then add") {
		const auto e = wld.add();
		wld.add<PositionNonTrivial>(e, {1, 1, 1});
		cb.template del<PositionNonTrivial>(e);
		cb.commit();
		CHECK_FALSE(wld.has<PositionNonTrivial>(e));
		cb.template add<PositionNonTrivial>(e, {2, 2, 2});
		cb.commit();
		CHECK(wld.get<PositionNonTrivial>(e).x == 2);
	}

	SUBCASE("Unobserved payload replacement keeps the entity in its chunk row") {
		ecs::World reducedWorld;
		(void)reducedWorld.add<PositionNonTrivial>();
		CmdBuffer reducedBuffer(reducedWorld);
		const auto first = reducedWorld.add();
		const auto second = reducedWorld.add();
		reducedWorld.add<PositionNonTrivial>(first, {1, 1, 1});
		reducedWorld.add<PositionNonTrivial>(second, {9, 9, 9});
		const auto* before = &reducedWorld.get<PositionNonTrivial>(first);
		reducedBuffer.template del<PositionNonTrivial>(first);
		reducedBuffer.template add<PositionNonTrivial>(first, {2, 2, 2});
		reducedBuffer.commit();
		CHECK(&reducedWorld.get<PositionNonTrivial>(first) == before);
		CHECK(reducedWorld.get<PositionNonTrivial>(first).x == 2);
		CHECK(reducedWorld.get<PositionNonTrivial>(second).x == 9);
		reducedBuffer.template del<PositionNonTrivial>(first);
		reducedBuffer.template add<PositionNonTrivial>(first);
		reducedBuffer.commit();
		CHECK(&reducedWorld.get<PositionNonTrivial>(first) == before);
		CHECK(reducedWorld.get<PositionNonTrivial>(first).x == 1);
		CHECK(reducedWorld.get<PositionNonTrivial>(first).y == 2);
		CHECK(reducedWorld.get<PositionNonTrivial>(first).z == 3);
	}

	SUBCASE("Canceled membership still applies required component side effects") {
		const auto component = wld.add<PositionNonTrivial>().entity;
		wld.add(component, ecs::Pair(ecs::Requires, marker));
		const auto e = wld.add();
		cb.template add<PositionNonTrivial>(e, {2, 2, 2});
		cb.template del<PositionNonTrivial>(e);
		cb.commit();
		CHECK_FALSE(wld.has<PositionNonTrivial>(e));
		CHECK(wld.has(e, marker));
	}

	SUBCASE("Bare re-add constructs empty tags at a valid address") {
		(void)wld.add<CmdBufCtorTag>();
		const auto e = wld.add();
		wld.add<CmdBufCtorTag>(e);
		cb.template del<CmdBufCtorTag>(e);
		cb.template add<CmdBufCtorTag>(e);
		CmdBufCtorTag::instance = nullptr;
		cb.commit();
		CHECK(CmdBufCtorTag::instance != nullptr);
		CHECK(wld.has<CmdBufCtorTag>(e));
	}

	SUBCASE("In-place replacement initializes fields omitted by a custom serializer") {
		(void)wld.add<CmdBufPartialPayload>();
		const auto e = wld.add();
		wld.add<CmdBufPartialPayload>(e, {1, 99});
		cb.template del<CmdBufPartialPayload>(e);
		cb.template add<CmdBufPartialPayload>(e, {2, 42});
		cb.commit();
		const auto& value = wld.get<CmdBufPartialPayload>(e);
		CHECK(value.serialized == 2);
		CHECK(value.omitted == 7);
	}

#if GAIA_ENABLE_ADD_DEL_HOOKS
	SUBCASE("Removal hooks retain the preceding payload write") {
		static uint32_t delHits;
		static float removedValue;
		delHits = 0;
		removedValue = 0;
		const auto& item = wld.add<PositionNonTrivial>();
		ecs::ComponentCache::hooks(item).func_del =
				[](const ecs::World& world, const ecs::ComponentCacheItem&, ecs::Entity e) {
					++delHits;
					removedValue = world.get<PositionNonTrivial>(e).x;
				};
		const auto e = wld.add();
		wld.add<PositionNonTrivial>(e, {1, 1, 1});
		cb.template add<PositionNonTrivial>(e, {2, 2, 2});
		cb.template del<PositionNonTrivial>(e);
		cb.commit();
		CHECK(delHits == 1);
		CHECK(removedValue == 2);
		ecs::ComponentCache::hooks(item).func_del = nullptr;
	}
#endif

	SUBCASE("A queued re-add replaces the value at commit time") {
		const auto e = wld.add();
		wld.add<PositionNonTrivial>(e, {1, 1, 1});
		cb.template del<PositionNonTrivial>(e);
		cb.template add<PositionNonTrivial>(e, {2, 2, 2});
		wld.set<PositionNonTrivial>(e) = {3, 3, 3};
		cb.commit();
		CHECK(wld.get<PositionNonTrivial>(e).x == 2);
	}
	SUBCASE("Removal observes the preceding payload write") {
		const auto e = wld.add();
		wld.add<PositionNonTrivial>(e, {1, 1, 1});
		uint32_t delHits = 0;
		float removedValue = 0;
		(void)wld.observer()
				.event(ecs::ObserverEvent::OnDel)
				.all<PositionNonTrivial>()
				.on_each([&](const PositionNonTrivial& value) {
					++delHits;
					removedValue = value.x;
				})
				.entity();
		cb.template add<PositionNonTrivial>(e, {2, 2, 2});
		cb.template del<PositionNonTrivial>(e);
		cb.commit();
		CHECK(delHits == 1);
		CHECK(removedValue == 2);
	}

	SUBCASE("A rejected removal preserves the preceding write") {
		const auto component = wld.add<PositionNonTrivial>().entity;
		wld.add(component, ecs::Requires);
		const auto e = wld.add();
		wld.add<PositionNonTrivial>(e, {1, 1, 1});
		cb.template add<PositionNonTrivial>(e, {2, 2, 2});
		cb.template del<PositionNonTrivial>(e);
		cb.commit();
		CHECK(wld.has<PositionNonTrivial>(e));
		CHECK(wld.get<PositionNonTrivial>(e).x == 2);
	}

	SUBCASE("Exclusive pair add then delete does not restore the old target") {
		const auto relation = wld.add();
		wld.add(relation, ecs::Exclusive);
		wld.add(relation, ecs::DontFragment);
		const auto a = wld.add();
		const auto b = wld.add();
		const auto e = wld.add();
		wld.add(e, ecs::Pair(relation, a));
		cb.add(e, ecs::Pair(relation, b));
		cb.del(e, ecs::Pair(relation, b));
		cb.commit();
		CHECK_FALSE(wld.has(e, ecs::Pair(relation, ecs::All)));
	}

	SUBCASE("Sparse storage respects the same order with and without fragmentation") {
		for (uint32_t mode = 0; mode < 2; ++mode) {
			ecs::World sparseWorld;
			const auto component = sparseWorld.add<PositionSparse>().entity;
			if (mode != 0)
				sparseWorld.add(component, ecs::DontFragment);
			CmdBuffer sparseBuffer(sparseWorld);
			for (uint32_t scenario = 0; scenario < 4; ++scenario) {
				const auto e = sparseWorld.add();
				const bool deleteFirst = (scenario & 2) != 0;
				if ((scenario & 1) != 0)
					sparseWorld.add<PositionSparse>(e, {1, 1, 1});
				if (deleteFirst)
					sparseBuffer.template del<PositionSparse>(e);
				sparseBuffer.template add<PositionSparse>(e, {2, 2, 2});
				if (!deleteFirst)
					sparseBuffer.template del<PositionSparse>(e);
				sparseBuffer.commit();
				CHECK(sparseWorld.has<PositionSparse>(e) == deleteFirst);
				if (deleteFirst)
					CHECK(sparseWorld.get<PositionSparse>(e).x == 2);
			}
		}
	}

	SUBCASE("Longer sequences match direct World membership and values") {
		for (uint32_t initial = 0; initial < 2; ++initial) {
			for (uint32_t sequence = 0; sequence < 256; ++sequence) {
				CAPTURE(initial);
				CAPTURE(sequence);
				const auto direct = wld.add();
				const auto deferred = wld.add();
				if (initial != 0) {
					wld.add<PositionNonTrivial>(direct, {9, 9, 9});
					wld.add<PositionNonTrivial>(deferred, {9, 9, 9});
				}
				for (uint32_t step = 0; step < 4; ++step) {
					const uint32_t op = (sequence >> (step * 2)) & 3;
					const float value = (float)(step + 2);
					if (op == 0) {
						wld.add<PositionNonTrivial>(direct);
						cb.template add<PositionNonTrivial>(deferred);
					} else if (op == 1) {
						wld.add<PositionNonTrivial>(direct, {value, value, value});
						cb.template add<PositionNonTrivial>(deferred, {value, value, value});
					} else if (op == 2) {
						if (wld.has<PositionNonTrivial>(direct)) {
							wld.del<PositionNonTrivial>(direct);
							cb.template del<PositionNonTrivial>(deferred);
						}
					} else if (wld.has<PositionNonTrivial>(direct)) {
						wld.set<PositionNonTrivial>(direct) = {value, value, value};
						cb.template set<PositionNonTrivial>(deferred, {value, value, value});
					}
				}
				cb.commit();
				const bool present = wld.has<PositionNonTrivial>(direct);
				CHECK(wld.has<PositionNonTrivial>(deferred) == present);
				if (present) {
					const auto expected = wld.get<PositionNonTrivial>(direct);
					const auto actual = wld.get<PositionNonTrivial>(deferred);
					CHECK(actual.x == expected.x);
					CHECK(actual.y == expected.y);
					CHECK(actual.z == expected.z);
				}
			}
		}
	}

}

TEST_CASE("Sparse storage - chunk transitions construct payloads only in sparse store") {
	for (uint32_t mode = 0; mode < 2; ++mode) {
		CAPTURE(mode);
		TestWorld twld;
		(void)wld.add<Position>();
		if (mode != 0)
			(void)wld.add<SparseCtorProbe>();
		(void)wld.add<Acceleration>();
		(void)wld.add<Rotation>();
		(void)wld.add<SparseCtorProbe>();
		const auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});
		wld.add<Acceleration>(e, {4, 5, 6});
		wld.add<Rotation>(e, {7, 8, 9, 10});
		SparseCtorProbe::ctorCalls = 0;
		wld.add<SparseCtorProbe>(e);
		CHECK(SparseCtorProbe::ctorCalls == 1);
		CHECK(wld.get<SparseCtorProbe>(e).value == 17);
		CHECK(wld.get<Position>(e).x == 1);
		CHECK(wld.get<Acceleration>(e).y == 5);
		CHECK(wld.get<Rotation>(e).w == 10);

		wld.sset<SparseCtorProbe>(e).value = 91;
		wld.name(e, "sparse_ctor_source");
		SparseCtorProbe::ctorCalls = 0;
		const auto copy = wld.copy(e);
		CHECK(SparseCtorProbe::ctorCalls == 1);
		CHECK(wld.get<SparseCtorProbe>(copy).value == 91);
		CHECK(wld.get<Position>(copy).x == 1);
		CHECK(wld.get<Acceleration>(copy).y == 5);
		CHECK(wld.get<Rotation>(copy).w == 10);
		CHECK(wld.name(copy).empty());

		wld.add(e, ecs::Prefab);
		SparseCtorProbe::ctorCalls = 0;
		uint32_t count = 0;
		wld.instantiate_n(e, 2, [&](ecs::Entity instance) {
			++count;
			CHECK(wld.get<SparseCtorProbe>(instance).value == 91);
			CHECK(wld.get<Position>(instance).x == 1);
			CHECK(wld.get<Acceleration>(instance).y == 5);
			CHECK(wld.get<Rotation>(instance).w == 10);
		});
		CHECK(count == 2);
		CHECK(SparseCtorProbe::ctorCalls == 2);
		CHECK(wld.get<SparseCtorProbe>(e).value == 91);
	}
}

TEST_CASE_TEMPLATE(
		"CommandBuffer - standalone SoA payload recording", CmdBuffer, ecs::CommandBufferST, ecs::CommandBufferMT) {
	TestWorld twld;
	using SoAPair = ecs::pair<PositionSoA, CmdBufRelTarget>;
	const auto component = wld.add<PositionSoA>().entity;
	const auto target = wld.add<CmdBufRelTarget>().entity;
	const ecs::Pair pair(component, target);
	const ecs::Entity pairEntity = pair;
	CmdBuffer cb(wld);
	for (uint32_t mode = 0; mode < 4; ++mode) {
		CAPTURE(mode);
		const auto e = wld.add();
		for (uint32_t step = 0; step < 2; ++step) {
			CAPTURE(step);
			const float x = 4.0f + (float)step * 3.0f;
			if (step == 0) {
				if (mode == 0)
					cb.template add<PositionSoA>(e, {x, x + 1, x + 2});
				else if (mode == 1)
					cb.add(e, component, PositionSoA{x, x + 1, x + 2});
				else if (mode == 2)
					cb.add(e, pair, PositionSoA{x, x + 1, x + 2});
				else
					cb.add(e, pairEntity, PositionSoA{x, x + 1, x + 2});
			} else {
				if (mode == 0)
					cb.template set<PositionSoA>(e, {x, x + 1, x + 2});
				else if (mode == 1)
					cb.set(e, component, PositionSoA{x, x + 1, x + 2});
				else if (mode == 2)
					cb.set(e, pair, PositionSoA{x, x + 1, x + 2});
				else
					cb.set(e, pairEntity, PositionSoA{x, x + 1, x + 2});
			}
			cb.commit();
			const auto value = mode < 2 ? wld.get<PositionSoA>(e) : wld.get<SoAPair>(e);
			CHECK(value.x == x);
			CHECK(value.y == x + 1);
			CHECK(value.z == x + 2);
		}
	}
}

TEST_CASE_TEMPLATE(
		"CommandBuffer - per-entity structural batches", CmdBuffer, ecs::CommandBufferST, ecs::CommandBufferMT) {
	TestWorld twld;
	(void)wld.add<Position>();
	(void)wld.add<Acceleration>();
	(void)wld.add<Rotation>();
	(void)wld.add<PositionNonTrivial>();
	(void)wld.add<CmdBufPartialPayload>();
	CmdBuffer cb(wld);

	SUBCASE("Multiple additions preserve payloads with sorted and interleaved recording") {
		for (uint32_t mode = 0; mode < 2; ++mode) {
			CAPTURE(mode);
			const ecs::Entity entities[] = {wld.add(), wld.add(), wld.add()};
			if (mode == 0) {
				for (auto e: entities) {
					cb.template add<Position>(e, {4, 5, 6});
					cb.template add<Acceleration>(e, {7, 8, 9});
					cb.template add<Rotation>(e, {10, 11, 12, 13});
					cb.template add<PositionNonTrivial>(e);
				}
			} else {
				for (uint32_t i = 3; i > 0; --i)
					cb.template add<PositionNonTrivial>(entities[i - 1]);
				for (uint32_t i = 3; i > 0; --i)
					cb.template add<Rotation>(entities[i - 1], {10, 11, 12, 13});
				for (uint32_t i = 3; i > 0; --i)
					cb.template add<Acceleration>(entities[i - 1], {7, 8, 9});
				for (uint32_t i = 3; i > 0; --i)
					cb.template add<Position>(entities[i - 1], {4, 5, 6});
			}
			cb.commit();
			for (auto e: entities) {
				CHECK(wld.get<Position>(e).x == 4);
				CHECK(wld.get<Position>(e).z == 6);
				CHECK(wld.get<Acceleration>(e).y == 8);
				CHECK(wld.get<Rotation>(e).w == 13);
				const auto& value = wld.get<PositionNonTrivial>(e);
				CHECK(value.x == 1);
				CHECK(value.y == 2);
				CHECK(value.z == 3);
			}
		}
	}

	SUBCASE("Mixed additions removals and sets preserve retained and neighboring values") {
		(void)wld.add<StringComponent>();
		const auto e = wld.add();
		const auto neighbor = wld.add();
		const ecs::Entity entities[] = {e, neighbor};
		for (auto entity: entities) {
			wld.add<StringComponent>(entity, {StringComponentDefaultValue});
			wld.add<Position>(entity, {1, 2, 3});
			wld.add<Acceleration>(entity, {4, 5, 6});
			wld.add<Rotation>(entity, {7, 8, 9, 10});
		}
		cb.template add<PositionNonTrivial>(e);
		cb.template del<Acceleration>(e);
		cb.template set<Rotation>(e, {11, 12, 13, 14});
		cb.template del<Position>(e);
		cb.template add<CmdBufPartialPayload>(e, {15, 99});
		cb.commit();
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));
		CHECK(wld.get<Rotation>(e).x == 11);
		CHECK(wld.get<Rotation>(e).w == 14);
		CHECK(wld.get<PositionNonTrivial>(e).y == 2);
		CHECK(wld.get<CmdBufPartialPayload>(e).serialized == 15);
		CHECK(wld.get<CmdBufPartialPayload>(e).omitted == 7);
		CHECK(wld.get<Position>(neighbor).x == 1);
		CHECK(wld.get<Acceleration>(neighbor).y == 5);
		CHECK(wld.get<Rotation>(neighbor).w == 10);
		CHECK_FALSE(wld.has<PositionNonTrivial>(neighbor));
		CHECK_FALSE(wld.has<CmdBufPartialPayload>(neighbor));
		CHECK(wld.get<StringComponent>(e).value == StringComponentDefaultValue);
		CHECK(wld.get<StringComponent>(neighbor).value == StringComponentDefaultValue);
	}

	SUBCASE("Table SoA payloads coexist with other additions") {
		(void)wld.add<PositionSoA>();
		const auto e = wld.add();
		cb.template add<PositionSoA>(e, {4, 5, 6});
		cb.template add<Acceleration>(e, {7, 8, 9});
		cb.commit();
		CHECK(wld.get<PositionSoA>(e).x == 4);
		CHECK(wld.get<PositionSoA>(e).y == 5);
		CHECK(wld.get<PositionSoA>(e).z == 6);
		CHECK(wld.get<Acceleration>(e).y == 8);
	}

	SUBCASE("Re-added lifetimes initialize after other components change archetype") {
		const auto e = wld.add();
		wld.add<PositionNonTrivial>(e, {9, 9, 9});
		wld.add<CmdBufPartialPayload>(e, {1, 99});
		wld.add<Acceleration>(e, {4, 5, 6});
		cb.template del<PositionNonTrivial>(e);
		cb.template del<CmdBufPartialPayload>(e);
		cb.template add<Position>(e, {3, 4, 5});
		cb.template add<PositionNonTrivial>(e);
		cb.template add<CmdBufPartialPayload>(e, {2, 42});
		cb.template del<Acceleration>(e);
		cb.commit();
		const auto& position = wld.get<PositionNonTrivial>(e);
		CHECK(position.x == 1);
		CHECK(position.y == 2);
		CHECK(position.z == 3);
		CHECK(wld.get<CmdBufPartialPayload>(e).serialized == 2);
		CHECK(wld.get<CmdBufPartialPayload>(e).omitted == 7);
		CHECK(wld.get<Position>(e).x == 3);
		CHECK_FALSE(wld.has<Acceleration>(e));
	}

	SUBCASE("Temporary allocations and copies accept multiple component groups") {
		const auto source = wld.add();
		wld.add<Position>(source, {1, 2, 3});
		const auto firstId = wld.size();
		const auto first = cb.add();
		cb.template add<PositionNonTrivial>(first);
		const auto second = cb.copy(source);
		cb.template add<Acceleration>(second, {4, 5, 6});
		cb.template add<Position>(first, {7, 8, 9});
		cb.template set<Position>(second, {10, 11, 12});
		cb.template add<Rotation>(first, {13, 14, 15, 16});
		cb.template add<PositionNonTrivial>(second);
		cb.commit();
		const auto firstEntity = wld.get(firstId);
		const auto secondEntity = wld.get(firstId + 1);
		CHECK(wld.get<Position>(firstEntity).x == 7);
		CHECK(wld.get<Rotation>(firstEntity).w == 16);
		CHECK(wld.get<PositionNonTrivial>(firstEntity).y == 2);
		CHECK(wld.get<Position>(secondEntity).x == 10);
		CHECK(wld.get<Acceleration>(secondEntity).z == 6);
		CHECK(wld.get<PositionNonTrivial>(secondEntity).z == 3);
		CHECK(wld.get<Position>(source).x == 1);
		CHECK_FALSE(wld.has<Acceleration>(source));
	}

	SUBCASE("Inherited values become local overrides without changing the base") {
		const auto component = wld.add<Position>().entity;
		wld.add(component, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
		const auto base = wld.add();
		wld.add<Position>(base, {1, 2, 3});
		const auto e = wld.add();
		wld.as(e, base);
		CHECK_FALSE(wld.has_direct(e, component));
		cb.template add<Acceleration>(e, {4, 5, 6});
		cb.template add<Position>(e, {7, 8, 9});
		cb.commit();
		CHECK(wld.has_direct(e, component));
		CHECK(wld.get<Position>(e).x == 7);
		CHECK(wld.get<Acceleration>(e).z == 6);
		CHECK(wld.get<Position>(base).x == 1);
		CHECK_FALSE(wld.has<Acceleration>(base));
	}

	SUBCASE("More component groups than batch capacity retain normal replay") {
		const auto e = wld.add();
		ecs::Entity tags[ecs::ChunkHeader::MAX_COMPONENTS + 1];
		for (auto& tag: tags) {
			tag = wld.add();
			cb.add(e, tag);
			cb.del(e, tag);
		}
		cb.template add<Position>(e, {1, 2, 3});
		cb.template add<Acceleration>(e, {4, 5, 6});
		cb.commit();
		for (auto tag: tags)
			CHECK_FALSE(wld.has(e, tag));
		CHECK(wld.get<Position>(e).x == 1);
		CHECK(wld.get<Acceleration>(e).z == 6);
	}

	SUBCASE("Removal observers retain preceding writes and multi-component membership") {
		const auto e = wld.add();
		wld.add<Position>(e, {1, 1, 1});
		wld.add<Acceleration>(e, {9, 9, 9});
		uint32_t delHits = 0;
		float removedValue = 0;
		float accompanyingValue = 0;
		(void)wld.observer()
				.event(ecs::ObserverEvent::OnDel)
				.all<Position>()
				.all<Acceleration>()
				.on_each([&](const Position& position, const Acceleration& acceleration) {
					++delHits;
					removedValue = position.x;
					accompanyingValue = acceleration.x;
				})
				.entity();
		cb.template set<Position>(e, {2, 2, 2});
		cb.template del<Position>(e);
		cb.template del<Acceleration>(e);
		cb.template add<Rotation>(e, {3, 4, 5, 6});
		cb.commit();
		CHECK(delHits == 1);
		CHECK(removedValue == 2);
		CHECK(accompanyingValue == 9);
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));
		CHECK(wld.get<Rotation>(e).w == 6);
	}

	SUBCASE("Set observers retain membership before other component groups replay") {
		const auto e = wld.add();
		wld.add<Position>(e, {1, 1, 1});
		wld.add<Acceleration>(e, {9, 9, 9});
		uint32_t setHits = 0;
		(void)wld.observer()
				.event(ecs::ObserverEvent::OnSet)
				.all<Position>()
				.on_each([&](const Position& position) {
					++setHits;
					CHECK(position.x == 2);
					CHECK(wld.has<Acceleration>(e));
					CHECK_FALSE(wld.has<Rotation>(e));
				})
				.entity();
		cb.template set<Position>(e, {2, 2, 2});
		cb.template del<Acceleration>(e);
		cb.template add<Rotation>(e, {3, 4, 5, 6});
		cb.commit();
		CHECK(setHits == 1);
		CHECK_FALSE(wld.has<Acceleration>(e));
		CHECK(wld.get<Rotation>(e).w == 6);
	}

#if GAIA_ENABLE_ADD_DEL_HOOKS
	SUBCASE("Removal hooks retain preceding writes before other component groups replay") {
		static uint32_t delHits;
		delHits = 0;
		const auto& item = wld.add<Position>();
		ecs::ComponentCache::hooks(item).func_del =
				[](const ecs::World& world, const ecs::ComponentCacheItem&, ecs::Entity e) {
					++delHits;
					CHECK(world.get<Position>(e).x == 2);
					CHECK(world.has<Acceleration>(e));
					CHECK_FALSE(world.has<Rotation>(e));
				};
		const auto e = wld.add();
		wld.add<Position>(e, {1, 1, 1});
		wld.add<Acceleration>(e, {9, 9, 9});
		cb.template set<Position>(e, {2, 2, 2});
		cb.template del<Position>(e);
		cb.template del<Acceleration>(e);
		cb.template add<Rotation>(e, {3, 4, 5, 6});
		cb.commit();
		CHECK(delHits == 1);
		CHECK_FALSE(wld.has<Position>(e));
		CHECK_FALSE(wld.has<Acceleration>(e));
		CHECK(wld.get<Rotation>(e).w == 6);
		ecs::ComponentCache::hooks(item).func_del = nullptr;
	}
#endif

#if GAIA_ENABLE_ADD_DEL_HOOKS
	SUBCASE("A late removal hook retains grouped replay after two eligible component groups") {
		static uint32_t delHits;
		delHits = 0;
		const auto& item = wld.add<Rotation>();
		ecs::ComponentCache::hooks(item).func_del =
				[](const ecs::World& world, const ecs::ComponentCacheItem&, ecs::Entity e) {
					++delHits;
					CHECK(world.get<Position>(e).x == 2);
					CHECK_FALSE(world.has<Acceleration>(e));
					CHECK(world.get<Rotation>(e).w == 6);
				};
		const auto direct = wld.add();
		const auto e = wld.add();
		const ecs::Entity entities[] = {direct, e};
		for (auto entity: entities) {
			wld.add<Position>(entity, {1, 1, 1});
			wld.add<Acceleration>(entity, {9, 9, 9});
			wld.add<Rotation>(entity, {1, 1, 1, 1});
		}
		wld.set<Position>(direct) = {2, 2, 2};
		wld.del<Acceleration>(direct);
		wld.set<Rotation>(direct) = {3, 4, 5, 6};
		wld.del<Rotation>(direct);
		CHECK(delHits == 1);
		delHits = 0;
		cb.template set<Rotation>(e, {3, 4, 5, 6});
		cb.template del<Rotation>(e);
		cb.template del<Acceleration>(e);
		cb.template set<Position>(e, {2, 2, 2});
		cb.commit();
		CHECK(delHits == 1);
		CHECK(wld.get<Position>(e).x == 2);
		CHECK_FALSE(wld.has<Acceleration>(e));
		CHECK_FALSE(wld.has<Rotation>(e));
		ecs::ComponentCache::hooks(item).func_del = nullptr;
	}
#endif

#if GAIA_ENABLE_ADD_DEL_HOOKS
	SUBCASE("Entity deletion follows a batched prefix and replays the remaining payload once") {
		static uint32_t delHits;
		delHits = 0;
		const auto& item = wld.add<Rotation>();
		ecs::ComponentCache::hooks(item).func_del =
				[](const ecs::World& world, const ecs::ComponentCacheItem&, ecs::Entity e) {
					++delHits;
					CHECK(world.get<Position>(e).x == 2);
					CHECK(world.get<Acceleration>(e).y == 5);
					CHECK(world.get<Rotation>(e).w == 6);
				};
		const auto e = wld.add();
		wld.add<Rotation>(e, {1, 1, 1, 1});
		cb.template set<Rotation>(e, {3, 4, 5, 6});
		cb.template add<Acceleration>(e, {4, 5, 6});
		cb.template add<Position>(e, {2, 2, 2});
		cb.del(e);
		cb.commit();
		CHECK_FALSE(wld.has(e));
		CHECK(delHits == 1);
		ecs::ComponentCache::hooks(item).func_del = nullptr;
	}
#endif

	SUBCASE("A late protected component retains its preceding write after a batched prefix") {
		wld.add(wld.add<Rotation>().entity, ecs::Requires);
		const auto e = wld.add();
		wld.add<Position>(e, {1, 1, 1});
		wld.add<Acceleration>(e, {9, 9, 9});
		wld.add<Rotation>(e, {1, 1, 1, 1});
		cb.template set<Rotation>(e, {3, 4, 5, 6});
		cb.template del<Rotation>(e);
		cb.template del<Acceleration>(e);
		cb.template set<Position>(e, {2, 2, 2});
		cb.commit();
		CHECK(wld.get<Position>(e).x == 2);
		CHECK_FALSE(wld.has<Acceleration>(e));
		CHECK(wld.has<Rotation>(e));
		CHECK(wld.get<Rotation>(e).x == 3);
		CHECK(wld.get<Rotation>(e).w == 6);
	}

	SUBCASE("Required components keep their preceding value beside other structural changes") {
		wld.add(wld.add<Position>().entity, ecs::Requires);
		const auto e = wld.add();
		wld.add<Position>(e, {1, 1, 1});
		wld.add<Acceleration>(e, {9, 9, 9});
		cb.template set<Position>(e, {2, 2, 2});
		cb.template del<Position>(e);
		cb.template del<Acceleration>(e);
		cb.template add<Rotation>(e, {3, 4, 5, 6});
		cb.commit();
		CHECK(wld.has<Position>(e));
		CHECK(wld.get<Position>(e).x == 2);
		CHECK_FALSE(wld.has<Acceleration>(e));
		CHECK(wld.get<Rotation>(e).w == 6);
	}

	SUBCASE("Sparse lifetime changes coexist with table additions and removals") {
		for (uint32_t mode = 0; mode < 4; ++mode) {
			CAPTURE(mode);
			ecs::World sparseWorld;
			if ((mode & 2) != 0) {
				(void)sparseWorld.add<Position>();
				(void)sparseWorld.add<Acceleration>();
			}
			const auto component = sparseWorld.add<PositionSparse>().entity;
			if ((mode & 1) != 0)
				sparseWorld.add(component, ecs::DontFragment);
			(void)sparseWorld.add<Position>();
			(void)sparseWorld.add<Acceleration>();
			CmdBuffer sparseBuffer(sparseWorld);
			const auto e = sparseWorld.add();
			sparseWorld.add<PositionSparse>(e, {1, 1, 1});
			sparseWorld.add<Position>(e, {9, 9, 9});
			sparseBuffer.template del<PositionSparse>(e);
			sparseBuffer.template add<Acceleration>(e, {3, 4, 5});
			sparseBuffer.template add<PositionSparse>(e, {2, 2, 2});
			sparseBuffer.template del<Position>(e);
			sparseBuffer.commit();
			CHECK(sparseWorld.get<PositionSparse>(e).x == 2);
			CHECK(sparseWorld.get<Acceleration>(e).z == 5);
			CHECK_FALSE(sparseWorld.has<Position>(e));
		}
	}
}

TEST_CASE("CommandBuffer - instantiate prefab") {
	SUBCASE("Drops Prefab, adds Is, copies data, and skips the prefab name") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto animal = wld.add();
		const auto prefab = wld.prefab();
		wld.as(prefab, animal);
		wld.name(prefab, "prefab_animal");
		wld.add<Position>(prefab, {7, 0, 0});

		(void)cb.instantiate(prefab);
		CHECK(wld.query().is(prefab).count() == 0);

		cb.commit();

		auto q = wld.query().is(prefab);
		CHECK(q.count() == 1);

		ecs::Entity instance = ecs::EntityBad;
		q.each([&](ecs::Entity e) {
			instance = e;
		});
		CHECK(instance != ecs::EntityBad);
		CHECK_FALSE(wld.has_direct(instance, ecs::Prefab));
		CHECK(wld.has_direct(instance, ecs::Pair(ecs::Is, prefab)));
		CHECK_FALSE(wld.has_direct(instance, ecs::Pair(ecs::Is, animal)));
		CHECK(wld.has(instance, ecs::Pair(ecs::Is, animal)));
		CHECK(wld.get<Position>(instance).x == 7.0f);
		CHECK(wld.name(instance).empty());
		CHECK(wld.name(prefab) == "prefab_animal");
	}

	SUBCASE("Per-instance writes override copied prefab data") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto prefab = wld.prefab();
		wld.add<Position>(prefab, {1, 2, 3});

		const auto tmp = cb.instantiate(prefab);
		cb.set<Position>(tmp, {9, 8, 7});
		cb.commit();

		auto q = wld.query().is(prefab);
		CHECK(q.count() == 1);
		q.each([&](ecs::Entity e) {
			CHECK(wld.get<Position>(e).x == 9.0f);
			CHECK(wld.get<Position>(e).y == 8.0f);
			CHECK(wld.get<Position>(e).z == 7.0f);
		});
	}

	SUBCASE("Instantiate plus delete cancels the spawn") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto prefab = wld.prefab();
		wld.add<Position>(prefab, {1, 2, 3});

		const auto tmp = cb.instantiate(prefab);
		cb.del(tmp);
		cb.commit();

		CHECK(wld.query().is(prefab).count() == 0);
		CHECK(wld.query().all<Position>().count() == 0);
	}

	SUBCASE("instantiate_n records per-instance writes") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto prefab = wld.prefab();
		wld.add<Position>(prefab, {1, 2, 3});

		uint32_t i = 0;
		cb.instantiate_n(prefab, 3, [&](ecs::Entity e) {
			cb.set<Position>(e, {(float)i, 0, 0});
			++i;
		});
		cb.commit();

		auto q = wld.query().all<Position>().is(prefab);
		CHECK(q.count() == 3);

		bool seen[3]{};
		q.each([&](const Position& p) {
			CHECK(p.x >= 0.0f);
			CHECK(p.x <= 2.0f);
			seen[(uint32_t)p.x] = true;
		});
		CHECK(seen[0]);
		CHECK(seen[1]);
		CHECK(seen[2]);
	}

	SUBCASE("instantiate_n with zero count does nothing") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto prefab = wld.prefab();
		wld.add<Position>(prefab, {1, 2, 3});

		cb.instantiate_n(prefab, 0);
		cb.instantiate_n(prefab, wld.add(), 0);
		cb.commit();

		CHECK(wld.query().is(prefab).count() == 0);
	}

	SUBCASE("Parented instantiate attaches Parent to the spawned root") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto scene = wld.add();
		const auto prefab = wld.prefab();
		wld.add<Position>(prefab, {1, 0, 0});

		cb.instantiate_n(prefab, scene, 2);
		cb.commit();

		auto q = wld.query().is(prefab);
		CHECK(q.count() == 2);
		q.each([&](ecs::Entity e) {
			CHECK(wld.has(e, ecs::Pair(ecs::Parent, scene)));
		});
	}

	SUBCASE("Parented instantiate accepts a temporary parent") {
		struct CmdBufSceneTag {};

		TestWorld twld;
		(void)wld.add<CmdBufSceneTag>();
		ecs::CommandBufferST cb(wld);

		const auto prefab = wld.prefab();
		wld.add<Position>(prefab, {1, 0, 0});

		const auto sceneTmp = cb.add();
		cb.add<CmdBufSceneTag>(sceneTmp);
		(void)cb.instantiate(prefab, sceneTmp);
		cb.commit();

		ecs::Entity scene = ecs::EntityBad;
		wld.query().all<CmdBufSceneTag>().each([&](ecs::Entity e) {
			scene = e;
		});
		CHECK(scene != ecs::EntityBad);

		auto q = wld.query().is(prefab);
		CHECK(q.count() == 1);
		q.each([&](ecs::Entity e) {
			CHECK(wld.has(e, ecs::Pair(ecs::Parent, scene)));
		});
	}

	SUBCASE("Recurses Parent-owned prefab children") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto rootPrefab = wld.prefab();
		const auto childPrefab = wld.prefab();
		wld.parent(childPrefab, rootPrefab);
		wld.add<Position>(rootPrefab, {1, 0, 0});
		wld.add<Position>(childPrefab, {2, 0, 0});

		(void)cb.instantiate(rootPrefab);
		cb.commit();

		ecs::Entity rootInstance = ecs::EntityBad;
		wld.query().is(rootPrefab).each([&](ecs::Entity e) {
			rootInstance = e;
		});
		CHECK(rootInstance != ecs::EntityBad);

		const auto childInstance = wld.find_prefab_instance(rootInstance, childPrefab);
		CHECK(childInstance != ecs::EntityBad);
		CHECK_FALSE(wld.has_direct(rootInstance, ecs::Prefab));
		CHECK_FALSE(wld.has_direct(childInstance, ecs::Prefab));
		CHECK(wld.has_direct(childInstance, ecs::Pair(ecs::Is, childPrefab)));
		CHECK(wld.has(childInstance, ecs::Pair(ecs::Parent, rootInstance)));
		CHECK(wld.get<Position>(childInstance).x == 2.0f);
	}

	SUBCASE("Recurses ChildOf-owned prefab children") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto rootPrefab = wld.prefab();
		const auto childPrefab = wld.prefab();
		wld.child(childPrefab, rootPrefab);
		wld.add<Position>(rootPrefab, {1, 0, 0});
		wld.add<Position>(childPrefab, {2, 0, 0});

		(void)cb.instantiate(rootPrefab);
		cb.commit();

		ecs::Entity rootInstance = ecs::EntityBad;
		wld.query().is(rootPrefab).each([&](ecs::Entity e) {
			rootInstance = e;
		});
		const auto childInstance = wld.find_prefab_instance(rootInstance, childPrefab);
		CHECK(childInstance != ecs::EntityBad);
		CHECK(wld.has(childInstance, ecs::Pair(ecs::ChildOf, rootInstance)));
		CHECK(wld.get<Position>(childInstance).x == 2.0f);
	}

	SUBCASE("Non-prefab source falls back to copy") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto animal = wld.add();
		wld.name(animal, "animal");
		wld.add<Position>(animal, {4, 5, 6});

		(void)cb.instantiate(animal);
		cb.commit();

		auto q = wld.query().all<Position>();
		CHECK(q.count() == 2);

		uint32_t copies = 0;
		q.each([&](ecs::Entity e) {
			if (e == animal)
				return;
			++copies;
			CHECK_FALSE(wld.has_direct(e, ecs::Prefab));
			CHECK_FALSE(wld.has_direct(e, ecs::Pair(ecs::Is, animal)));
			CHECK(wld.name(e).empty());
			CHECK(wld.get<Position>(e).x == 4.0f);
		});
		CHECK(copies == 1);
	}

	SUBCASE("Respects DontInherit policy") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto prefab = wld.prefab();
		const auto position = wld.add<Position>().entity;
		wld.add<Position>(prefab, {7, 0, 0});
		wld.add<Scale>(prefab, {3, 0, 0});
		wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::DontInherit));

		(void)cb.instantiate(prefab);
		cb.commit();

		ecs::Entity instance = ecs::EntityBad;
		wld.query().is(prefab).each([&](ecs::Entity e) {
			instance = e;
		});
		CHECK_FALSE(wld.has<Position>(instance));
		CHECK(wld.has<Scale>(instance));
		CHECK(wld.get<Scale>(instance).x == 3.0f);
	}

	SUBCASE("Locked query iteration queues instantiate until unlock") {
		struct CmdBufPrefabSpawner {};

		TestWorld twld;
		(void)wld.add<CmdBufPrefabSpawner>();

		const auto prefab = wld.prefab();
		wld.add<Position>(prefab, {1, 2, 3});
		const auto spawner = wld.add();
		wld.add<CmdBufPrefabSpawner>(spawner);

		uint32_t queued = 0;
		wld.query().all<CmdBufPrefabSpawner>().each([&](ecs::Iter& it) {
			auto& cb = it.cmd_buffer_st();
			GAIA_EACH(it) {
				const auto tmp = cb.instantiate(prefab);
				cb.set<Position>(tmp, {9, 0, 0});
				++queued;
			}
		});

		CHECK(queued == 1);
		auto q = wld.query().is(prefab);
		CHECK(q.count() == 1);
		q.each([&](ecs::Entity e) {
			CHECK(wld.get<Position>(e).x == 9.0f);
		});
	}

	SUBCASE("CommandBufferMT instantiate uses the same Prefab and Is rules") {
		TestWorld twld;
		ecs::CommandBufferMT cb(wld);

		const auto prefab = wld.prefab();
		wld.add<Position>(prefab, {1, 2, 3});

		cb.instantiate_n(prefab, 2);
		cb.commit();

		CHECK(wld.query().is(prefab).count() == 2);
		wld.query().is(prefab).each([&](ecs::Entity e) {
			CHECK_FALSE(wld.has_direct(e, ecs::Prefab));
			CHECK(wld.has_direct(e, ecs::Pair(ecs::Is, prefab)));
			CHECK(wld.get<Position>(e).x == 1.0f);
		});
	}

	SUBCASE("Copies sparse prefab payload") {
		TestWorld twld;
		ecs::CommandBufferST cb(wld);

		const auto prefab = wld.prefab();
		wld.add<PositionSparse>(prefab, {1.0f, 2.0f, 3.0f});

		(void)cb.instantiate(prefab);
		cb.commit();

		ecs::Entity instance = ecs::EntityBad;
		wld.query().is(prefab).each([&](ecs::Entity e) {
			instance = e;
		});
		CHECK(wld.has<PositionSparse>(instance));
		CHECK(wld.get<PositionSparse>(instance).x == 1.0f);
		CHECK(wld.get<PositionSparse>(instance).y == 2.0f);
		CHECK(wld.get<PositionSparse>(instance).z == 3.0f);
	}
}

TEST_CASE("Query Filter - no systems") {
	TestWorld twld;
	ecs::Query q = wld.query().all<Position>().changed<Position>();

	auto e = wld.add();
	wld.add<Position>(e);
	wld.add<Acceleration>(e);

	// System-less filters
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 1); // first run always happens
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0); // no change of position so this shouldn't run
	}
	{
		wld.set<Position>(e) = {};
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0);
	}
	{
		wld.sset<Position>(e) = {};
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0);
	}
	{
		auto* ch = wld.get_chunk(e);
		auto p = ch->sview_mut<Position>();
		p[0] = {};
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0); // no new change since the last time
	}
	{
		wld.set<Acceleration>(e) = {4, 5, 6};
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0); // changing an unrelated component must not trigger changed<Position>
	}
	auto e2 = wld.copy(e);
	(void)e2;
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 2); // adding an entity triggers the change
	}
	{
		uint32_t cnt = 0;
		q.each([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		CHECK(cnt == 0); // no new change since the last time
	}
}

TEST_CASE("Query Filter - changed query survives removal, deletion, and row recreation") {
	TestWorld twld;
	struct Marker {};
	struct Value {
		int value;
	};

	const auto entity = wld.add();
	wld.add<Marker>(entity);
	wld.add<Value>(entity, {1});

	auto query = wld.query().all<Marker>().all<Value>().changed<Value>();
	expect_changed_consume_exact(query, {entity});

	wld.del<Value>(entity);
	expect_changed_consume_exact(query, {});

	wld.add<Value>(entity, {2});
	expect_changed_consume_exact(query, {entity});

	wld.del(entity);
	wld.update();
	expect_changed_consume_exact(query, {});

	const auto replacement = wld.add();
	wld.add<Marker>(replacement);
	wld.add<Value>(replacement, {3});
	expect_changed_consume_exact(query, {replacement});

	ecs::CommandBufferST commandBuffer(wld);
	commandBuffer.set<Value>(replacement, {4});
	commandBuffer.commit();
	expect_changed_consume_exact(query, {replacement});
}

TEST_CASE("Query Filter - changed pair query survives target deletion and recycling") {
	TestWorld twld;
	struct Marker {};

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto source = wld.add();
	const auto pair = ecs::Pair(relation, target);
	wld.add<Marker>(source);
	wld.add(source, pair);

	auto query = wld.query().all<Marker>().all(pair).changed(pair);
	expect_changed_consume_exact(query, {source});

	wld.del(target);
	wld.update();
	CHECK(wld.has(source));
	CHECK_FALSE(wld.has(source, pair));
	expect_changed_consume_exact(query, {});

	const auto recycledTarget = wld.add();
	CHECK(recycledTarget.id() == target.id());
	CHECK(recycledTarget.gen() != target.gen());
	wld.add(source, ecs::Pair(relation, recycledTarget));

	expect_changed_consume_exact(query, {source});
}

TEST_CASE("Query Filter - Iter direct mutable views track changes correctly") {
	SUBCASE("AoS") {
		TestWorld twld;

		auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});

		ecs::Query qChanged = wld.query().all<Position>().changed<Position>();
		ecs::Query qMut = wld.query().all<Position&>();

		uint32_t cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 1);

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.sview_mut<Position>(0);
			GAIA_EACH(it) {
				posView[i].x += 10.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.view_mut<Position>(0);
			GAIA_EACH(it) {
				posView[i].y += 20.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 1);
	}

	SUBCASE("SoA") {
		TestWorld twld;

		auto e = wld.add();
		wld.add<PositionSoA>(e, {1, 2, 3});

		ecs::Query qChanged = wld.query().all<PositionSoA>().changed<PositionSoA>();
		ecs::Query qMut = wld.query().all<PositionSoA&>();

		uint32_t cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 1);

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.sview_mut<PositionSoA>(0);
			auto xs = posView.template set<0>();
			GAIA_EACH(it) {
				xs[i] += 10.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.view_mut<PositionSoA>(0);
			auto ys = posView.template set<1>();
			GAIA_EACH(it) {
				ys[i] += 20.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 1);
	}
}

TEST_CASE("Query Filter - Iter auto mutable views track changes correctly") {
	SUBCASE("AoS") {
		TestWorld twld;

		auto e = wld.add();
		wld.add<Position>(e, {1, 2, 3});

		ecs::Query qChanged = wld.query().all<Position>().changed<Position>();
		ecs::Query qMut = wld.query().all<Position&>();

		uint32_t cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 1);

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.sview_auto_any<Position&>();
			auto posViewDirect = it.sview_auto<Position&>();
			GAIA_EACH(it) {
				posView[i].x += 10.0f;
				posViewDirect[i].y += 10.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.view_auto_any<Position&>();
			auto posViewDirect = it.view_auto<Position&>();
			GAIA_EACH(it) {
				posView[i].x += 20.0f;
				posViewDirect[i].y += 20.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const Position& p) {
			++cnt;
		});
		CHECK(cnt == 1);
	}

	SUBCASE("SoA") {
		TestWorld twld;

		auto e = wld.add();
		wld.add<PositionSoA>(e, {1, 2, 3});

		ecs::Query qChanged = wld.query().all<PositionSoA>().changed<PositionSoA>();
		ecs::Query qMut = wld.query().all<PositionSoA&>();

		uint32_t cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 1);

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.sview_auto_any<PositionSoA&>();
			auto posViewDirect = it.sview_auto<PositionSoA&>();
			auto xs = posView.template set<0>();
			auto ys = posViewDirect.template set<1>();
			GAIA_EACH(it) {
				xs[i] += 10.0f;
				ys[i] += 10.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 0);

		qMut.each([&](ecs::Iter& it) {
			auto posView = it.view_auto_any<PositionSoA&>();
			auto posViewDirect = it.view_auto<PositionSoA&>();
			auto xs = posView.template set<0>();
			auto ys = posViewDirect.template set<1>();
			GAIA_EACH(it) {
				xs[i] += 20.0f;
				ys[i] += 20.0f;
			}
		});

		cnt = 0;
		qChanged.each([&]([[maybe_unused]] const PositionSoA& p) {
			++cnt;
		});
		CHECK(cnt == 1);
	}
}

template <typename TQuery>
void Test_Query_Filter_Changed_Order_NoSystems() {
	constexpr bool UseCachedQuery = use_cached_query_v<TQuery>;

	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};
	struct B {
		int value;
	};

	const auto e = wld.add();
	wld.add<Marker>(e);
	wld.add<A>(e, {1});
	wld.add<B>(e, {2});

	// Intentionally reversed relative to canonical component order.
	auto q = make_query<UseCachedQuery>(wld) //
							 .template all<Marker>()
							 .template all<A>()
							 .template all<B>()
							 .template changed<B>()
							 .template changed<A>();

	CHECK(q.count() == 1);
	expect_exact_entities(q, {e});

	// No writes between runs.
	CHECK(q.count() == 0);
	expect_exact_entities(q, {});

	wld.set<A>(e) = {3};
	CHECK(q.count() == 1);
	expect_exact_entities(q, {e});

	CHECK(q.count() == 0);
	expect_exact_entities(q, {});

	wld.set<B>(e) = {4};
	CHECK(q.count() == 1);
	expect_exact_entities(q, {e});
}

TEST_CASE("Query Filter - changed order no systems") {
	SUBCASE("Cached query") {
		Test_Query_Filter_Changed_Order_NoSystems<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Filter_Changed_Order_NoSystems<QueryUncached>();
	}
}

template <typename TQuery>
void Test_Query_Filter_Changed_Or_Missing_Component() {
	constexpr bool UseCachedQuery = use_cached_query_v<TQuery>;

	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};
	struct B {
		int value;
	};
	const auto compA = wld.add<A>().entity;
	const auto compB = wld.add<B>().entity;

	const auto eA = wld.add();
	wld.add<Marker>(eA);
	wld.add<A>(eA, {1});

	const auto eB = wld.add();
	wld.add<Marker>(eB);
	wld.add<B>(eB, {2});

	// Archetypes can match with only one of OR terms present.
	// Both changed filters must remain safe.
	auto q = make_query<UseCachedQuery>(wld)
							 .template all<Marker>()
							 .template or_<A>()
							 .template or_<B>()
							 .template changed<B>()
							 .template changed<A>();
	{
		auto& info = q.fetch();
		q.match_all(info);

		uint8_t fieldA = 0xFF;
		uint8_t fieldB = 0xFF;
		for (const auto& term: info.ctx().data.terms_view()) {
			if (term.id == compA)
				fieldA = term.fieldIndex;
			else if (term.id == compB)
				fieldB = term.fieldIndex;
		}
		CHECK(fieldA != 0xFF);
		CHECK(fieldB != 0xFF);

		bool sawAOnly = false;
		bool sawBOnly = false;
		const auto archetypes = info.cache_archetype_view();
		const auto cnt = (uint32_t)archetypes.size();
		GAIA_FOR(cnt) {
			const auto* pArchetype = archetypes[i];
			const bool hasA = pArchetype->has(compA);
			const bool hasB = pArchetype->has(compB);
			if (hasA == hasB)
				continue;

			const auto indices = info.indices_mapping_view(i);
			if (hasA) {
				sawAOnly = true;
				CHECK(indices[fieldA] != 0xFF);
				CHECK(indices[fieldB] == 0xFF);
			} else {
				sawBOnly = true;
				CHECK(indices[fieldA] == 0xFF);
				CHECK(indices[fieldB] != 0xFF);
			}
		}
		CHECK(sawAOnly);
		CHECK(sawBOnly);
	}

	CHECK(q.count() == 2);
	expect_exact_entities(q, {eA, eB});

	CHECK(q.count() == 0);
	expect_exact_entities(q, {});

	wld.set<A>(eA) = {3};
	CHECK(q.count() == 1);
	expect_exact_entities(q, {eA});

	wld.set<B>(eB) = {4};
	CHECK(q.count() == 1);
	expect_exact_entities(q, {eB});
}

TEST_CASE("Query Filter - changed OR terms") {
	SUBCASE("Cached query") {
		Test_Query_Filter_Changed_Or_Missing_Component<ecs::Query>();
	}
	SUBCASE("Non-cached query") {
		Test_Query_Filter_Changed_Or_Missing_Component<QueryUncached>();
	}
}

TEST_CASE("Query Filter - changed order cache keys") {
	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};
	struct B {
		int value;
	};

	const auto e = wld.add();
	wld.add<Marker>(e);
	wld.add<A>(e, {1});
	wld.add<B>(e, {2});

	ecs::Query qAB = wld.query() //
											 .scope(ecs::QueryCacheScope::Shared)
											 .template all<Marker>()
											 .template all<A>()
											 .template all<B>()
											 .template changed<A>()
											 .template changed<B>();
	ecs::Query qBA = wld.query() //
											 .scope(ecs::QueryCacheScope::Shared)
											 .template all<Marker>()
											 .template all<A>()
											 .template all<B>()
											 .template changed<B>()
											 .template changed<A>();

	CHECK(qAB.count() == 1);
	CHECK(qBA.count() == 1);
	CHECK(qAB.id() == qBA.id());
	CHECK(qAB.gen() == qBA.gen());
}

TEST_CASE("Query Filter - cached changed queries with instance-local reporting state") {
	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};

	const auto e = wld.add();
	wld.add<Marker>(e);
	wld.add<A>(e, {1});

	ecs::Query q0 = wld.query().template all<Marker>().template all<A>().template changed<A>();
	ecs::Query q1 = wld.query().template all<Marker>().template all<A>().template changed<A>();

	CHECK(q0.id() == q1.id());
	CHECK(q0.gen() == q1.gen());

	expect_changed_probe_state(q0, 1);
	expect_changed_consume_exact(q0, {e});
	expect_changed_probe_state(q1, 1);
	expect_changed_consume_exact(q1, {e});

	wld.set<A>(e) = {2};

	expect_changed_probe_state(q0, 1);
	expect_changed_consume_exact(q0, {e});
	expect_changed_probe_state(q1, 1);
	expect_changed_consume_exact(q1, {e});
}

TEST_CASE("Query Filter - cached changed query count is non-consuming and instance-local") {
	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};

	const auto e = wld.add();
	wld.add<Marker>(e);
	wld.add<A>(e, {1});

	ecs::Query q0 = wld.query().template all<Marker>().template all<A>().template changed<A>();
	ecs::Query q1 = wld.query().template all<Marker>().template all<A>().template changed<A>();

	CHECK(q0.id() == q1.id());
	CHECK(q0.gen() == q1.gen());

	expect_changed_probe_state(q0, 1);
	expect_changed_probe_state(q1, 1);

	expect_changed_consume_exact(q0, {e});
	expect_changed_probe_state(q1, 1);

	expect_changed_consume_exact(q1, {e});
}

TEST_CASE("Query Filter - cached changed query empty is non-consuming and instance-local") {
	TestWorld twld;
	struct Marker {};
	struct A {
		int value;
	};

	const auto e = wld.add();
	wld.add<Marker>(e);
	wld.add<A>(e, {1});

	ecs::Query q0 = wld.query().template all<Marker>().template all<A>().template changed<A>();
	ecs::Query q1 = wld.query().template all<Marker>().template all<A>().template changed<A>();

	CHECK(q0.id() == q1.id());
	CHECK(q0.gen() == q1.gen());

	expect_changed_probe_state(q0, 1);
	expect_changed_probe_state(q1, 1);

	expect_changed_consume_exact(q0, {e});
	expect_changed_probe_state(q1, 1);

	expect_changed_consume_exact(q1, {e});
}

TEST_CASE("Query Filter - cached changed queries with instance-local var bindings") {
	TestWorld twld;
	struct Ship {};
	struct Planet {};
	struct Status {
		int value;
	};

	const auto dockedTo = wld.add();
	const auto earth = wld.add();
	const auto mars = wld.add();
	wld.add<Planet>(earth);
	wld.add<Planet>(mars);

	const auto shipEarth = wld.add();
	wld.add<Ship>(shipEarth);
	wld.add<Status>(shipEarth, {1});
	wld.add(shipEarth, ecs::Pair(dockedTo, earth));

	const auto shipMars = wld.add();
	wld.add<Ship>(shipMars);
	wld.add<Status>(shipMars, {2});
	wld.add(shipMars, ecs::Pair(dockedTo, mars));

	auto makeQuery = [&twld, dockedTo] {
		return wld.query()
				.template all<Ship>()
				.template all<Status>()
				.all(ecs::Pair(dockedTo, ecs::Var0))
				.template all<Planet>(ecs::QueryTermOptions{}.src(ecs::Var0))
				.template changed<Status>();
	};

	auto qEarth = makeQuery();
	auto qMars = makeQuery();

	qEarth.set_var(ecs::Var0, earth);
	qMars.set_var(ecs::Var0, mars);

	CHECK(qEarth.id() == qMars.id());
	CHECK(qEarth.gen() == qMars.gen());

	expect_changed_probe_state(qEarth, 1);
	expect_changed_consume_exact(qEarth, {shipEarth});
	expect_changed_probe_state(qMars, 1);
	expect_changed_consume_exact(qMars, {shipMars});

	wld.set<Status>(shipEarth) = {3};

	expect_changed_probe_state(qEarth, 1);
	expect_changed_consume_exact(qEarth, {shipEarth});
	expect_changed_probe_state(qMars, 0);
	expect_changed_consume_exact(qMars, {});

	wld.set<Status>(shipMars) = {4};

	expect_changed_probe_state(qEarth, 0);
	expect_changed_consume_exact(qEarth, {});
	expect_changed_probe_state(qMars, 1);
	expect_changed_consume_exact(qMars, {shipMars});
}

TEST_CASE("Query Filter - cached changed queries with instance-local group filters") {
	TestWorld twld;
	struct Position {
		float x, y, z;
	};

	const auto eats = wld.add();
	const auto carrot = wld.add();
	const auto salad = wld.add();

	const auto eCarrotA = wld.add();
	wld.add<Position>(eCarrotA, {1, 0, 0});
	wld.add(eCarrotA, ecs::Pair(eats, carrot));

	const auto eCarrotB = wld.add();
	wld.add<Position>(eCarrotB, {2, 0, 0});
	wld.add(eCarrotB, ecs::Pair(eats, carrot));

	const auto eSaladA = wld.add();
	wld.add<Position>(eSaladA, {3, 0, 0});
	wld.add(eSaladA, ecs::Pair(eats, salad));

	const auto eSaladB = wld.add();
	wld.add<Position>(eSaladB, {4, 0, 0});
	wld.add(eSaladB, ecs::Pair(eats, salad));

	auto qCarrot = wld.query().all<Position>().group_by(eats).changed<Position>();
	auto qSalad = wld.query().all<Position>().group_by(eats).changed<Position>();

	qCarrot.group_id(carrot);
	qSalad.group_id(salad);

	CHECK(qCarrot.id() == qSalad.id());
	CHECK(qCarrot.gen() == qSalad.gen());

	expect_changed_probe_state(qCarrot, 2);
	expect_changed_consume_exact(qCarrot, {eCarrotA, eCarrotB});
	expect_changed_probe_state(qSalad, 2);
	expect_changed_consume_exact(qSalad, {eSaladA, eSaladB});

	wld.set<Position>(eCarrotA) = {10, 0, 0};

	expect_changed_probe_state(qCarrot, 2);
	expect_changed_consume_exact(qCarrot, {eCarrotA, eCarrotB});
	expect_changed_probe_state(qSalad, 0);
	expect_changed_consume_exact(qSalad, {});

	wld.set<Position>(eSaladB) = {20, 0, 0};

	expect_changed_probe_state(qCarrot, 0);
	expect_changed_consume_exact(qCarrot, {});
	expect_changed_probe_state(qSalad, 2);
	expect_changed_consume_exact(qSalad, {eSaladA, eSaladB});
}

TEST_CASE("Query Filter - cached changed traversed source queries with instance-local var bindings") {
	TestWorld twld;
	struct Status {
		int value;
	};

	const auto rootEarth = wld.add();
	const auto rootMars = wld.add();
	wld.add<Acceleration>(rootEarth);
	wld.add<Acceleration>(rootMars);

	const auto parentEarth = wld.add();
	const auto parentMars = wld.add();
	wld.child(parentEarth, rootEarth);
	wld.child(parentMars, rootMars);

	const auto childEarth = wld.add();
	const auto childMars = wld.add();
	wld.add<Status>(childEarth, {1});
	wld.add<Status>(childMars, {2});
	wld.child(childEarth, parentEarth);
	wld.child(childMars, parentMars);

	auto makeQuery = [&] {
		return wld.query()
				.template all<Status>()
				.all(ecs::Pair(ecs::ChildOf, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.template changed<Status>();
	};

	auto qEarth = makeQuery();
	auto qMars = makeQuery();

	qEarth.set_var(ecs::Var0, parentEarth);
	qMars.set_var(ecs::Var0, parentMars);

	CHECK(qEarth.id() == qMars.id());
	CHECK(qEarth.gen() == qMars.gen());

	expect_changed_probe_state(qEarth, 1);
	expect_changed_consume_exact(qEarth, {childEarth});
	expect_changed_probe_state(qMars, 1);
	expect_changed_consume_exact(qMars, {childMars});

	wld.set<Status>(childEarth) = {3};

	expect_changed_probe_state(qEarth, 1);
	expect_changed_consume_exact(qEarth, {childEarth});
	expect_changed_probe_state(qMars, 0);
	expect_changed_consume_exact(qMars, {});

	wld.set<Status>(childMars) = {4};

	expect_changed_probe_state(qEarth, 0);
	expect_changed_consume_exact(qEarth, {});
	expect_changed_probe_state(qMars, 1);
	expect_changed_consume_exact(qMars, {childMars});
}

TEST_CASE("Query Filter - cached changed Parent traversed source queries report changed chunks") {
	TestWorld twld;
	struct Status {
		int value;
	};

	const auto rootEarth = wld.add();
	const auto rootMars = wld.add();
	wld.add<Acceleration>(rootEarth);
	wld.add<Acceleration>(rootMars);

	const auto parentEarth = wld.add();
	const auto parentMars = wld.add();
	wld.parent(parentEarth, rootEarth);
	wld.parent(parentMars, rootMars);

	const auto childEarth = wld.add();
	const auto childMars = wld.add();
	wld.add<Status>(childEarth, {1});
	wld.add<Status>(childMars, {2});
	wld.parent(childEarth, parentEarth);
	wld.parent(childMars, parentMars);

	auto makeQuery = [&](ecs::Entity parent) {
		return wld.query()
				.template all<Status>()
				.all(ecs::Pair(ecs::Parent, parent))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(parent).trav(ecs::Parent))
				.template changed<Status>();
	};

	auto qEarth = makeQuery(parentEarth);
	auto qMars = makeQuery(parentMars);

	expect_changed_probe_state(qEarth, 1);
	expect_changed_consume_exact(qEarth, {childEarth});
	expect_changed_probe_state(qMars, 1);
	expect_changed_consume_exact(qMars, {childMars});

	wld.set<Status>(childEarth) = {3};

	// Parent is non-fragmenting, so both filtered rows stay in the same changed chunk.
	expect_changed_probe_state(qEarth, 1);
	expect_changed_consume_exact(qEarth, {childEarth});
	expect_changed_probe_state(qMars, 1);
	expect_changed_consume_exact(qMars, {childMars});

	wld.set<Status>(childMars) = {4};

	expect_changed_probe_state(qEarth, 1);
	expect_changed_consume_exact(qEarth, {childEarth});
	expect_changed_probe_state(qMars, 1);
	expect_changed_consume_exact(qMars, {childMars});
}

TEST_CASE("Query Filter - systems") {
	uint32_t expectedCnt = 0;
	uint32_t actualCnt = 0;
	uint32_t wsCnt = 0;
	uint32_t wssCnt = 0;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e);

	// WriterSystem
	auto ws = wld.system()
								.name("WriterSystem")
								.no<ecs::System_>()
								.all<Position&>()
								.on_each([&](Position& a) {
									++wsCnt;
									(void)a;
								})
								.entity();
	// WriterSystemSilent
	auto wss = wld.system()
								 .name("WriterSystemSilent")
								 .no<ecs::System_>()
								 .all<Position&>()
								 .on_each([&](ecs::Iter& it) {
									 ++wssCnt;
									 auto posRWView = it.sview_mut<Position>();
									 (void)posRWView;
								 })
								 .entity();
	// ReaderSystem
	auto rs = wld.system()
								.name("ReaderSystem")
								.no<ecs::System_>()
								.all<Position>()
								.changed<Position>()
								.on_each([&](ecs::Iter& it) {
									GAIA_EACH(it)++ actualCnt;
								})
								.entity();
	(void)rs;

	// first run always happens
	{
		wld.enable(ws, false);
		wld.enable(wss, false);
		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		expectedCnt = 1;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt == 0);
		CHECK(wssCnt == 0);
	}
	// no change of position so ReaderSystem should't see any changes
	{
		expectedCnt = 0;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt == 0);
		CHECK(wssCnt == 0);
	}
	// update position so ReaderSystem should detect a change
	{
		wld.enable(ws, true);
		CHECK(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		expectedCnt = 1;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt > 0);
		CHECK(wssCnt == 0);
	}
	// no change of position so ReaderSystem shouldn't see any changes
	{
		wld.enable(ws, false);
		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		expectedCnt = 0;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK_FALSE(wld.enabled(ws));
		CHECK_FALSE(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt == 0);
		CHECK(wssCnt == 0);
	}
	// silent writer enabled again. If should not trigger an update
	{
		wld.enable(ws, false);
		wld.enable(wss, true);
		CHECK_FALSE(wld.enabled(ws));
		CHECK(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		expectedCnt = 0;
		actualCnt = 0;
		wsCnt = 0;
		wssCnt = 0;
		wld.update();

		CHECK_FALSE(wld.enabled(ws));
		CHECK(wld.enabled(wss));
		CHECK(wld.enabled(rs));

		CHECK(actualCnt == expectedCnt);
		CHECK(wsCnt == 0);
		CHECK(wssCnt > 0);
	}
}

struct Eats {};
struct Healthy {};
static uint32_t g_query_sort_cmp_cnt = 0;
static int compare_position_counted([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
	++g_query_sort_cmp_cnt;

	const auto& p0 = *static_cast<const Position*>(pData0);
	const auto& p1 = *static_cast<const Position*>(pData1);
	if (p0.x < p1.x)
		return -1;
	if (p0.x > p1.x)
		return 1;
	return 0;
}
ecs::GroupId
group_by_rel([[maybe_unused]] const ecs::World& world, const ecs::Archetype& archetype, ecs::Entity groupBy) {
	if (archetype.pairs() > 0) {
		auto ids = archetype.ids_view();
		for (auto id: ids) {
			if (!id.pair() || id.id() != groupBy.id())
				continue;

			// Consider the pair's target the groupId
			return id.gen();
		}
	}

	// No group
	return 0;
}

TEST_CASE("Query - group") {
	TestWorld twld;

	ecs::Entity eats = wld.add(); // 16
	ecs::Entity carrot = wld.add(); // 17
	ecs::Entity salad = wld.add(); // 18
	ecs::Entity apple = wld.add(); // 19

	ecs::Entity ents[6];
	GAIA_FOR(6) ents[i] = wld.add(); // 20, 21, 22, 23, 24, 25
	(void)wld.add<Position>();
	(void)wld.add<Healthy>();
	{
		// 26 - Position
		// 27 - Healthy
		wld.build(ents[0]).add<Position>().add({eats, salad}); // 20 <-- Pos, {Eats,Salad}
		wld.build(ents[1]).add<Position>().add({eats, carrot});
		wld.build(ents[2]).add<Position>().add({eats, apple});

		wld.build(ents[3]).add<Position>().add({eats, apple}).add<Healthy>();
		wld.build(ents[4]).add<Position>().add({eats, salad}).add<Healthy>();
		wld.build(ents[5]).add<Position>().add({eats, carrot}).add<Healthy>();
	}
	// This query is going to group entities by what they eat.
	// The query cache is going to contain following 6 archetypes in 3 groups as follows:
	//  - Eats:carrot:
	//     - Position, (Eats, carrot)
	//     - Position, (Eats, carrot), Healthy
	//  - Eats:salad:
	//     - Position, (Eats, salad)
	//     - Position, (Eats, salad), Healthy
	//  - Eats::apple:
	//     - Position, (Eats, apple)
	//     - Position, (Eats, apple), Healthy
	ecs::Entity ents_expected[] = {ents[1], ents[5], // carrot, 21, 25
																 ents[0], ents[4], // salad, 20, 24
																 ents[2], ents[3]}; // apple, 22, 23

	auto checkQuery = [&](ecs::Query& q, //
												std::span<ecs::Entity> ents_expected_view) {
		{
			uint32_t j = 0;
			q.each([&](ecs::Iter& it) {
				auto ents = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					const auto e = ents[i];
					const auto e_wanted = ents_expected_view[j++];
					CHECK(e == e_wanted);
				}
			});
			CHECK(j == (uint32_t)ents_expected_view.size());
		}
		{
			uint32_t j = 0;
			q.each([&](ecs::Entity e) {
				const auto e_wanted = ents_expected_view[j++];
				CHECK(e == e_wanted);
			});
			CHECK(j == (uint32_t)ents_expected_view.size());
		}
	};

	{
		auto qq = wld.query().all<Position>().group_by(eats);

		// group_by partitions the cache but does not order iteration when no group is selected.
		checkQuery(qq, {&ents[0], 6});
		// Grouping on, a group is enforced
		qq.group_id(carrot);
		checkQuery(qq, {&ents_expected[0], 2});
		qq.group_id(salad);
		checkQuery(qq, {&ents_expected[2], 2});
		qq.group_id(apple);
		checkQuery(qq, {&ents_expected[4], 2});
	}

	{
		auto qq = wld.query().all<Position>().group_by(eats, group_by_rel);

		// group_by partitions the cache but does not order iteration when no group is selected.
		checkQuery(qq, {&ents[0], 6});
		// Grouping on, a group is enforced
		qq.group_id(carrot);
		checkQuery(qq, {&ents_expected[0], 2});
		qq.group_id(salad);
		checkQuery(qq, {&ents_expected[2], 2});
		qq.group_id(apple);
		checkQuery(qq, {&ents_expected[4], 2});
	}

	{
		const auto missingFood = wld.add();
		auto qq = wld.query().all<Position>().group_by(eats);
		qq.group_id(missingFood);

		CHECK(qq.count() == 0);
		CHECK(qq.empty());

		uint32_t iterCnt = 0;
		qq.each([&](ecs::Iter& it) {
			iterCnt += it.size();
		});
		CHECK(iterCnt == 0);

		uint32_t entityCnt = 0;
		qq.each([&](ecs::Entity) {
			++entityCnt;
		});
		CHECK(entityCnt == 0);

		cnt::darr<ecs::Entity> entities;
		qq.arr(entities);
		CHECK(entities.empty());

		cnt::darr<Position> positions;
		qq.arr(positions);
		CHECK(positions.empty());
	}
}

TEST_CASE("Query - groups") {
	TestWorld twld;

	const auto eats = wld.add();
	const auto carrot = wld.add();
	const auto salad = wld.add();
	const auto apple = wld.add();
	(void)wld.add<Position>();
	(void)wld.add<Healthy>();

	const auto eNoGroup = wld.add();
	const auto eSalad = wld.add();
	const auto eCarrotA = wld.add();
	const auto eApple = wld.add();
	const auto eCarrotB = wld.add();

	wld.add<Position>(eNoGroup);
	wld.build(eSalad).add<Position>().add({eats, salad});
	wld.build(eCarrotA).add<Position>().add({eats, carrot});
	wld.build(eApple).add<Position>().add({eats, apple});
	wld.build(eCarrotB).add<Position>().add({eats, carrot}).add<Healthy>();

	auto qq = wld.query().all<Position>().group_by(eats);
	cnt::darr<ecs::GroupId> groups;
	qq.groups(groups, true);

	CHECK(groups.size() == 3);
	CHECK(groups[0] == carrot.id());
	CHECK(groups[1] == salad.id());
	CHECK(groups[2] == apple.id());

	uint32_t total = 0;
	GAIA_FOR((uint32_t)groups.size()) {
		qq.group_id(groups[i]);
		total += qq.count();
	}
	CHECK(total == 4);

	const auto banana = wld.add();
	const auto eBanana = wld.add();
	wld.build(eBanana).add<Position>().add({eats, banana});

	qq.groups(groups, true);
	CHECK(groups.size() == 4);
	CHECK(groups[3] == banana.id());
	CHECK(qq.group_id(banana).count() == 1);
}

TEST_CASE("Query - sort") {
	TestWorld twld;

	ecs::Entity e0 = wld.add();
	ecs::Entity e1 = wld.add();
	ecs::Entity e2 = wld.add();
	ecs::Entity e3 = wld.add();

	wld.add<Position>(e0, {2, 0, 0});
	wld.add<Position>(e1, {4, 0, 0});
	wld.add<Position>(e2, {1, 0, 0});
	wld.add<Position>(e3, {3, 0, 0});

	SUBCASE("By entity index") {
		auto q = wld.query().all<Position>().sort_by(
				ecs::EntityBad, []([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
					const auto& e0 = *static_cast<const ecs::Entity*>(pData0);
					const auto& e1 = *static_cast<const ecs::Entity*>(pData1);
					return (int)e0.id() - (int)e1.id();
				});
		q.each([&](ecs::Iter& it) {
			auto ents = it.view<ecs::Entity>();
			CHECK(ents[0] == e0);
			CHECK(ents[1] == e1);
			CHECK(ents[2] == e2);
			CHECK(ents[3] == e3);
		});
	}

	SUBCASE("By component value (1)") {
		auto q = wld.query().all<Position>().sort_by<Position>(
				[]([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
					const auto& p0 = *static_cast<const Position*>(pData0);
					const auto& p1 = *static_cast<const Position*>(pData1);
					const float diff = p0.x - p1.x;
					if (diff < 0.f)
						return -1;
					if (diff > 0.f)
						return 1;
					return 0;
				});
		q.each([&](ecs::Iter& it) {
			auto ents = it.view<ecs::Entity>();
			CHECK(ents[0] == e2);
			CHECK(ents[1] == e0);
			CHECK(ents[2] == e3);
			CHECK(ents[3] == e1);
		});
	}

	SUBCASE("By component value (2)") {
		auto q = wld.query().all<Position>().sort_by(
				wld.get<Position>(), //
				[]([[maybe_unused]] const ecs::World& world, const void* pData0, const void* pData1) {
					const auto& p0 = *static_cast<const Position*>(pData0);
					const auto& p1 = *static_cast<const Position*>(pData1);
					const float diff = p0.x - p1.x;
					if (diff < 0.f)
						return -1;
					if (diff > 0.f)
						return 1;
					return 0;
				});
		q.each([&](ecs::Iter& it) {
			auto ents = it.view<ecs::Entity>();
			CHECK(ents[0] == e2);
			CHECK(ents[1] == e0);
			CHECK(ents[2] == e3);
			CHECK(ents[3] == e1);
		});

		cnt::darr<ecs::Entity> tmp;

		// Change some archetype
		{
			wld.add<Something>(e0, {false});
			q.each([&tmp](ecs::Iter& it) {
				auto ents = it.view<ecs::Entity>();
				GAIA_EACH(ents) tmp.push_back(ents[i]);
			});

			CHECK(tmp[0] == e2);
			CHECK(tmp[1] == e0);
			CHECK(tmp[2] == e3);
			CHECK(tmp[3] == e1);
		}

		// Add new entity
		auto e4 = wld.add();
		{
			wld.add<Position>(e4, {0, 0, 0});
			tmp.clear();
			q.each([&tmp](ecs::Iter& it) {
				auto ents = it.view<ecs::Entity>();
				GAIA_EACH(ents) tmp.push_back(ents[i]);
			});

			CHECK(tmp[0] == e4);
			CHECK(tmp[1] == e2);
			CHECK(tmp[2] == e0);
			CHECK(tmp[3] == e3);
			CHECK(tmp[4] == e1);
		}

		// Delete entity
		{
			wld.del(e0);
			tmp.clear();
			q.each([&tmp](ecs::Iter& it) {
				auto ents = it.view<ecs::Entity>();
				GAIA_EACH(ents) tmp.push_back(ents[i]);
			});
			CHECK(tmp[0] == e4);
			CHECK(tmp[1] == e2);
			CHECK(tmp[2] == e3);
			CHECK(tmp[3] == e1);
		}
	}

	SUBCASE("Doesn't resort after unrelated component write") {
		wld.add<Something>(e0, {false});
		wld.add<Something>(e1, {false});
		wld.add<Something>(e2, {false});
		wld.add<Something>(e3, {false});

		g_query_sort_cmp_cnt = 0;
		auto q = wld.query().all<Position>().all<Something>().sort_by<Position>(compare_position_counted);

		q.each([](ecs::Iter&) {});
		CHECK(g_query_sort_cmp_cnt > 0);

		g_query_sort_cmp_cnt = 0;
		auto something = wld.set<Something>(e0);
		something.value = true;
		q.each([](ecs::Iter&) {});
		CHECK(g_query_sort_cmp_cnt == 0);
	}

	SUBCASE("Resorts after entity order changes") {
		wld.add<Something>(e0, {false});
		wld.add<Something>(e1, {false});
		wld.add<Something>(e2, {false});
		wld.add<Something>(e3, {false});

		g_query_sort_cmp_cnt = 0;
		auto q = wld.query().all<Position>().all<Something>().sort_by<Position>(compare_position_counted);

		q.each([](ecs::Iter&) {});
		g_query_sort_cmp_cnt = 0;

		auto e4 = wld.add();
		wld.add<Position>(e4, {0, 0, 0});
		wld.add<Something>(e4, {true});
		q.each([](ecs::Iter&) {});
		CHECK(g_query_sort_cmp_cnt > 0);
	}
}

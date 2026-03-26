#include "common.h"
#include "registry.h"

namespace {
	struct XorShift32 {
		uint32_t state = 0x12345678u;

		explicit XorShift32(uint32_t seed): state(seed == 0 ? 0x12345678u : seed) {}

		GAIA_NODISCARD uint32_t next() noexcept {
			auto x = state;
			x ^= x << 13;
			x ^= x >> 17;
			x ^= x << 5;
			state = x;
			return x;
		}
	};

	struct ListHandle {
		static constexpr uint32_t IdMask = uint32_t(-1);

		uint32_t m_id = IdMask;
		uint32_t m_gen = 0;

		ListHandle() = default;
		ListHandle(uint32_t id, uint32_t gen): m_id(id), m_gen(gen) {}

		GAIA_NODISCARD uint32_t id() const noexcept {
			return m_id;
		}
		GAIA_NODISCARD uint32_t gen() const noexcept {
			return m_gen;
		}

		GAIA_NODISCARD bool operator==(const ListHandle& other) const noexcept {
			return m_id == other.m_id && m_gen == other.m_gen;
		}
	};

	struct ListItemCtx {
		uint32_t value = 0;
	};

	struct ListItem {
		uint32_t idx = 0;
		struct ItemData {
			uint32_t gen = 0;
		} data;
		uint32_t value = 0;

		ListItem() = default;
		ListItem(uint32_t index, uint32_t generation): idx(index) {
			data.gen = generation;
		}

		GAIA_NODISCARD static ListItem create(uint32_t index, uint32_t generation, void* pCtx) {
			ListItem item(index, generation);
			if (pCtx != nullptr)
				item.value = ((ListItemCtx*)pCtx)->value;
			return item;
		}

		GAIA_NODISCARD static ListHandle handle(const ListItem& item) {
			return ListHandle(item.idx, item.data.gen);
		}
	};

	template <typename TList>
	void init_dense(TList& list, cnt::darray<ListHandle>& handles, uint32_t count) {
		list.reserve(count);
		handles.reserve(count);
		GAIA_FOR(count) {
			ListItemCtx ctx{i + 1};
			handles.push_back(list.alloc(&ctx));
		}
	}

	void remove_swap(cnt::darray<ListHandle>& handles, uint32_t idx) {
		GAIA_ASSERT(idx < handles.size());
		if (idx + 1 != handles.size())
			handles[idx] = handles.back();
		handles.pop_back();
	}

	void init_random_order(cnt::darray<uint32_t>& order, uint32_t count, uint32_t seed) {
		order.resize(count);
		GAIA_FOR(count)
		order[i] = i;

		XorShift32 rng(seed);
		for (uint32_t i = count; i > 1; --i) {
			const auto j = rng.next() % i;
			const auto tmp = order[i - 1];
			order[i - 1] = order[j];
			order[j] = tmp;
		}
	}

	template <typename TList>
	void init_stable_sparse(
			TList& list, cnt::darray<ListHandle>& activeHandles, cnt::darray<uint32_t>& accessOrder, uint32_t count) {
		cnt::darray<ListHandle> handles;
		init_dense(list, handles, count);

		activeHandles.reserve(count - count / 4);
		GAIA_FOR(count) {
			if ((i & 3U) == 0U) {
				list.free(handles[i]);
				continue;
			}

			activeHandles.push_back(handles[i]);
		}

		init_random_order(accessOrder, (uint32_t)activeHandles.size(), 0xC001D00Du);
	}

	template <typename TList>
	void run_alloc_free(picobench::state& state) {
		const auto count = (uint32_t)state.user_data();

		TList list;
		cnt::darray<ListHandle> handles;
		init_dense(list, handles, count);

		for (auto _: state) {
			(void)_;

			for (auto handle: handles)
				list.free(handle);

			handles.clear();
			GAIA_FOR(count) {
				ListItemCtx ctx{i + 1};
				handles.push_back(list.alloc(&ctx));
			}

			gaia::dont_optimize(list.item_count());
		}
	}

	template <typename TList>
	void run_iter_dense(picobench::state& state) {
		const auto count = (uint32_t)state.user_data();

		TList list;
		cnt::darray<ListHandle> handles;
		init_dense(list, handles, count);
		gaia::dont_optimize(handles.size());

		for (auto _: state) {
			(void)_;

			uint64_t sum = 0;
			for (const auto& item: list)
				sum += item.value;

			gaia::dont_optimize(sum);
		}
	}

	template <typename TList>
	void run_access_stable_random(picobench::state& state) {
		const auto count = (uint32_t)state.user_data();

		TList list;
		cnt::darray<ListHandle> activeHandles;
		cnt::darray<uint32_t> accessOrder;
		init_stable_sparse(list, activeHandles, accessOrder, count);
		gaia::dont_optimize(list.item_count());

		for (auto _: state) {
			(void)_;

			uint64_t sum = 0;
			for (const auto idx: accessOrder) {
				const auto handle = activeHandles[idx];
				sum += list[handle.id()].value;
			}

			gaia::dont_optimize(sum);
		}
	}

	template <typename TList>
	void run_mixed(picobench::state& state) {
		const auto count = (uint32_t)state.user_data();
		const auto probeCount = count / 2;
		const auto churnCount = count >= 32 ? count / 16 : 2;

		TList list;
		cnt::darray<ListHandle> activeHandles;
		init_dense(list, activeHandles, count);

		XorShift32 rng(0xA511E9B3u);
		uint32_t nextValue = count + 1;

		for (auto _: state) {
			(void)_;

			uint64_t sum = 0;

			GAIA_FOR(churnCount) {
				if (activeHandles.empty())
					break;

				const auto pos = rng.next() % (uint32_t)activeHandles.size();
				const auto handle = activeHandles[pos];
				sum += list[handle.id()].value;
				list.free(handle);
				remove_swap(activeHandles, pos);
			}

			GAIA_FOR(churnCount) {
				ListItemCtx ctx{nextValue++};
				activeHandles.push_back(list.alloc(&ctx));
			}

			GAIA_FOR(probeCount) {
				const auto pos = rng.next() % (uint32_t)activeHandles.size();
				const auto handle = activeHandles[pos];
				sum += list[handle.id()].value;
			}

			for (const auto& item: list)
				sum += item.value;

			gaia::dont_optimize(sum);
			gaia::dont_optimize(activeHandles.size());
			gaia::dont_optimize(list.item_count());
		}
	}
} // namespace

void BM_IList_AllocFree(picobench::state& state) {
	run_alloc_free<cnt::ilist<ListItem, ListHandle>>(state);
}

void BM_PagedIList_AllocFree(picobench::state& state) {
	run_alloc_free<cnt::paged_ilist<ListItem, ListHandle>>(state);
}

void BM_IList_IterDense(picobench::state& state) {
	run_iter_dense<cnt::ilist<ListItem, ListHandle>>(state);
}

void BM_PagedIList_IterDense(picobench::state& state) {
	run_iter_dense<cnt::paged_ilist<ListItem, ListHandle>>(state);
}

void BM_IList_AccessStableRandom(picobench::state& state) {
	run_access_stable_random<cnt::ilist<ListItem, ListHandle>>(state);
}

void BM_PagedIList_AccessStableRandom(picobench::state& state) {
	run_access_stable_random<cnt::paged_ilist<ListItem, ListHandle>>(state);
}

void BM_IList_Mixed(picobench::state& state) {
	run_mixed<cnt::ilist<ListItem, ListHandle>>(state);
}

void BM_PagedIList_Mixed(picobench::state& state) {
	run_mixed<cnt::paged_ilist<ListItem, ListHandle>>(state);
}

#define PICO_SETTINGS() iterations({128}).samples(5)
#define PICO_SETTINGS_SANI() iterations({8}).samples(1)
#define PICOBENCH_SUITE_REG(name) (void)picobench::global_registry::set_bench_suite(name)
#define PICOBENCH_REG(func) picobench::global_registry::new_benchmark(#func, func)

void register_containers(PerfRunMode mode) {
	switch (mode) {
		case PerfRunMode::Profiling:
			PICOBENCH_SUITE_REG("Profile picks");
			PICOBENCH_REG(BM_PagedIList_Mixed).PICO_SETTINGS().user_data(100'000).label("paged_ilist mixed");
			return;
		case PerfRunMode::Sanitizer:
			PICOBENCH_SUITE_REG("Sanitizer picks");
			PICOBENCH_REG(BM_PagedIList_Mixed).PICO_SETTINGS_SANI().user_data(10'000).label("paged_ilist mixed");
			return;
		case PerfRunMode::Normal:
			PICOBENCH_SUITE_REG("Containers");
			PICOBENCH_REG(BM_IList_AllocFree).PICO_SETTINGS().user_data(100'000).label("ilist alloc/free");
			PICOBENCH_REG(BM_PagedIList_AllocFree).PICO_SETTINGS().user_data(100'000).label("paged_ilist alloc/free");
			PICOBENCH_REG(BM_IList_IterDense).PICO_SETTINGS().user_data(100'000).label("ilist iter dense");
			PICOBENCH_REG(BM_PagedIList_IterDense).PICO_SETTINGS().user_data(100'000).label("paged_ilist iter dense");
			PICOBENCH_REG(BM_IList_AccessStableRandom).PICO_SETTINGS().user_data(100'000).label("ilist access stable random");
			PICOBENCH_REG(BM_PagedIList_AccessStableRandom)
					.PICO_SETTINGS()
					.user_data(100'000)
					.label("paged_ilist access stable random");
			PICOBENCH_REG(BM_IList_Mixed).PICO_SETTINGS().user_data(100'000).label("ilist mixed");
			PICOBENCH_REG(BM_PagedIList_Mixed).PICO_SETTINGS().user_data(100'000).label("paged_ilist mixed");
			return;
	}
}

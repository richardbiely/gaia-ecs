#include "common.h"
#include "registry.h"

#include <cstring>
#include <functional>

namespace {
	constexpr uint32_t InvokeOps = 64 * 1024;
	constexpr uint32_t ConstructOps = 16 * 1024;

	struct SmallCallable {
		int32_t bias = 0;

		int32_t operator()(int32_t lhs, int32_t rhs) const noexcept {
			return lhs + rhs + bias;
		}
	};

	struct LargeCallable {
		int32_t bias = 0;
		uint8_t payload[128]{};

		int32_t operator()(int32_t lhs, int32_t rhs) const noexcept {
			return lhs + rhs + bias + payload[0];
		}
	};

	struct HeapCallable {
		int32_t bias = 0;
		uint8_t payload[1024]{};

		int32_t operator()(int32_t lhs, int32_t rhs) const noexcept {
			return lhs + rhs + bias + payload[0];
		}
	};

	volatile uintptr_t g_trackedLargeCallableGuard = 0;
	volatile uintptr_t g_trackedHeapCallableGuard = 0;

	struct TrackedLargeCallable {
		int32_t bias = 0;
		uint8_t payload[128]{};

		TrackedLargeCallable() = default;
		TrackedLargeCallable(int32_t value): bias(value) {}

		TrackedLargeCallable(const TrackedLargeCallable& other): bias(other.bias) {
			std::memcpy(payload, other.payload, sizeof(payload));
			g_trackedLargeCallableGuard += (uintptr_t)this;
		}

		~TrackedLargeCallable() {
			g_trackedLargeCallableGuard ^= (uintptr_t)this;
		}

		int32_t operator()(int32_t lhs, int32_t rhs) const noexcept {
			return lhs + rhs + bias + payload[0];
		}
	};

	struct TrackedHeapCallable {
		int32_t bias = 0;
		uint8_t payload[1024]{};

		TrackedHeapCallable() = default;
		TrackedHeapCallable(int32_t value): bias(value) {}

		TrackedHeapCallable(const TrackedHeapCallable& other): bias(other.bias) {
			std::memcpy(payload, other.payload, sizeof(payload));
			g_trackedHeapCallableGuard += (uintptr_t)this;
		}

		~TrackedHeapCallable() {
			g_trackedHeapCallableGuard ^= (uintptr_t)this;
		}

		int32_t operator()(int32_t lhs, int32_t rhs) const noexcept {
			return lhs + rhs + bias + payload[0];
		}
	};

	struct SmallVoidCallable {
		uint32_t* pValue = nullptr;

		void operator()() const noexcept {
			++*pValue;
		}
	};

	struct LargeVoidCallable {
		uint32_t* pValue = nullptr;
		uint8_t payload[128]{};

		void operator()() const noexcept {
			*pValue += 1 + payload[0];
		}
	};

	struct HeapVoidCallable {
		uint32_t* pValue = nullptr;
		uint8_t payload[1024]{};

		void operator()() const noexcept {
			*pValue += 1 + payload[0];
		}
	};

	template <typename TFunc, typename TCallable>
	void run_invoke(picobench::state& state, TCallable callable) {
		TFunc func(callable);

		for (auto _: state) {
			(void)_;

			int32_t sum = 0;
			GAIA_FOR(InvokeOps) {
				sum += func((int32_t)i, 3);
			}

			gaia::dont_optimize(sum);
		}
	}

	template <typename TFunc>
	GAIA_NOINLINE int32_t invoke_erased_once(const TFunc& func, int32_t lhs, int32_t rhs) {
		return func(lhs, rhs);
	}

	template <typename TFunc, typename TCallable>
	void run_invoke_erased(picobench::state& state, TCallable callable) {
		TFunc func(callable);

		for (auto _: state) {
			(void)_;

			int32_t sum = 0;
			GAIA_FOR(InvokeOps) {
				sum += invoke_erased_once(func, (int32_t)i, 3);
			}

			gaia::dont_optimize(sum);
		}
	}

	template <typename TFunc, typename TCallable>
	void run_construct(picobench::state& state, TCallable callable) {
		for (auto _: state) {
			(void)_;

			int32_t sum = 0;
			GAIA_FOR(ConstructOps) {
				TFunc func(callable);
				gaia::dont_optimize(func);
				sum += func((int32_t)i, 2);
			}

			gaia::dont_optimize(sum);
		}
	}

	template <typename TCallable, bool UseMoveFuncStorage>
	void run_raw_box(picobench::state& state, TCallable callable) {
		for (auto _: state) {
			(void)_;

			int32_t sum = 0;
			GAIA_FOR(ConstructOps) {
				void* pStorage = nullptr;
				if constexpr (UseMoveFuncStorage && sizeof(TCallable) <= mem::SmallBlockMaxSize)
					pStorage = mem::SmallBlockAllocator::get().alloc((uint32_t)sizeof(TCallable));
				else
					pStorage = mem::AllocHelper::alloc<TCallable>();

				gaia::dont_optimize(pStorage);
				auto* pCallable = new (pStorage) TCallable(callable);
				gaia::dont_optimize(pCallable);
				sum += (*pCallable)((int32_t)i, 2);
				pCallable->~TCallable();

				if constexpr (UseMoveFuncStorage && sizeof(TCallable) <= mem::SmallBlockMaxSize)
					mem::SmallBlockAllocator::get().free(pCallable);
				else
					mem::AllocHelper::free(pCallable);
			}

			gaia::dont_optimize(sum);
		}
	}

	template <typename TCallable, bool UseSmallBlock>
	void warm_smallblock_storage() {
		if constexpr (UseSmallBlock && sizeof(TCallable) <= mem::SmallBlockMaxSize) {
			void* pStorage = mem::SmallBlockAllocator::get().alloc((uint32_t)sizeof(TCallable));
			gaia::dont_optimize(pStorage);
			mem::SmallBlockAllocator::get().free(pStorage);
		}
	}

	template <typename TCallable, bool UseSmallBlock>
	void run_raw_storage_tracked(picobench::state& state) {
		warm_smallblock_storage<TCallable, UseSmallBlock>();

		for (auto _: state) {
			(void)_;

			uintptr_t sum = 0;
			GAIA_FOR(ConstructOps) {
				void* pStorage = nullptr;
				if constexpr (UseSmallBlock && sizeof(TCallable) <= mem::SmallBlockMaxSize)
					pStorage = mem::SmallBlockAllocator::get().alloc((uint32_t)sizeof(TCallable));
				else
					pStorage = mem::AllocHelper::alloc<TCallable>();

				gaia::dont_optimize(pStorage);
				sum += (uintptr_t)pStorage >> 4;

				if constexpr (UseSmallBlock && sizeof(TCallable) <= mem::SmallBlockMaxSize)
					mem::SmallBlockAllocator::get().free(pStorage);
				else
					mem::AllocHelper::free(pStorage);
			}

			gaia::dont_optimize(sum);
		}
	}

	template <typename TCallable, bool UseSmallBlock>
	void run_raw_box_tracked(picobench::state& state, TCallable callable) {
		warm_smallblock_storage<TCallable, UseSmallBlock>();

		for (auto _: state) {
			(void)_;

			int32_t sum = 0;
			GAIA_FOR(ConstructOps) {
				void* pStorage = nullptr;
				if constexpr (UseSmallBlock && sizeof(TCallable) <= mem::SmallBlockMaxSize)
					pStorage = mem::SmallBlockAllocator::get().alloc((uint32_t)sizeof(TCallable));
				else
					pStorage = mem::AllocHelper::alloc<TCallable>();

				gaia::dont_optimize(pStorage);
				auto* pCallable = new (pStorage) TCallable(callable);
				gaia::dont_optimize(pCallable);
				sum += (*pCallable)((int32_t)i, 2);
				pCallable->~TCallable();

				if constexpr (UseSmallBlock && sizeof(TCallable) <= mem::SmallBlockMaxSize)
					mem::SmallBlockAllocator::get().free(pCallable);
				else
					mem::AllocHelper::free(pCallable);
			}

			gaia::dont_optimize(sum);
		}
	}

	template <typename TFunc, typename TCallable>
	void run_construct_only(picobench::state& state, TCallable callable) {
		for (auto _: state) {
			(void)_;

			uintptr_t sum = 0;
			GAIA_FOR(ConstructOps) {
				TFunc func(callable);
				gaia::dont_optimize(func);
				auto* pBytes = reinterpret_cast<const volatile uint8_t*>(&func);
				sum += pBytes[0];
			}

			gaia::dont_optimize(sum);
		}
	}

	template <typename TCallable, bool UseMoveFuncStorage>
	void run_raw_box_only(picobench::state& state, TCallable callable) {
		for (auto _: state) {
			(void)_;

			uintptr_t sum = 0;
			GAIA_FOR(ConstructOps) {
				void* pStorage = nullptr;
				if constexpr (UseMoveFuncStorage && sizeof(TCallable) <= mem::SmallBlockMaxSize)
					pStorage = mem::SmallBlockAllocator::get().alloc((uint32_t)sizeof(TCallable));
				else
					pStorage = mem::AllocHelper::alloc<TCallable>();

				gaia::dont_optimize(pStorage);
				auto* pCallable = new (pStorage) TCallable(callable);
				gaia::dont_optimize(pCallable);
				auto* pBytes = reinterpret_cast<const volatile uint8_t*>(pCallable);
				sum += pBytes[0];
				pCallable->~TCallable();

				if constexpr (UseMoveFuncStorage && sizeof(TCallable) <= mem::SmallBlockMaxSize)
					mem::SmallBlockAllocator::get().free(pCallable);
				else
					mem::AllocHelper::free(pCallable);
			}

			gaia::dont_optimize(sum);
		}
	}

	template <typename TFunc, typename TCallable>
	void run_void_construct(picobench::state& state, TCallable callable) {
		for (auto _: state) {
			(void)_;

			uint32_t value = 0;
			callable.pValue = &value;
			GAIA_FOR(ConstructOps) {
				TFunc func(callable);
				gaia::dont_optimize(func);
				func();
			}

			gaia::dont_optimize(value);
		}
	}
} // namespace

void BM_StdFunction_InvokeSmall(picobench::state& state) {
	run_invoke<std::function<int32_t(int32_t, int32_t)>>(state, SmallCallable{4});
}

void BM_MoveFunc_InvokeSmall(picobench::state& state) {
	run_invoke<util::MoveFunc<int32_t(int32_t, int32_t)>>(state, SmallCallable{4});
}

void BM_StdFunction_InvokeLarge(picobench::state& state) {
	run_invoke<std::function<int32_t(int32_t, int32_t)>>(state, LargeCallable{4, {}});
}

void BM_MoveFunc_InvokeLarge(picobench::state& state) {
	run_invoke<util::MoveFunc<int32_t(int32_t, int32_t)>>(state, LargeCallable{4, {}});
}

void BM_StdFunction_InvokeLargeErased(picobench::state& state) {
	run_invoke_erased<std::function<int32_t(int32_t, int32_t)>>(state, LargeCallable{4, {}});
}

void BM_MoveFunc_InvokeLargeErased(picobench::state& state) {
	run_invoke_erased<util::MoveFunc<int32_t(int32_t, int32_t)>>(state, LargeCallable{4, {}});
}

void BM_StdFunction_ConstructSmall(picobench::state& state) {
	run_construct<std::function<int32_t(int32_t, int32_t)>>(state, SmallCallable{4});
}

void BM_MoveFunc_ConstructSmall(picobench::state& state) {
	run_construct<util::MoveFunc<int32_t(int32_t, int32_t)>>(state, SmallCallable{4});
}

void BM_StdFunction_ConstructLarge(picobench::state& state) {
	run_construct<std::function<int32_t(int32_t, int32_t)>>(state, LargeCallable{4, {}});
}

void BM_MoveFunc_ConstructLarge(picobench::state& state) {
	run_construct<util::MoveFunc<int32_t(int32_t, int32_t)>>(state, LargeCallable{4, {}});
}

void BM_RawBox_ConstructLarge_DefaultHeap(picobench::state& state) {
	run_raw_box<LargeCallable, false>(state, LargeCallable{4, {}});
}

void BM_RawBox_ConstructLarge_MoveFuncStorage(picobench::state& state) {
	run_raw_box<LargeCallable, true>(state, LargeCallable{4, {}});
}

void BM_RawStorage_TrackedLarge_DefaultHeap(picobench::state& state) {
	run_raw_storage_tracked<TrackedLargeCallable, false>(state);
}

void BM_RawStorage_TrackedLarge_SmallBlock(picobench::state& state) {
	run_raw_storage_tracked<TrackedLargeCallable, true>(state);
}

void BM_RawBox_TrackedLarge_DefaultHeap(picobench::state& state) {
	run_raw_box_tracked<TrackedLargeCallable, false>(state, TrackedLargeCallable{4});
}

void BM_RawBox_TrackedLarge_SmallBlock(picobench::state& state) {
	run_raw_box_tracked<TrackedLargeCallable, true>(state, TrackedLargeCallable{4});
}

void BM_StdFunction_ConstructHeap(picobench::state& state) {
	run_construct<std::function<int32_t(int32_t, int32_t)>>(state, HeapCallable{4, {}});
}

void BM_MoveFunc_ConstructHeap(picobench::state& state) {
	run_construct<util::MoveFunc<int32_t(int32_t, int32_t)>>(state, HeapCallable{4, {}});
}

void BM_RawBox_ConstructHeap_DefaultHeap(picobench::state& state) {
	run_raw_box<HeapCallable, false>(state, HeapCallable{4, {}});
}

void BM_RawBox_ConstructHeap_MoveFuncStorage(picobench::state& state) {
	run_raw_box<HeapCallable, true>(state, HeapCallable{4, {}});
}

void BM_StdFunction_ConstructOnly_Large(picobench::state& state) {
	run_construct_only<std::function<int32_t(int32_t, int32_t)>>(state, LargeCallable{4, {}});
}

void BM_MoveFunc_ConstructOnly_Large(picobench::state& state) {
	run_construct_only<util::MoveFunc<int32_t(int32_t, int32_t)>>(state, LargeCallable{4, {}});
}

void BM_RawBox_ConstructOnly_Large_DefaultHeap(picobench::state& state) {
	run_raw_box_only<LargeCallable, false>(state, LargeCallable{4, {}});
}

void BM_StdFunction_ConstructOnly_Heap(picobench::state& state) {
	run_construct_only<std::function<int32_t(int32_t, int32_t)>>(state, TrackedHeapCallable{4});
}

void BM_MoveFunc_ConstructOnly_Heap(picobench::state& state) {
	run_construct_only<util::MoveFunc<int32_t(int32_t, int32_t)>>(state, TrackedHeapCallable{4});
}

void BM_RawBox_ConstructOnly_Heap_DefaultHeap(picobench::state& state) {
	run_raw_box_only<TrackedHeapCallable, false>(state, TrackedHeapCallable{4});
}

void BM_StdFunction_Construct_TrackedHeap(picobench::state& state) {
	run_construct<std::function<int32_t(int32_t, int32_t)>>(state, TrackedHeapCallable{4});
}

void BM_MoveFunc_Construct_TrackedHeap(picobench::state& state) {
	run_construct<util::MoveFunc<int32_t(int32_t, int32_t)>>(state, TrackedHeapCallable{4});
}

void BM_RawBox_Construct_TrackedHeap_DefaultHeap(picobench::state& state) {
	run_raw_box<TrackedHeapCallable, false>(state, TrackedHeapCallable{4});
}

void BM_StdFunction_ConstructSmallVoid(picobench::state& state) {
	run_void_construct<std::function<void()>>(state, SmallVoidCallable{});
}

void BM_SmallFunc_ConstructSmallVoid(picobench::state& state) {
	run_void_construct<util::SmallFunc>(state, SmallVoidCallable{});
}

void BM_StdFunction_ConstructLargeVoid(picobench::state& state) {
	run_void_construct<std::function<void()>>(state, LargeVoidCallable{});
}

void BM_SmallFunc_ConstructLargeVoid(picobench::state& state) {
	run_void_construct<util::SmallFunc>(state, LargeVoidCallable{});
}

void BM_StdFunction_ConstructHeapVoid(picobench::state& state) {
	run_void_construct<std::function<void()>>(state, HeapVoidCallable{});
}

void BM_SmallFunc_ConstructHeapVoid(picobench::state& state) {
	run_void_construct<util::SmallFunc>(state, HeapVoidCallable{});
}

////////////////////////////////////////////////////////////////////////////////

void register_funcs(PerfRunMode mode) {
	switch (mode) {
		case PerfRunMode::Profiling:
			PICOBENCH_SUITE_REG("Profile picks");
			PICOBENCH_REG(BM_StdFunction_InvokeSmall).PICO_SETTINGS_FOCUS().label("std::function invoke small");
			PICOBENCH_REG(BM_MoveFunc_InvokeSmall).PICO_SETTINGS_FOCUS().label("MoveFunc invoke small");
			PICOBENCH_REG(BM_StdFunction_ConstructSmall).PICO_SETTINGS_FOCUS().label("std::function construct small");
			PICOBENCH_REG(BM_MoveFunc_ConstructSmall).PICO_SETTINGS_FOCUS().label("MoveFunc construct small");
			return;
		case PerfRunMode::Sanitizer:
			PICOBENCH_SUITE_REG("Sanitizer picks");
			PICOBENCH_REG(BM_MoveFunc_InvokeSmall).PICO_SETTINGS_SANI().label("MoveFunc invoke small");
			PICOBENCH_REG(BM_MoveFunc_ConstructSmall).PICO_SETTINGS_SANI().label("MoveFunc construct small");
			return;
			case PerfRunMode::Normal:
				PICOBENCH_SUITE_REG("Function wrappers invoke");
				PICOBENCH_REG(BM_StdFunction_InvokeSmall).PICO_SETTINGS().label("std::function invoke small");
				PICOBENCH_REG(BM_MoveFunc_InvokeSmall).PICO_SETTINGS().label("MoveFunc invoke small");
			PICOBENCH_REG(BM_StdFunction_InvokeLarge).PICO_SETTINGS().label("std::function invoke large");
			PICOBENCH_REG(BM_MoveFunc_InvokeLarge).PICO_SETTINGS().label("MoveFunc invoke large");

				PICOBENCH_SUITE_REG("Function wrappers invoke erased");
				PICOBENCH_REG(BM_StdFunction_InvokeLargeErased).PICO_SETTINGS().label("std::function invoke large erased");
				PICOBENCH_REG(BM_MoveFunc_InvokeLargeErased).PICO_SETTINGS().label("MoveFunc invoke large erased");

				PICOBENCH_SUITE_REG("Function wrappers construct");
				PICOBENCH_REG(BM_StdFunction_ConstructSmall).PICO_SETTINGS().label("std::function construct small");
				PICOBENCH_REG(BM_MoveFunc_ConstructSmall).PICO_SETTINGS().label("MoveFunc construct small");
				PICOBENCH_REG(BM_StdFunction_ConstructLarge).PICO_SETTINGS().label("std::function construct large");
				PICOBENCH_REG(BM_MoveFunc_ConstructLarge).PICO_SETTINGS().label("MoveFunc construct large");

				PICOBENCH_SUITE_REG("Function wrapper boxing");
				PICOBENCH_REG(BM_RawBox_ConstructLarge_DefaultHeap).PICO_SETTINGS().label("Raw box construct large default heap");
				PICOBENCH_REG(BM_RawBox_ConstructLarge_MoveFuncStorage)
						.PICO_SETTINGS()
						.label("Raw box construct large MoveFunc storage");
				PICOBENCH_REG(BM_StdFunction_ConstructHeap).PICO_SETTINGS().label("std::function construct heap");
				PICOBENCH_REG(BM_MoveFunc_ConstructHeap).PICO_SETTINGS().label("MoveFunc construct heap");
				PICOBENCH_REG(BM_RawBox_ConstructHeap_DefaultHeap).PICO_SETTINGS().label("Raw box construct heap default heap");
				PICOBENCH_REG(BM_RawBox_ConstructHeap_MoveFuncStorage)
						.PICO_SETTINGS()
						.label("Raw box construct heap MoveFunc storage");

				PICOBENCH_SUITE_REG("Function wrapper boxing tracked");
				PICOBENCH_REG(BM_RawStorage_TrackedLarge_DefaultHeap)
						.PICO_SETTINGS()
						.label("Raw storage tracked large default heap");
				PICOBENCH_REG(BM_RawStorage_TrackedLarge_SmallBlock)
						.PICO_SETTINGS()
						.label("Raw storage tracked large smallblock");
				PICOBENCH_REG(BM_RawBox_TrackedLarge_DefaultHeap)
						.PICO_SETTINGS()
						.label("Raw box tracked large default heap");
				PICOBENCH_REG(BM_RawBox_TrackedLarge_SmallBlock)
						.PICO_SETTINGS()
						.label("Raw box tracked large smallblock");

				PICOBENCH_SUITE_REG("Function wrapper lifetime trivial");
				PICOBENCH_REG(BM_StdFunction_ConstructOnly_Large).PICO_SETTINGS().label("std::function construct-only large");
				PICOBENCH_REG(BM_MoveFunc_ConstructOnly_Large).PICO_SETTINGS().label("MoveFunc construct-only large");
				PICOBENCH_REG(BM_RawBox_ConstructOnly_Large_DefaultHeap)
						.PICO_SETTINGS()
						.label("Raw box construct-only large default heap");

				PICOBENCH_SUITE_REG("Function wrapper lifetime");
				PICOBENCH_REG(BM_StdFunction_ConstructOnly_Heap).PICO_SETTINGS().label("std::function construct-only heap");
				PICOBENCH_REG(BM_MoveFunc_ConstructOnly_Heap).PICO_SETTINGS().label("MoveFunc construct-only heap");
				PICOBENCH_REG(BM_RawBox_ConstructOnly_Heap_DefaultHeap)
						.PICO_SETTINGS()
						.label("Raw box construct-only heap default heap");

				PICOBENCH_SUITE_REG("Function wrapper tracked heap");
				PICOBENCH_REG(BM_StdFunction_Construct_TrackedHeap).PICO_SETTINGS().label("std::function construct tracked heap");
				PICOBENCH_REG(BM_MoveFunc_Construct_TrackedHeap).PICO_SETTINGS().label("MoveFunc construct tracked heap");
				PICOBENCH_REG(BM_RawBox_Construct_TrackedHeap_DefaultHeap)
						.PICO_SETTINGS()
						.label("Raw box construct tracked heap default heap");

				PICOBENCH_SUITE_REG("SmallFunc construct");
				PICOBENCH_REG(BM_StdFunction_ConstructSmallVoid).PICO_SETTINGS().label("std::function construct small void");
				PICOBENCH_REG(BM_SmallFunc_ConstructSmallVoid).PICO_SETTINGS().label("SmallFunc construct small void");
				PICOBENCH_REG(BM_StdFunction_ConstructLargeVoid).PICO_SETTINGS().label("std::function construct large void");
				PICOBENCH_REG(BM_SmallFunc_ConstructLargeVoid).PICO_SETTINGS().label("SmallFunc construct large void");
				PICOBENCH_REG(BM_StdFunction_ConstructHeapVoid).PICO_SETTINGS().label("std::function construct heap void");
				PICOBENCH_REG(BM_SmallFunc_ConstructHeapVoid).PICO_SETTINGS().label("SmallFunc construct heap void");
				return;
		}
	}

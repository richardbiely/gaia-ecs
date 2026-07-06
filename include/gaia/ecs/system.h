#pragma once
#include "gaia/config/config.h"

#include "gaia/cnt/map.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/query.h"
#include "gaia/util/move_func.h"

#if GAIA_SYSTEMS_ENABLED
namespace gaia {
	namespace ecs {
		//! Runtime payload for systems kept outside ECS component storage.
		//!
		//! System entities store their stable configuration in the ECS world, while the callable payload lives here so
		//! non-trivial function objects do not become component data. The registry owns the callable and clears it during
		//! teardown or deletion.
		//! \see SystemRegistry
		struct SystemRuntimeData {
			//! Execution path requested from a system runtime callback.
			enum class RunMode {
				//! Run the system immediately and return an empty scheduler job.
				Immediate,
				//! Prepare scheduler work and return it without submitting it.
				DeferredJob
			};

			//! Type-erased callback used for immediate and deferred system execution.
			//!
			//! The callback receives the system's underlying query, execution mode, and requested run mode. Immediate runs
			//! execute the stored query callback directly and return an empty SchedJob. Deferred runs add a
			//! scheduler-agnostic job wrapper from the same stored callback object. Per-system user context is stored on the
			//! query itself and is visible to iterator callbacks through Iter::ctx().
			//! \see QueryImpl::ctx(void*)
			//! \see QueryImpl::job(Func, QueryExecType)
			//! \see Iter::ctx() const
			using TSystemRunFunc = util::MoveFunc<SchedJob(Query&, QueryExecType, RunMode)>;

			//! Called when the system runs immediately or is added as deferred scheduler work.
			TSystemRunFunc on_each_func;
		};

		//! Runtime storage for system callbacks kept outside ECS component storage.
		//!
		//! The registry maps system entities to their callable runtime payload. It deliberately does not participate in
		//! normal component serialization: systems are expected to be rebuilt by setup code after loading a world.
		//! \see SystemRuntimeData
		class SystemRegistry {
			cnt::map<EntityLookupKey, SystemRuntimeData> m_system_data;

		public:
			//! Clears all registered system callbacks.
			//!
			//! Existing system entities/components are not removed from the world by this call. Only the callable
			//! payload outside ECS component storage is released.
			void teardown() {
				for (auto& it: m_system_data) {
					it.second.on_each_func = {};
				}

				m_system_data = {};
			}

			//! Creates or returns runtime data for a system entity.
			//! \param system Entity carrying the System_ component.
			//! \return Mutable runtime payload associated with @a system.
			SystemRuntimeData& data_add(Entity system) {
				return m_system_data[EntityLookupKey(system)];
			}

			//! Tries to find runtime data for a system entity.
			//! \param system Entity carrying the System_ component.
			//! \return Mutable runtime payload, or null when @a system has no registry entry.
			GAIA_NODISCARD SystemRuntimeData* data_try(Entity system) {
				const auto it = m_system_data.find(EntityLookupKey(system));
				if (it == m_system_data.end())
					return nullptr;
				return &it->second;
			}

			//! Tries to find runtime data for a system entity.
			//! \param system Entity carrying the System_ component.
			//! \return Immutable runtime payload, or null when @a system has no registry entry.
			GAIA_NODISCARD const SystemRuntimeData* data_try(Entity system) const {
				const auto it = m_system_data.find(EntityLookupKey(system));
				if (it == m_system_data.end())
					return nullptr;
				return &it->second;
			}

			//! Returns runtime data for a registered system entity.
			//! \param system Entity carrying the System_ component.
			//! \return Mutable runtime payload associated with @a system.
			//! \warning Asserts if @a system has no registry entry.
			GAIA_NODISCARD SystemRuntimeData& data(Entity system) {
				auto* pData = data_try(system);
				GAIA_ASSERT(pData != nullptr);
				return *pData;
			}

			//! Returns runtime data for a registered system entity.
			//! \param system Entity carrying the System_ component.
			//! \return Immutable runtime payload associated with @a system.
			//! \warning Asserts if @a system has no registry entry.
			GAIA_NODISCARD const SystemRuntimeData& data(Entity system) const {
				const auto* pData = data_try(system);
				GAIA_ASSERT(pData != nullptr);
				return *pData;
			}

			//! Deletes runtime data for a system entity.
			//! \param system Entity carrying the System_ component.
			//!
			//! Missing entries are ignored. The callback is cleared before erasing the map entry so captured data is released
			//! promptly.
			void del(Entity system) {
				const auto it = m_system_data.find(EntityLookupKey(system));
				if (it == m_system_data.end())
					return;

				it->second.on_each_func = {};
				m_system_data.erase(it);
			}
		};
	} // namespace ecs
} // namespace gaia
#endif

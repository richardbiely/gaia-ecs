#pragma once
#include "../config/config.h"

#include <cstdint>
#include <cstring>

#include "../config/profiler.h"
#if GAIA_DEBUG
	#include "../config/logging.h"
#endif
#include "../cnt/darray.h"
#include "../cnt/map.h"
#include "../core/hashing_policy.h"
#include "../core/utility.h"
#include "../meta/type_info.h"

namespace gaia {
	namespace ecs {
		class World;
		class BaseSystemManager;

#if GAIA_PROFILER_CPU
		constexpr uint32_t MaxSystemNameLength = 64;
#endif

		class BaseSystem {
			friend class BaseSystemManager;

			// A world this system belongs to
			World* m_world = nullptr;
#if GAIA_PROFILER_CPU
			//! System's name
			char m_name[MaxSystemNameLength]{};
#endif
			//! System's hash code
			uint64_t m_hash = 0;
			//! If true, the system is enabled and running
			bool m_enabled = true;
			//! If true, the system is to be destroyed
			bool m_destroy = false;

		protected:
			BaseSystem() = default;
			virtual ~BaseSystem() = default;

			BaseSystem(BaseSystem&&) = delete;
			BaseSystem(const BaseSystem&) = delete;
			BaseSystem& operator=(BaseSystem&&) = delete;
			BaseSystem& operator=(const BaseSystem&) = delete;

		public:
			GAIA_NODISCARD World& world() {
				return *m_world;
			}
			GAIA_NODISCARD const World& world() const {
				return *m_world;
			}

			//! Enable/disable system
			void enable(bool enable) {
				bool prev = m_enabled;
				m_enabled = enable;
				if (prev == enable)
					return;

				if (enable)
					OnStarted();
				else
					OnStopped();
			}

			//! Returns true if system is enabled
			GAIA_NODISCARD bool enabled() const {
				return m_enabled;
			}

		protected:
			//! Called when system is first created
			virtual void OnCreated() {}
			//! Called every time system is started (before the first run and after
			//! enable(true) is called
			virtual void OnStarted() {}

			//! Called right before every OnUpdate()
			virtual void BeforeOnUpdate() {}
			//! Called every time system is allowed to tick
			virtual void OnUpdate() {}
			//! Called aright after every OnUpdate()
			virtual void AfterOnUpdate() {}

			//! Called every time system is stopped (after enable(false) is called and
			//! before OnDestroyed when system is being destroyed
			virtual void OnStopped() {}
			//! Called when system are to be cleaned up.
			//! This always happens before OnDestroyed is called or at any point when
			//! simulation decides to bring the system back to the initial state
			//! without actually destroying it.
			virtual void OnCleanup() {}
			//! Called when system is being destroyed
			virtual void OnDestroyed() {}

			//! Returns true for systems this system depends on. False otherwise
			virtual bool DependsOn([[maybe_unused]] const BaseSystem* system) const {
				return false;
			}

		private:
			void set_destroyed(bool destroy) {
				m_destroy = destroy;
			}
			bool destroyed() const {
				return m_destroy;
			}
		};

		class BaseSystemManager {
		protected:
			using SystemHash = core::direct_hash_key<uint64_t>;

			World& m_world;
			//! Map of all systems - used for look-ups only
			cnt::map<SystemHash, BaseSystem*> m_systemsMap;
			//! List of systems - used for iteration
			cnt::darray<BaseSystem*> m_systems;
			//! List of new systems which need to be initialised
			cnt::darray<BaseSystem*> m_systemsToCreate;
			//! List of systems which need to be deleted
			cnt::darray<BaseSystem*> m_systemsToRemove;

		public:
			BaseSystemManager(World& world): m_world(world) {}
			virtual ~BaseSystemManager() {
				clear();
			}

			BaseSystemManager(BaseSystemManager&& world) = delete;
			BaseSystemManager(const BaseSystemManager& world) = delete;
			BaseSystemManager& operator=(BaseSystemManager&&) = delete;
			BaseSystemManager& operator=(const BaseSystemManager&) = delete;

			void clear() {
				for (auto* pSystem: m_systems)
					pSystem->enable(false);
				for (auto* pSystem: m_systems)
					pSystem->OnCleanup();
				for (auto* pSystem: m_systems)
					pSystem->OnDestroyed();
				for (auto* pSystem: m_systems)
					delete pSystem;

				m_systems.clear();
				m_systemsMap.clear();

				m_systemsToCreate.clear();
				m_systemsToRemove.clear();
			}

			void cleanup() {
				for (auto& s: m_systems)
					s->OnCleanup();
			}

			void update() {
				// Remove all systems queued to be destroyed
				for (auto* pSystem: m_systemsToRemove)
					pSystem->enable(false);
				for (auto* pSystem: m_systemsToRemove)
					pSystem->OnCleanup();
				for (auto* pSystem: m_systemsToRemove)
					pSystem->OnDestroyed();
				for (auto* pSystem: m_systemsToRemove)
					m_systemsMap.erase({pSystem->m_hash});
				for (auto* pSystem: m_systemsToRemove) {
					m_systems.erase(core::find(m_systems, pSystem));
				}
				for (auto* pSystem: m_systemsToRemove)
					delete pSystem;
				m_systemsToRemove.clear();

				if GAIA_UNLIKELY (!m_systemsToCreate.empty()) {
					// Sort systems if necessary
					sort();

					// Create all new systems
					for (auto* pSystem: m_systemsToCreate) {
						pSystem->OnCreated();
						if (pSystem->enabled())
							pSystem->OnStarted();
					}
					m_systemsToCreate.clear();
				}

				OnBeforeUpdate();

				for (auto* pSystem: m_systems) {
					if (!pSystem->enabled())
						continue;

					{
						GAIA_PROF_SCOPE2(&pSystem->m_name[0]);
						pSystem->BeforeOnUpdate();
						pSystem->OnUpdate();
						pSystem->AfterOnUpdate();
					}
				}

				OnAfterUpdate();
			}

			template <typename T>
			T* add([[maybe_unused]] const char* name = nullptr) {
				GAIA_SAFE_CONSTEXPR auto hash = meta::type_info::hash<std::decay_t<T>>();

				const auto res = m_systemsMap.try_emplace({hash}, nullptr);
				if GAIA_UNLIKELY (!res.second)
					return (T*)res.first->second;

				BaseSystem* pSystem = new T();
				pSystem->m_world = &m_world;

#if GAIA_PROFILER_CPU
				if (name == nullptr) {
					constexpr auto ct_name = meta::type_info::name<T>();
					const size_t len = ct_name.size() >= MaxSystemNameLength ? MaxSystemNameLength : ct_name.size() + 1;
					GAIA_SETSTR(pSystem->m_name, ct_name.data(), len);
				} else {
					GAIA_SETSTR(pSystem->m_name, name, MaxSystemNameLength);
				}
#endif

				pSystem->m_hash = hash;
				res.first->second = pSystem;

				m_systems.push_back(pSystem);
				// Request initialization of the pSystem
				m_systemsToCreate.push_back(pSystem);

				return (T*)pSystem;
			}

			template <typename T>
			void del() {
				auto pSystem = find<T>();
				if (pSystem == nullptr || pSystem->destroyed())
					return;

				pSystem->set_destroyed(true);

				// Request removal of the system
				m_systemsToRemove.push_back(pSystem);
			}

			template <typename T>
			GAIA_NODISCARD T* find() {
				GAIA_SAFE_CONSTEXPR auto hash = meta::type_info::hash<std::decay_t<T>>();

				const auto it = m_systemsMap.find({hash});
				if (it != m_systemsMap.end())
					return (T*)it->second;

				return (T*)nullptr;
			}

		protected:
			virtual void OnBeforeUpdate() {}
			virtual void OnAfterUpdate() {}

		private:
			void sort() {
				GAIA_FOR_(m_systems.size() - 1, l) {
					auto min = l;
					GAIA_FOR2_(l + 1, m_systems.size(), p) {
						const auto* sl = m_systems[l];
						const auto* pl = m_systems[p];
						if (sl->DependsOn(pl))
							min = p;
					}

					auto* tmp = m_systems[min];
					m_systems[min] = m_systems[l];
					m_systems[l] = tmp;
				}

#if GAIA_DEBUG
				// Make sure there are no circular dependencies
				GAIA_FOR2_(1U, m_systems.size(), j) {
					if (!m_systems[j - 1]->DependsOn(m_systems[j]))
						continue;
					GAIA_ASSERT2(false, "Wrong systems dependencies!");
					GAIA_LOG_E("Wrong systems dependencies!");
				}
#endif
			}
		};

	} // namespace ecs
} // namespace gaia

#pragma once
#include <cinttypes>
#include <cstring>

#include "../config/config.h"
#if GAIA_DEBUG
	#include "../config/logging.h"
#endif
#include "../containers/darray.h"
#include "../containers/map.h"
#include "../utils/containers.h"
#include "../utils/hashing_policy.h"
#include "../utils/type_info.h"

namespace gaia {
	namespace ecs {
		class World;
		class BaseSystemManager;

		constexpr size_t MaxSystemNameLength = 64;

		class BaseSystem {
			friend class BaseSystemManager;

			// A world this system belongs to
			World* m_world = nullptr;
			//! System's name
			char m_name[MaxSystemNameLength]{};
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
			GAIA_NODISCARD World& GetWorld() {
				return *m_world;
			}
			GAIA_NODISCARD const World& GetWorld() const {
				return *m_world;
			}

			//! Enable/disable system
			void Enable(bool enable) {
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
			GAIA_NODISCARD bool IsEnabled() const {
				return m_enabled;
			}

		protected:
			//! Called when system is first created
			virtual void OnCreated() {}
			//! Called every time system is started (before the first run and after
			//! Enable(true) is called
			virtual void OnStarted() {}

			//! Called right before every OnUpdate()
			virtual void BeforeOnUpdate() {}
			//! Called every time system is allowed to tick
			virtual void OnUpdate() {}
			//! Called aright after every OnUpdate()
			virtual void AfterOnUpdate() {}

			//! Called every time system is stopped (after Enable(false) is called and
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
			void SetDestroyed(bool destroy) {
				m_destroy = destroy;
			}
			bool IsDestroyed() const {
				return m_destroy;
			}
		};

		class BaseSystemManager {
		protected:
			World& m_world;
			//! Map of all systems - used for look-ups only
			containers::map<utils::direct_hash_key, BaseSystem*> m_systemsMap;
			//! List of system - used for iteration
			containers::darray<BaseSystem*> m_systems;
			//! List of new systems which need to be initialised
			containers::darray<BaseSystem*> m_systemsToCreate;
			//! List of systems which need to be deleted
			containers::darray<BaseSystem*> m_systemsToRemove;

		public:
			BaseSystemManager(World& world): m_world(world) {}
			~BaseSystemManager() {
				Clear();
			}

			BaseSystemManager(BaseSystemManager&& world) = delete;
			BaseSystemManager(const BaseSystemManager& world) = delete;
			BaseSystemManager& operator=(BaseSystemManager&&) = delete;
			BaseSystemManager& operator=(const BaseSystemManager&) = delete;

			void Clear() {
				for (auto* pSystem: m_systems)
					pSystem->Enable(false);
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

			void Cleanup() {
				for (auto& s: m_systems)
					s->OnCleanup();
			}

			void Update() {
				// Remove all systems queued to be destroyed
				for (auto* pSystem: m_systemsToRemove)
					pSystem->Enable(false);
				for (auto* pSystem: m_systemsToRemove)
					pSystem->OnCleanup();
				for (auto* pSystem: m_systemsToRemove)
					pSystem->OnDestroyed();
				for (auto* pSystem: m_systemsToRemove)
					m_systemsMap.erase({pSystem->m_hash});
				for (auto* pSystem: m_systemsToRemove) {
					m_systems.erase(utils::find(m_systems, pSystem));
				}
				for (auto* pSystem: m_systemsToRemove)
					delete pSystem;
				m_systemsToRemove.clear();

				if GAIA_UNLIKELY (!m_systemsToCreate.empty()) {
					// Sort systems if necessary
					SortSystems();

					// Create all new systems
					for (auto* pSystem: m_systemsToCreate) {
						pSystem->OnCreated();
						if (pSystem->IsEnabled())
							pSystem->OnStarted();
					}
					m_systemsToCreate.clear();
				}

				OnBeforeUpdate();

				for (auto* pSystem: m_systems) {
					if (!pSystem->IsEnabled())
						continue;

						pSystem->BeforeOnUpdate();
						pSystem->OnUpdate();
						pSystem->AfterOnUpdate();
					}

				OnAfterUpdate();
			}

			template <typename T>
			T* CreateSystem(const char* name) {
				GAIA_SAFE_CONSTEXPR auto hash = utils::type_info::hash<std::decay_t<T>>();

				const auto res = m_systemsMap.emplace(utils::direct_hash_key{hash}, nullptr);
				if GAIA_UNLIKELY (!res.second)
					return (T*)res.first->second;

				BaseSystem* pSystem = new T();
				pSystem->m_world = &m_world;

#if GAIA_COMPILER_MSVC || defined(_WIN32)
				strncpy_s(pSystem->m_name, name, (size_t)-1);
#else
				strncpy(pSystem->m_name, name, MaxSystemNameLength - 1);
#endif
				pSystem->m_name[MaxSystemNameLength - 1] = 0;

				pSystem->m_hash = hash;
				res.first->second = pSystem;

				m_systems.push_back(pSystem);
				// Request initialization of the pSystem
				m_systemsToCreate.push_back(pSystem);

				return (T*)pSystem;
			}

			template <typename T>
			void RemoveSystem() {
				auto pSystem = FindSystem<T>();
				if (pSystem == nullptr || pSystem->IsDestroyed())
					return;

				pSystem->SetDestroyed(true);

				// Request removal of the system
				m_systemsToRemove.push_back(pSystem);
			}

			template <typename T>
			GAIA_NODISCARD T* FindSystem() {
				GAIA_SAFE_CONSTEXPR auto hash = utils::type_info::hash<std::decay_t<T>>();

				const auto it = m_systemsMap.find({hash});
				if (it != m_systemsMap.end())
					return (T*)it->second;

				return (T*)nullptr;
			}

		protected:
			virtual void OnBeforeUpdate() {}
			virtual void OnAfterUpdate() {}

		private:
			void SortSystems() {
				for (size_t l = 0; l < m_systems.size() - 1; l++) {
					auto min = l;
					for (size_t p = l + 1; p < m_systems.size(); p++) {
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
				for (auto j = 1U; j < m_systems.size(); j++) {
					if (!m_systems[j - 1]->DependsOn(m_systems[j]))
						continue;
					GAIA_ASSERT(false && "Wrong systems dependencies!");
					LOG_E("Wrong systems dependencies!");
				}
#endif
			}
		};

	} // namespace ecs
} // namespace gaia

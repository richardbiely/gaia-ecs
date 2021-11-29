#pragma once
#include <cinttypes>
#include <string>

#include "../config/config.h"
#if GAIA_DEBUG
	#include "../config/logging.h"
#endif
#include "../utils/map.h"
#include "../utils/type_info.h"
#include "../utils/utils_containers.h"
#include "../utils/vector.h"

namespace gaia {
	namespace ecs {
		class World;
		class BaseSystemManager;

		class BaseSystem {
			friend class BaseSystemManager;

			// A world this system belongs to
			World* m_world = nullptr;
			//! System's name
			std::string m_name;
			//! System's hash code
			uint64_t m_hash = 0;
			//! If true, the system is enabled and running
			bool m_enabled = true;
			//! If true, the system is to be destroyed
			[[maybe_unused]] bool m_destroy = false;

		public:
			BaseSystem() = default;
			virtual ~BaseSystem() = default;

			BaseSystem(BaseSystem&&) = delete;
			BaseSystem(const BaseSystem&) = delete;
			BaseSystem& operator=(BaseSystem&&) = delete;
			BaseSystem& operator=(const BaseSystem&) = delete;

			[[nodiscard]] World& GetWorld() {
				return *m_world;
			}
			[[nodiscard]] const World& GetWorld() const {
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
			[[nodiscard]] bool IsEnabled() const {
				return m_enabled;
			}

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
		};

		class BaseSystemManager {
		protected:
			World& m_world;
			//! Map of all systems - used for look-ups only
			utils::map<uint64_t, BaseSystem*> m_systemsMap;
			//! List of system - used for iteration
			utils::darray<BaseSystem*> m_systems;
			//! List of new systems which need to be initialised
			utils::darray<BaseSystem*> m_systemsToCreate;
			//! List of systems which need to be deleted
			utils::darray<BaseSystem*> m_systemsToRemove;

		public:
			BaseSystemManager(World& world): m_world(world) {}
			~BaseSystemManager() {
				Clear();
			}

			void Clear() {
				for (auto system: m_systems)
					system->Enable(false);
				for (auto system: m_systems)
					system->OnCleanup();
				for (auto system: m_systems)
					system->OnDestroyed();
				for (auto system: m_systems)
					delete system;

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
				for (auto system: m_systemsToRemove)
					system->Enable(false);
				for (auto system: m_systemsToRemove)
					system->OnCleanup();
				for (auto system: m_systemsToRemove)
					system->OnDestroyed();
				for (auto system: m_systemsToRemove)
					m_systemsMap.erase(system->m_hash);
				for (auto system: m_systemsToRemove) {
					m_systems.erase(utils::find(m_systems, system));
				}
				for (auto system: m_systemsToRemove)
					delete system;
				m_systemsToRemove.clear();

				if (!m_systemsToCreate.empty()) {
					// Sort systems if necessary
					SortSystems();

					// Create all new systems
					for (auto system: m_systemsToCreate) {
						system->OnCreated();
						if (system->IsEnabled())
							system->OnStarted();
					}
					m_systemsToCreate.clear();
				}

				OnAfterUpdate();

				for (auto system: m_systems) {
					if (!system->IsEnabled())
						continue;

					system->BeforeOnUpdate();
					system->OnUpdate();
					system->AfterOnUpdate();
				}

				OnAfterUpdate();
			}

			template <typename T>
			T* CreateSystem(const char* name) {
				constexpr uint64_t hash = utils::type_info::hash<std::decay_t<T>>();

				const auto res = m_systemsMap.emplace(hash, nullptr);
				if (!res.second)
					return (T*)res.first->second;

				BaseSystem* system = new T();
				system->m_world = &m_world;
				system->m_name = name;
				system->m_hash = hash;
				res.first->second = system;

				m_systems.push_back(system);
				// Request initialization of the system
				m_systemsToCreate.push_back(system);

				return (T*)system;
			}

			template <typename T>
			void RemoveSystem() {
				auto system = FindSystem<T>();
				if (system == nullptr || system->m_destroy)
					return;

				system->m_destroy = true;

				// Request removal of the system
				m_systemsToRemove.push_back(system);
			}

			template <typename T>
			[[nodiscard]] T* FindSystem() {
				constexpr auto hash = utils::type_info::hash<std::decay_t<T>>();

				const auto it = m_systemsMap.find(hash);
				if (it != m_systemsMap.end())
					return (T*)it->second;

				return (T*)nullptr;
			}

		protected:
			virtual void OnBeforeUpdate() {}
			virtual void OnAfterUpdate() {}

		private:
			void SortSystems() {
				for (auto l = 0U; l < m_systems.size() - 1; l++) {
					auto min = l;
					for (auto p = l + 1; p < m_systems.size(); p++) {
						const auto* sl = m_systems[l];
						const auto* pl = m_systems[p];
						if (sl->DependsOn(pl))
							min = p;
					}

					auto tmp = m_systems[min];
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
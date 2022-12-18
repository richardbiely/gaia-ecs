#pragma once
#include <cinttypes>

#include "command_buffer.h"
#include "system_base.h"
#include "world.h"

namespace gaia {
	namespace ecs {
		class System: public BaseSystem {
			friend class World;
			friend struct ExecutionContextBase;

			//! Last version of the world the system was updated.
			uint32_t m_lastSystemVersion = 0;

		private:
			void BeforeOnUpdate() final {
				GetWorld().UpdateWorldVersion();
			}
			void AfterOnUpdate() final {
				m_lastSystemVersion = GetWorld().GetWorldVersion();
			}

		public:
			//! Returns the world version when the system was updated
			GAIA_NODISCARD uint32_t GetLastSystemVersion() const {
				return m_lastSystemVersion;
			}
		};

		class SystemManager final: public BaseSystemManager {
			CommandBuffer m_beforeUpdateCmdBuffer;
			CommandBuffer m_afterUpdateCmdBuffer;

		public:
			SystemManager(World& world):
					BaseSystemManager(world), m_beforeUpdateCmdBuffer(world), m_afterUpdateCmdBuffer(world) {}

			CommandBuffer& BeforeUpdateCmdBufer() {
				return m_beforeUpdateCmdBuffer;
			}
			CommandBuffer& AfterUpdateCmdBufer() {
				return m_afterUpdateCmdBuffer;
			}

		protected:
			void OnBeforeUpdate() final {
				m_beforeUpdateCmdBuffer.Commit();
			}

			void OnAfterUpdate() final {
				m_afterUpdateCmdBuffer.Commit();
				m_world.GC();
			}
		};
	} // namespace ecs
} // namespace gaia

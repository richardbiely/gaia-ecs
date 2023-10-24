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
				m_beforeUpdateCmdBuffer.commit();
			}

			void OnAfterUpdate() final {
				m_afterUpdateCmdBuffer.commit();
			}
		};
	} // namespace ecs
} // namespace gaia

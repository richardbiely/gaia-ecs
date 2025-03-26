#pragma once
#include <cstdint>

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
			CommandBufferST m_beforeUpdateCmdBuffer;
			CommandBufferST m_afterUpdateCmdBuffer;

		public:
			SystemManager(World& world):
					BaseSystemManager(world), m_beforeUpdateCmdBuffer(world), m_afterUpdateCmdBuffer(world) {}

			CommandBufferST& BeforeUpdateCmdBuffer() {
				return m_beforeUpdateCmdBuffer;
			}
			CommandBufferST& AfterUpdateCmdBuffer() {
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

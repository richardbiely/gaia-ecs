#pragma once

namespace gaia {
	namespace ecs {
		class World;
		class CommandBuffer;

		CommandBuffer* cmd_buffer_create(World& world);
		void cmd_buffer_destroy(CommandBuffer& cmdBuffer);
		void cmd_buffer_commit(CommandBuffer& cmdBuffer);
	} // namespace ecs
} // namespace gaia

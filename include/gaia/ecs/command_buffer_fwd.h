#pragma once

namespace gaia {
	namespace ecs {
		class World;

		struct AccessContextST;
		struct AccessContextMT;

		template <typename AccessContext>
		class CommandBuffer;

		using CommandBufferST = CommandBuffer<AccessContextST>;
		using CommandBufferMT = CommandBuffer<AccessContextMT>;

		CommandBufferST* cmd_buffer_st_create(World& world);
		CommandBufferMT* cmd_buffer_mt_create(World& world);
		void cmd_buffer_destroy(CommandBufferST& cmdBuffer);
		void cmd_buffer_commit(CommandBufferST& cmdBuffer);
		void cmd_buffer_destroy(CommandBufferMT& cmdBuffer);
		void cmd_buffer_commit(CommandBufferMT& cmdBuffer);
	} // namespace ecs
} // namespace gaia

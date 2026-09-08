#pragma once

namespace gaia {
	namespace ecs {
		class World;

		namespace detail {
			template <typename AccessContext, bool Optimize = true>
			class CommandBuffer;
		}
		struct AccessContextST;
		struct AccessContextMT;
		using CommandBufferST = detail::CommandBuffer<AccessContextST>;
		using CommandBufferMT = detail::CommandBuffer<AccessContextMT>;
		using CommandBufferPlainST = detail::CommandBuffer<AccessContextST, false>;
		using CommandBufferPlainMT = detail::CommandBuffer<AccessContextMT, false>;

		CommandBufferST* cmd_buffer_st_create(World& world);
		void cmd_buffer_destroy(CommandBufferST& cmdBuffer);
		void cmd_buffer_commit(CommandBufferST& cmdBuffer);

		CommandBufferMT* cmd_buffer_mt_create(World& world);
		void cmd_buffer_destroy(CommandBufferMT& cmdBuffer);
		void cmd_buffer_commit(CommandBufferMT& cmdBuffer);
	} // namespace ecs
} // namespace gaia

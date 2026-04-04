#pragma once

namespace gaia {
	namespace ecs {
		struct TypedQueryExecState {
			uint32_t argCount = 0;
			Entity argIds[MAX_ITEMS_IN_QUERY]{};
			bool writeFlags[MAX_ITEMS_IN_QUERY]{};
			bool hasWriteArgs = false;
			bool needsInheritedArgIds = false;
			bool canUseDirectChunkEval = false;
			bool hasInheritedTerms = false;
		};

		struct TypedQueryArgMeta {
			Entity termId = EntityBad;
			bool isWrite = false;
			bool isEntity = false;
			bool isPair = false;
		};

		template <typename T>
		GAIA_NODISCARD inline TypedQueryArgMeta typed_query_arg_meta(World& world) {
			using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
			const bool isWrite =
					std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>> && !std::is_same_v<Arg, Entity>;
			if constexpr (std::is_same_v<Arg, Entity>)
				return {.termId = EntityBad, .isWrite = isWrite, .isEntity = true, .isPair = false};
			else {
				using FT = typename component_type_t<Arg>::TypeFull;
				if constexpr (is_pair<FT>::value)
					return {.termId = EntityBad, .isWrite = isWrite, .isEntity = false, .isPair = true};
				else
					return {.termId = world_query_arg_id<Arg>(world), .isWrite = isWrite, .isEntity = false, .isPair = false};
			}
		}

		template <typename... T>
		inline uint32_t
		init_typed_query_arg_metas(TypedQueryArgMeta* pMetas, World& world, [[maybe_unused]] core::func_type_list<T...>) {
			if constexpr (sizeof...(T) > 0) {
				const TypedQueryArgMeta descs[] = {typed_query_arg_meta<T>(world)...};
				GAIA_FOR(sizeof...(T)) {
					pMetas[i] = descs[i];
				}
			}
			return (uint32_t)sizeof...(T);
		}

#if GAIA_ASSERT_ENABLED
		template <typename... T>
		GAIA_NODISCARD inline bool
		typed_query_args_match_query(const QueryInfo& queryInfo, [[maybe_unused]] core::func_type_list<T...>) {
			if constexpr (sizeof...(T) > 0)
				return queryInfo.template has_all<T...>();
			else
				return true;
		}
#endif

		template <typename... T, typename Func, size_t... I>
		inline void invoke_typed_query_args_by_id(
				World& world, Entity entity, const Entity* pArgIds, Func& func, std::index_sequence<I...>) {
			func(([&]() -> decltype(auto) {
				using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
				if constexpr (std::is_same_v<Arg, Entity>)
					return entity;
				else if constexpr (std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>)
					return world_query_entity_arg_by_id_raw<T>(world, entity, pArgIds[I]);
				else
					return world_query_entity_arg_by_id<T>(world, entity, pArgIds[I]);
			}())...);
		}

		template <typename Func, typename... T>
		inline void invoke_typed_query_args_by_id_erased(World& world, Entity entity, const Entity* pArgIds, void* pFunc) {
			auto& func = *static_cast<Func*>(pFunc);
			invoke_typed_query_args_by_id<T...>(world, entity, pArgIds, func, std::index_sequence_for<T...>{});
		}

		template <typename Func, typename... T>
		GAIA_NODISCARD inline auto typed_invoke_inherited_ptr(core::func_type_list<T...>) {
			return &invoke_typed_query_args_by_id_erased<Func, T...>;
		}

		inline void finish_typed_query_args_by_id(
				World& world, Entity entity, const Entity* pArgIds, const bool* pWriteFlags, uint32_t argCnt) {
			Entity seenTerms[ChunkHeader::MAX_COMPONENTS]{};
			uint32_t seenCnt = 0;
			const auto finish_term = [&](Entity term) {
				GAIA_FOR(seenCnt) {
					if (seenTerms[i] == term)
						return;
				}

				seenTerms[seenCnt++] = term;
				world_finish_write(world, term, entity);
			};

			GAIA_FOR(argCnt) {
				if (pWriteFlags[i])
					finish_term(pArgIds[i]);
			}
		}
	} // namespace ecs
} // namespace gaia

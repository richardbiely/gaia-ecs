#pragma once

namespace gaia {
	namespace ecs {
		//! \cond INTERNAL
		void world_notify_on_set_entity(World& world, Entity term, Entity entity);

		template <typename T>
		void* world_typed_sparse_store_ptr(World& world, Entity component);
		template <typename T>
		const T& world_typed_sparse_store_get(const void* pStore, Entity entity);
		template <typename T>
		T& world_typed_sparse_store_mut(void* pStore, Entity entity);
		template <typename T>
		bool world_typed_sparse_store_has(const void* pStore, Entity entity);

		template <typename T, bool IsEntity = std::is_same_v<typename actual_type_t<T>::Type, Entity>>
		struct typed_query_arg_uses_sparse_storage: std::false_type {};

		template <typename T>
		struct typed_query_arg_uses_sparse_storage<T, false>:
				std::bool_constant<auto_storage_policy_v<typename actual_type_t<T>::Type> == DataStorageType::Sparse> {};

		template <typename... T>
		inline constexpr bool typed_query_args_use_sparse_storage_v =
				(typed_query_arg_uses_sparse_storage<T>::value || ...);

		template <typename T>
		struct typed_query_arg_list_uses_sparse_storage;

		template <typename... T>
		struct typed_query_arg_list_uses_sparse_storage<core::func_type_list<T...>>:
				std::bool_constant<typed_query_args_use_sparse_storage_v<T...>> {};

		template <typename T>
		inline constexpr bool typed_query_arg_list_uses_sparse_storage_v =
				typed_query_arg_list_uses_sparse_storage<T>::value;

		struct TypedQueryExecState {
			uint32_t argCount = 0;
			Entity argIds[MAX_ITEMS_IN_QUERY]{};
			bool writeFlags[MAX_ITEMS_IN_QUERY]{};
			void* sparseStores[MAX_ITEMS_IN_QUERY]{};
			uint32_t firstWriteArg = MAX_ITEMS_IN_QUERY;
			bool hasWriteArgs = false;
			bool needsInheritedArgIds = false;
			bool canUseDirectChunkEval = false;
			bool canUseSparseChunkEval = false;
			bool hasInheritedTerms = false;
		};

		struct TypedQueryArgMeta {
			Entity termId = EntityBad;
			bool isWrite = false;
			bool isEntity = false;
			bool isPair = false;
			bool usesSparseStorage = false;
		};

		template <typename T>
		GAIA_NODISCARD inline TypedQueryArgMeta typed_query_arg_meta(World& world) {
			using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
			const bool isWrite =
					std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>> && !std::is_same_v<Arg, Entity>;
			if constexpr (std::is_same_v<Arg, Entity>)
				return TypedQueryArgMeta{EntityBad, isWrite, true, false, false};
			else {
				using FT = typename component_type_t<Arg>::TypeFull;
				if constexpr (is_pair<FT>::value)
					return TypedQueryArgMeta{EntityBad, isWrite, false, true, false};
				else {
					const auto termId = world_query_arg_id<Arg>(world);
					constexpr bool UsesSparseStorage =
							auto_storage_policy_v<typename actual_type_t<Arg>::Type> == DataStorageType::Sparse;
					return TypedQueryArgMeta{termId, isWrite, false, false, UsesSparseStorage};
				}
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

		template <typename... T, size_t... I>
		inline void bind_typed_sparse_stores(
				TypedQueryExecState& state, World& world, core::func_type_list<T...>, std::index_sequence<I...>) {
			(([&]() {
				 using U = typename actual_type_t<T>::Type;
				 if constexpr (!std::is_same_v<U, Entity> && auto_storage_policy_v<U> == DataStorageType::Sparse)
					 state.sparseStores[I] = world_typed_sparse_store_ptr<U>(world, state.argIds[I]);
			 }()),
			 ...);
		}

		template <typename... T>
		inline void bind_typed_sparse_stores(TypedQueryExecState& state, World& world, core::func_type_list<T...> types) {
			bind_typed_sparse_stores(state, world, types, std::index_sequence_for<T...>{});
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

		inline void finish_typed_query_args_by_id(World& world, Entity entity, const TypedQueryExecState& state) {
			if (state.firstWriteArg >= state.argCount)
				return;

			Entity seenTerms[ChunkHeader::MAX_COMPONENTS]{};
			uint32_t seenCnt = 0;
			const auto finish_term = [&](Entity term, bool sparseStoreBound) {
				GAIA_FOR(seenCnt) {
					if (seenTerms[i] == term)
						return;
				}

				seenTerms[seenCnt++] = term;
				if (sparseStoreBound)
					world_notify_on_set_entity(world, term, entity);
				else
					world_finish_write(world, term, entity);
			};

			for (uint32_t i = state.firstWriteArg; i < state.argCount; ++i) {
				if (state.writeFlags[i] && state.argIds[i] != EntityBad)
					finish_term(state.argIds[i], state.sparseStores[i] != nullptr);
			}
		}
		//! \endcond
	} // namespace ecs
} // namespace gaia

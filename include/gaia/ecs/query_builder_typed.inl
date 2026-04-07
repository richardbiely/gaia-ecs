#pragma once

namespace gaia {
	namespace ecs {
		namespace detail {
			template <typename T>
			GAIA_NODISCARD inline Entity typed_query_raw_entity(World& world) {
				using U = typename component_type_t<T>::Type;
				static_assert(core::is_raw_v<U>, "Use raw types only");

				const auto& desc = comp_cache_add<T>(world);
				return desc.entity;
			}

			template <typename Rel, typename Tgt>
			GAIA_NODISCARD inline Entity typed_query_pair_entity(World& world) {
				using URel = typename component_type_t<Rel>::Type;
				using UTgt = typename component_type_t<Tgt>::Type;
				static_assert(core::is_raw_v<URel>, "Use raw relation types only");
				static_assert(core::is_raw_v<UTgt>, "Use raw target types only");

				const auto& descRel = comp_cache_add<Rel>(world);
				const auto& descTgt = comp_cache_add<Tgt>(world);
				return Pair(descRel.entity, descTgt.entity);
			}

			template <typename T>
			GAIA_NODISCARD inline Entity typed_query_term_entity(World& world) {
				using FT = typename component_type_t<T>::TypeFull;
				if constexpr (is_pair<FT>::value)
					return typed_query_pair_entity<typename FT::rel_type, typename FT::tgt_type>(world);
				else
					return typed_query_raw_entity<T>(world);
			}

			template <typename T>
			GAIA_NODISCARD inline QueryTermOptions typed_query_term_options(QueryOpKind op, QueryTermOptions options) {
				if (op != QueryOpKind::Not && op != QueryOpKind::Any && options.access == QueryAccess::None) {
					constexpr auto isReadWrite = core::is_mut_v<T>;
					options.access = isReadWrite ? QueryAccess::Write : QueryAccess::Read;
				}
				return options;
			}

			template <typename T>
			inline QueryImpl& QueryImpl::all(const QueryTermOptions& options) {
				return all(
						typed_query_term_entity<T>(*m_storage.world()), typed_query_term_options<T>(QueryOpKind::All, options));
			}

			template <typename T>
			inline QueryImpl& QueryImpl::all() {
				return all<T>(QueryTermOptions{});
			}

			template <typename T>
			inline QueryImpl& QueryImpl::any(const QueryTermOptions& options) {
				return any(
						typed_query_term_entity<T>(*m_storage.world()), typed_query_term_options<T>(QueryOpKind::Any, options));
			}

			template <typename T>
			inline QueryImpl& QueryImpl::any() {
				return any<T>(QueryTermOptions{});
			}

			template <typename T>
			inline QueryImpl& QueryImpl::or_(const QueryTermOptions& options) {
				return or_(
						typed_query_term_entity<T>(*m_storage.world()), typed_query_term_options<T>(QueryOpKind::Or, options));
			}

			template <typename T>
			inline QueryImpl& QueryImpl::or_() {
				return or_<T>(QueryTermOptions{});
			}

			template <typename T>
			inline QueryImpl& QueryImpl::no(const QueryTermOptions& options) {
				return no(
						typed_query_term_entity<T>(*m_storage.world()), typed_query_term_options<T>(QueryOpKind::Not, options));
			}

			template <typename T>
			inline QueryImpl& QueryImpl::no() {
				return no<T>(QueryTermOptions{});
			}

			template <typename T>
			inline QueryImpl& QueryImpl::changed() {
				return changed(typed_query_raw_entity<T>(*m_storage.world()));
			}

			template <typename T>
			inline QueryImpl& QueryImpl::sort_by(TSortByFunc func) {
				using UO = typename component_type_t<T>::TypeOriginal;
				if constexpr (std::is_same_v<UO, Entity>)
					return sort_by(EntityBad, func);
				else
					return sort_by(typed_query_raw_entity<T>(*m_storage.world()), func);
			}

			template <typename Rel, typename Tgt>
			inline QueryImpl& QueryImpl::sort_by(TSortByFunc func) {
				return sort_by(typed_query_pair_entity<Rel, Tgt>(*m_storage.world()), func);
			}

			template <typename Rel>
			inline QueryImpl& QueryImpl::depth_order() {
				return depth_order(typed_query_raw_entity<Rel>(*m_storage.world()));
			}

			template <typename T>
			inline QueryImpl& QueryImpl::group_by(TGroupByFunc func) {
				return group_by(typed_query_raw_entity<T>(*m_storage.world()), func);
			}

			template <typename Rel, typename Tgt>
			inline QueryImpl& QueryImpl::group_by(TGroupByFunc func) {
				return group_by(typed_query_pair_entity<Rel, Tgt>(*m_storage.world()), func);
			}

			template <typename Rel>
			inline QueryImpl& QueryImpl::group_dep() {
				return group_dep(typed_query_raw_entity<Rel>(*m_storage.world()));
			}

			template <typename T>
			inline QueryImpl& QueryImpl::group_id() {
				return group_id(typed_query_raw_entity<T>(*m_storage.world()));
			}
		} // namespace detail
	} // namespace ecs
} // namespace gaia

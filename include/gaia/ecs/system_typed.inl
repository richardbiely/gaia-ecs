#pragma once

#if GAIA_SYSTEMS_ENABLED
namespace gaia {
	namespace ecs {
		template <typename T>
		inline SystemBuilder& SystemBuilder::all(const QueryTermOptions& options) {
			validate();
			data().query.template all<T>(options);
			return *this;
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::any(const QueryTermOptions& options) {
			validate();
			data().query.template any<T>(options);
			return *this;
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::or_(const QueryTermOptions& options) {
			validate();
			data().query.template or_<T>(options);
			return *this;
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::no(const QueryTermOptions& options) {
			validate();
			data().query.template no<T>(options);
			return *this;
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::all() {
			return all<T>(QueryTermOptions{});
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::any() {
			return any<T>(QueryTermOptions{});
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::or_() {
			return or_<T>(QueryTermOptions{});
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::no() {
			return no<T>(QueryTermOptions{});
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::changed() {
			using UO = typename component_type_t<T>::TypeOriginal;
			static_assert(core::is_raw_v<UO>, "Use changed() with raw types only");

			const auto& desc = comp_cache_add<T>(m_world);
			return changed(desc.entity);
		}

		template <typename Rel>
		inline SystemBuilder& SystemBuilder::depth_order() {
			using UO = typename component_type_t<Rel>::TypeOriginal;
			static_assert(core::is_raw_v<UO>, "Use depth_order() with raw relation types only");

			const auto& desc = comp_cache_add<Rel>(m_world);
			return depth_order(desc.entity);
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::group_by(TGroupByFunc func) {
			using UO = typename component_type_t<T>::TypeOriginal;
			static_assert(core::is_raw_v<UO>, "Use changed() with raw types only");

			const auto& desc = comp_cache_add<T>(m_world);
			return group_by(desc.entity, func);
		}

		template <typename Rel, typename Tgt>
		inline SystemBuilder& SystemBuilder::group_by(TGroupByFunc func) {
			using UO_Rel = typename component_type_t<Rel>::TypeOriginal;
			using UO_Tgt = typename component_type_t<Tgt>::TypeOriginal;
			static_assert(core::is_raw_v<UO_Rel>, "Use group_by() with raw types only");
			static_assert(core::is_raw_v<UO_Tgt>, "Use group_by() with raw types only");

			const auto& descRel = comp_cache_add<Rel>(m_world);
			const auto& descTgt = comp_cache_add<Tgt>(m_world);
			return group_by({descRel.entity, descTgt.entity}, func);
		}

		template <typename Rel>
		inline SystemBuilder& SystemBuilder::group_dep() {
			using UO = typename component_type_t<Rel>::TypeOriginal;
			static_assert(core::is_raw_v<UO>, "Use group_dep() with raw types only");

			const auto& desc = comp_cache_add<Rel>(m_world);
			return group_dep(desc.entity);
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::group_id() {
			using UO = typename component_type_t<T>::TypeOriginal;
			static_assert(core::is_raw_v<UO>, "Use group_id() with raw types only");

			const auto& desc = comp_cache_add<T>(m_world);
			return group_id(desc.entity);
		}

		template <typename Func, std::enable_if_t<!detail::is_query_iter_callback_v<Func>, int>>
		inline SystemBuilder& SystemBuilder::on_each(Func func) {
			validate();

			auto& ctx = data();
			const bool hasInheritedTerms = ctx.query.fetch().has_potential_inherited_id_terms();
			if (hasInheritedTerms) {
				ctx.on_each_func = [func](Query& query, QueryExecType execType) {
					query.each(func, execType);
				};
			} else {
				ctx.on_each_func = [func](Query& query, QueryExecType execType) {
					query.each(
							[&query, func](Iter& it) mutable {
								query.each_iter(it, func);
							},
							execType);
				};
			}

			return *this;
		}
	} // namespace ecs
} // namespace gaia
#endif

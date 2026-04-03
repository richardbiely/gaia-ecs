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
			validate();
			data().query.template all<T>();
			return *this;
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::any() {
			validate();
			data().query.template any<T>();
			return *this;
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::or_() {
			validate();
			data().query.template or_<T>();
			return *this;
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::no() {
			validate();
			data().query.template no<T>();
			return *this;
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::changed() {
			validate();
			data().query.template changed<T>();
			return *this;
		}

		template <typename Rel>
		inline SystemBuilder& SystemBuilder::depth_order() {
			validate();
			data().query.template depth_order<Rel>();
			return *this;
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::group_by(TGroupByFunc func) {
			validate();
			data().query.template group_by<T>(func);
			return *this;
		}

		template <typename Rel, typename Tgt>
		inline SystemBuilder& SystemBuilder::group_by(TGroupByFunc func) {
			validate();
			data().query.template group_by<Rel, Tgt>(func);
			return *this;
		}

		template <typename Rel>
		inline SystemBuilder& SystemBuilder::group_dep() {
			validate();
			data().query.template group_dep<Rel>();
			return *this;
		}

		template <typename T>
		inline SystemBuilder& SystemBuilder::group_id() {
			validate();
			data().query.template group_id<T>();
			return *this;
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

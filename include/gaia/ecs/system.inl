#include "../config/config.h"

#if !GAIA_SYSTEMS_ENABLED
namespace gaia {
	namespace ecs {
		struct System2_ {};
	} // namespace ecs
} // namespace gaia
#else

	#include <cinttypes>
	// TODO: Currently necessasry due to std::function. Replace them!
	#include <functional>

	#include "chunk_iterator.h"
	#include "id.h"
	#include "query.h"

namespace gaia {
	namespace ecs {

		struct System2_ {
			using TSystemIterFunc = std::function<void(Iter&)>;

			//! Entity identifying the system
			Entity entity;
			//! Called every time system is allowed to tick
			TSystemIterFunc on_each_func;
			//! Query associated with the system
			Query query;

			void exec() {
				auto& queryInfo = query.fetch();
				query.run_query_on_chunks<Iter>(queryInfo, on_each_func);
			}
		};

		// Usage:
		// auto s = w.system()
		// 						 .all<Position&, Velocity>()
		// 						 .any<Rotation>()
		// 						 .OnCreated([](ecs::Query& q) {
		// 						 })
		// 						 .OnStopped([](ecs::Query& q) {
		// 							 ...
		// 						 })
		// 						 .OnUpdate([](ecs::Query& q) {
		// 							 q.each([](Position& p, const Velocity& v) {
		// 								 ...
		// 							 });
		// 						 })
		// 						 .commit();
		class SystemBuilder {
			World& m_world;
			Entity m_entity;

			void validate() {
				GAIA_ASSERT(m_world.valid(m_entity));
			}

			System2_& data() {
				auto ss = m_world.acc_mut(m_entity);
				auto& sys = ss.smut<System2_>();
				return sys;
			}

		public:
			SystemBuilder(World& world, Entity entity): m_world(world), m_entity(entity) {}

			template <typename... T>
			SystemBuilder& all() {
				validate();
				data().query.all<T...>();
				return *this;
			}
			template <typename... T>
			SystemBuilder& any() {
				validate();
				data().query.any<T...>();
				return *this;
			}
			template <typename... T>
			SystemBuilder& no() {
				validate();
				data().query.no<T...>();
				return *this;
			}
			template <typename... T>
			SystemBuilder& changed() {
				validate();
				data().query.changed<T...>();
				return *this;
			}

			template <typename Func>
			SystemBuilder& on_each(Func func) {
				validate();

				auto& ctx = data();
				if constexpr (std::is_invocable_v<Func, Iter&>) {
					ctx.on_each_func = [this, func](Iter& it) {
						GAIA_PROF_SCOPE(query_func);
						func(it);
					};
				} else {
					using InputArgs = decltype(core::func_args(&Func::operator()));

	#if GAIA_ASSERT_ENABLED
					// Make sure we only use components specified in the query.
					// Constness is respected. Therefore, if a type is const when registered to query,
					// it has to be const (or immutable) also in each().
					auto& queryInfo = ctx.query.fetch();
					GAIA_ASSERT(ctx.query.unpack_args_into_query_has_all(queryInfo, InputArgs{}));
	#endif

					ctx.on_each_func = [this, func](Iter& it) {
						GAIA_PROF_SCOPE(query_func);
						data().query.run_query_on_chunk(it, func, InputArgs{});
					};
				}

				return (SystemBuilder&)*this;
			}

			GAIA_NODISCARD Entity entity() const {
				return m_entity;
			}

			void exec() {
				auto& ctx = data();
				ctx.exec();
			}
		};

	} // namespace ecs
} // namespace gaia

#endif
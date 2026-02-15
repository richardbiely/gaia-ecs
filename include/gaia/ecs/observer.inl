#include "gaia/config/config.h"

#include <cinttypes>

#include "gaia/ecs/chunk_iterator.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/observer.h"

#if GAIA_OBSERVERS_ENABLED
namespace gaia {
	namespace ecs {
		class ObserverBuilder {
			World& m_world;
			Entity m_entity;

			void validate() {
				GAIA_ASSERT(m_world.valid(m_entity));
			}

			Observer_& data() {
				auto ss = m_world.acc_mut(m_entity);
				auto& sys = ss.smut<Observer_>();
				return sys;
			}

			const Observer_& data() const {
				auto ss = m_world.acc(m_entity);
				const auto& sys = ss.get<Observer_>();
				return sys;
			}

		public:
			ObserverBuilder(World& world, Entity entity): m_world(world), m_entity(entity) {}

			//------------------------------------------------

			ObserverBuilder& event(ObserverEvent event) {
				validate();
				data().event = event;
				return *this;
			}

			//------------------------------------------------

			ObserverBuilder& add(QueryInput item) {
				validate();
				data().query.add(item);
				return *this;
			}

			//------------------------------------------------

			ObserverBuilder& all(Entity entity) {
				validate();
				data().query.all(entity, false);
				m_world.observers().add(m_world, entity, m_entity);
				return *this;
			}

			ObserverBuilder& all(Entity entity, Entity src) {
				validate();
				data().query.all(entity, src, false);
				m_world.observers().add(m_world, entity, m_entity);
				return *this;
			}

			ObserverBuilder& any(Entity entity) {
				validate();
				data().query.any(entity, false);
				m_world.observers().add(m_world, entity, m_entity);
				return *this;
			}

			ObserverBuilder& no(Entity entity) {
				validate();
				data().query.no(entity);
				m_world.observers().add(m_world, entity, m_entity);
				return *this;
			}

			//------------------------------------------------

	#if GAIA_USE_VARIADIC_API
			template <typename... T>
			ObserverBuilder& all() {
				validate();
				data().query.all<T...>();
				(m_world.observers().add(m_world, m_world.add<T>().entity(), m_entity), ...);
				return *this;
			}
			template <typename... T>
			ObserverBuilder& any() {
				validate();
				data().query.any<T...>();
				(m_world.observers().add(m_world, m_world.add<T>().entity, m_entity), ...);
				return *this;
			}
			template <typename... T>
			ObserverBuilder& no() {
				validate();
				data().query.no<T...>();
				(m_world.observers().add(m_world, m_world.add<T>().entity, m_entity), ...);
				return *this;
			}
	#else
			template <typename T>
			ObserverBuilder& all() {
				validate();
				data().query.all<T>();
				m_world.observers().add(m_world, m_world.add<T>().entity, m_entity);
				return *this;
			}
			template <typename T>
			ObserverBuilder& any() {
				validate();
				data().query.any<T>();
				m_world.observers().add(m_world, m_world.add<T>().entity, m_entity);
				return *this;
			}
			template <typename T>
			ObserverBuilder& no() {
				validate();
				data().query.no<T>();
				m_world.observers().add(m_world, m_world.add<T>().entity, m_entity);
				return *this;
			}
	#endif

			//------------------------------------------------

			ObserverBuilder& name(const char* name, uint32_t len = 0) {
				m_world.name(m_entity, name, len);
				return *this;
			}

			ObserverBuilder& name_raw(const char* name, uint32_t len = 0) {
				m_world.name_raw(m_entity, name, len);
				return *this;
			}

			//------------------------------------------------

			template <typename Func>
			ObserverBuilder& on_each(Func func) {
				validate();

				auto& ctx = data();
				if constexpr (std::is_invocable_v<Func, Iter&>) {
					ctx.on_each_func = [func](Iter& it) {
						func(it);
					};
				} else {
					using InputArgs = decltype(core::func_args(&Func::operator()));

	#if GAIA_ASSERT_ENABLED
					// Make sure we only use components specified in the query.
					// Constness is respected. Therefore, if a type is const when registered to query,
					// it has to be const (or immutable) also in each().
					auto& queryInfo = ctx.query.fetch();
					ctx.query.match_all(queryInfo);
					GAIA_ASSERT(ctx.query.unpack_args_into_query_has_all(queryInfo, InputArgs{}));
	#endif

					ctx.on_each_func = [e = m_entity, func](Iter& it) {
						// NOTE: We can't directly use data().query here because the function relies
						//       on ObserverBuilder to be present at all times. If it goes out of scope
						//       the only option left is having a copy of the world pointer and entity.
						//       They are then used to get to the query stored inside Observer_.
						auto ss = it.world()->acc_mut(e);
						auto& sys = ss.smut<Observer_>();
						sys.query.run_query_on_chunk(it, func, InputArgs{});
					};
				}

				return (ObserverBuilder&)*this;
			}

			GAIA_NODISCARD Entity entity() const {
				return m_entity;
			}

			void exec(Iter& iter, EntitySpan targets) {
				auto& ctx = data();
				ctx.exec(iter, targets);
			}
		};

	} // namespace ecs
} // namespace gaia

#endif
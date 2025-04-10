#include "../config/config.h"

#if !GAIA_SYSTEMS_ENABLED
namespace gaia {
	namespace ecs {
		struct System2_ {};
	} // namespace ecs
} // namespace gaia
#else

	#include <cinttypes>
	// TODO: Currently necessary due to std::function. Replace them!
	#include <functional>

	#include "../mt/jobhandle.h"
	#include "chunk_iterator.h"
	#include "id.h"
	#include "query.h"

namespace gaia {
	namespace ecs {

	#if GAIA_PROFILER_CPU
		inline constexpr const char* sc_query_func_str = "System2_exec";
		const char* entity_name(const World& world, Entity entity);
	#endif

		struct System2_ {
			using TSystemIterFunc = std::function<void(Iter&)>;

			//! Entity identifying the system
			Entity entity;
			//! Called every time system is allowed to tick
			TSystemIterFunc on_each_func;
			//! Query associated with the system
			Query query;
			//! Execution type
			QueryExecType execType;
			//! Query job dependency handle
			mt::JobHandle m_jobHandle = mt::JobNull;

			System2_() = default;

			~System2_() {
				// If the query contains a job handle we can only
				// destroy the query once the task associated with the handle is finished.
				if (m_jobHandle != (mt::JobHandle)mt::JobNull_t{}) {
					auto& tp = mt::ThreadPool::get();
					tp.wait(m_jobHandle);
					// Job handles created by queries are MANUAL_DELETE so delete it explicitly.
					tp.del(m_jobHandle);
				}
			}

			void exec() {
				auto& queryInfo = query.fetch();

	#if GAIA_PROFILER_CPU
				const char* pName = entity_name(*queryInfo.world(), entity);
				const char* pScopeName = pName != nullptr ? pName : sc_query_func_str;
				GAIA_PROF_SCOPE2(pScopeName);
	#endif

				switch (execType) {
					case QueryExecType::Parallel:
						query.run_query_on_chunks<QueryExecType::Parallel, Iter>(queryInfo, on_each_func);
						break;
					case QueryExecType::ParallelPerf:
						query.run_query_on_chunks<QueryExecType::ParallelPerf, Iter>(queryInfo, on_each_func);
						break;
					case QueryExecType::ParallelEff:
						query.run_query_on_chunks<QueryExecType::ParallelEff, Iter>(queryInfo, on_each_func);
						break;
					default:
						query.run_query_on_chunks<QueryExecType::Default, Iter>(queryInfo, on_each_func);
						break;
				}
			}

			//! Returns the job handle associated with the system
			GAIA_NODISCARD mt::JobHandle job_handle() {
				if (m_jobHandle == (mt::JobHandle)mt::JobNull_t{}) {
					auto& tp = mt::ThreadPool::get();
					mt::Job syncJob;
					syncJob.func = [&]() {
						exec();
					};
					syncJob.flags = mt::JobCreationFlags::ManualDelete;
					m_jobHandle = tp.add(syncJob);
				}
				return m_jobHandle;
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
			QueryExecType m_execType;

			void validate() {
				GAIA_ASSERT(m_world.valid(m_entity));
			}

			System2_& data() {
				auto ss = m_world.acc_mut(m_entity);
				auto& sys = ss.smut<System2_>();
				return sys;
			}

			const System2_& data() const {
				auto ss = m_world.acc(m_entity);
				const auto& sys = ss.get<System2_>();
				return sys;
			}

		public:
			SystemBuilder(World& world, Entity entity): m_world(world), m_entity(entity) {}

			//------------------------------------------------

			SystemBuilder& add(QueryInput item) {
				validate();
				data().query.add(item);
				return *this;
			}

			//------------------------------------------------

			SystemBuilder& all(Entity entity, bool isReadWrite = false) {
				validate();
				data().query.all(entity, isReadWrite);
				return *this;
			}

			SystemBuilder& all(Entity entity, Entity src, bool isReadWrite = false) {
				validate();
				data().query.all(entity, src, isReadWrite);
				return *this;
			}

			SystemBuilder& any(Entity entity, bool isReadWrite = false) {
				validate();
				data().query.any(entity, isReadWrite);
				return *this;
			}

			SystemBuilder& no(Entity entity) {
				validate();
				data().query.no(entity);
				return *this;
			}

			SystemBuilder& changed(Entity entity) {
				validate();
				data().query.changed(entity);
				return *this;
			}

			//------------------------------------------------

	#if GAIA_USE_VARIADIC_API
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
	#else
			template <typename T>
			SystemBuilder& all() {
				validate();
				data().query.all<T>();
				return *this;
			}
			template <typename T>
			SystemBuilder& any() {
				validate();
				data().query.any<T>();
				return *this;
			}
			template <typename T>
			SystemBuilder& no() {
				validate();
				data().query.no<T>();
				return *this;
			}
			template <typename T>
			SystemBuilder& changed() {
				validate();
				data().query.changed<T>();
				return *this;
			}
	#endif

			//------------------------------------------------

			SystemBuilder& group_by(Entity entity, TGroupByFunc func = group_by_func_default) {
				data().query.group_by(entity, func);
				return *this;
			}

			template <typename T>
			SystemBuilder& group_by(TGroupByFunc func = group_by_func_default) {
				data().query.group_by<T>(func);
				return *this;
			}

			template <typename Rel, typename Tgt>
			SystemBuilder& group_by(TGroupByFunc func = group_by_func_default) {
				data().query.group_by<Rel, Tgt>(func);
				return *this;
			}

			//------------------------------------------------

			SystemBuilder& group_id(GroupId groupId) {
				data().query.group_id(groupId);
				return *this;
			}

			SystemBuilder& group_id(Entity entity) {
				GAIA_ASSERT(!entity.pair());
				data().query.group_id(entity.id());
				return *this;
			}

			template <typename T>
			SystemBuilder& group_id() {
				data().query.group_id<T>();
				return *this;
			}

			//------------------------------------------------

			SystemBuilder& mode(QueryExecType type) {
				m_execType = type;
				return *this;
			}

			template <typename Func>
			SystemBuilder& on_each(Func func) {
				validate();

				auto& ctx = data();
				ctx.execType = m_execType;
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
					GAIA_ASSERT(ctx.query.unpack_args_into_query_has_all(queryInfo, InputArgs{}));
	#endif

					ctx.on_each_func = [e = m_entity, func](Iter& it) {
						// NOTE: We can't directly use data().query here because the function relies
						//       on SystemBuilder to be present at all times. If it goes out of scope
						//       the only option left is having a copy of the world pointer and entity.
						//       They are then used to get to the query stored inside System2_.
						auto ss = const_cast<World*>(it.world())->acc_mut(e);
						auto& sys = ss.smut<System2_>();
						sys.query.run_query_on_chunk(it, func, InputArgs{});
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

			GAIA_NODISCARD mt::JobHandle job_handle() {
				auto& ctx = data();
				return ctx.job_handle();
			}
		};

	} // namespace ecs
} // namespace gaia

#endif
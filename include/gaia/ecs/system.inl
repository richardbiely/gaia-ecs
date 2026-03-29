#include "gaia/config/config.h"

#include <cinttypes>
// TODO: Currently necessary due to std::function. Replace them!
#include <functional>

#include "gaia/ecs/chunk_iterator.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/query.h"
#include "gaia/mt/jobhandle.h"

#if GAIA_SYSTEMS_ENABLED
namespace gaia {
	namespace ecs {

	#if GAIA_PROFILER_CPU
		inline constexpr const char* sc_system_query_func_str = "System_exec";
		util::str_view entity_name(const World& world, Entity entity);
	#endif

		struct System_ {
			using TSystemExecFunc = std::function<void(Query&, QueryExecType)>;

			//! Entity identifying the system
			Entity entity = EntityBad;
			//! Called every time system is allowed to tick
			TSystemExecFunc on_each_func;
			//! Query associated with the system
			Query query;
			//! Execution type
			QueryExecType execType = QueryExecType::Default;
			//! Query job dependency handle
			mt::JobHandle jobHandle = mt::JobNull;

			System_() = default;

			~System_() {
				// If the query contains a job handle we can only
				// destroy the query once the task associated with the handle is finished.
				if (jobHandle != (mt::JobHandle)mt::JobNull_t{}) {
					auto& tp = mt::ThreadPool::get();
					tp.wait(jobHandle);
					// Job handles created by queries are MANUAL_DELETE so delete it explicitly.
					tp.del(jobHandle);
				}
			}

			void exec() {
				[[maybe_unused]] auto& queryInfo = query.fetch();

	#if GAIA_PROFILER_CPU
				const auto name = entity_name(*queryInfo.world(), entity);
				const char* pScopeName = !name.empty() ? name.data() : sc_system_query_func_str;
				GAIA_PROF_SCOPE2(pScopeName);
	#endif

				on_each_func(query, execType);
			}

			//! Returns the job handle associated with the system
			GAIA_NODISCARD mt::JobHandle job_handle() {
				if (jobHandle == (mt::JobHandle)mt::JobNull_t{}) {
					auto& tp = mt::ThreadPool::get();
					mt::Job syncJob;
					syncJob.func = [&]() {
						exec();
					};
					syncJob.flags = mt::JobCreationFlags::ManualDelete;
					jobHandle = tp.add(syncJob);
				}
				return jobHandle;
			}

			//! Disable automatic System_ serialization
			template <typename Serializer>
			void save(Serializer& s) const {
				(void)s;
			}
			//! Disable automatic System_ serialization
			template <typename Serializer>
			void load(Serializer& s) {
				(void)s;
			}
		};

		class SystemBuilder {
			World& m_world;
			Entity m_entity;

			void validate() {
				GAIA_ASSERT(m_world.valid(m_entity));
			}

			System_& data() {
				auto ss = m_world.acc_mut(m_entity);
				auto& sys = ss.smut<System_>();
				return sys;
			}

			const System_& data() const {
				auto ss = m_world.acc(m_entity);
				const auto& sys = ss.get<System_>();
				return sys;
			}

		public:
			SystemBuilder(World& world, Entity entity): m_world(world), m_entity(entity) {}

			//------------------------------------------------

			SystemBuilder& kind(QueryCacheKind kind) {
				validate();
				data().query.kind(kind);
				return *this;
			}

			SystemBuilder& scope(QueryCacheScope scope) {
				validate();
				data().query.scope(scope);
				return *this;
			}

			//------------------------------------------------

			SystemBuilder& add(QueryInput item) {
				validate();
				data().query.add(item);
				return *this;
			}

			//------------------------------------------------

			SystemBuilder& is(Entity entity, const QueryTermOptions& options = {}) {
				return all(Pair(Is, entity), options);
			}

			//------------------------------------------------

			SystemBuilder& in(Entity entity, QueryTermOptions options = {}) {
				options.in();
				return all(Pair(Is, entity), options);
			}

			//------------------------------------------------

			SystemBuilder& all(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				data().query.all(entity, options);
				return *this;
			}

			SystemBuilder& any(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				data().query.any(entity, options);
				return *this;
			}

			SystemBuilder& or_(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				data().query.or_(entity, options);
				return *this;
			}

			SystemBuilder& no(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				data().query.no(entity, options);
				return *this;
			}

			SystemBuilder& match_prefab() {
				validate();
				data().query.match_prefab();
				return *this;
			}

			SystemBuilder& changed(Entity entity) {
				validate();
				data().query.changed(entity);
				return *this;
			}

			template <typename T>
			SystemBuilder& all(const QueryTermOptions& options) {
				validate();
				data().query.template all<T>(options);
				return *this;
			}

			template <typename T>
			SystemBuilder& any(const QueryTermOptions& options) {
				validate();
				data().query.template any<T>(options);
				return *this;
			}

			template <typename T>
			SystemBuilder& or_(const QueryTermOptions& options) {
				validate();
				data().query.template or_<T>(options);
				return *this;
			}

			template <typename T>
			SystemBuilder& no(const QueryTermOptions& options) {
				validate();
				data().query.template no<T>(options);
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
			SystemBuilder& or_() {
				validate();
				data().query.or_<T...>();
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
			SystemBuilder& or_() {
				validate();
				data().query.or_<T>();
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

			//! Orders cached query entries by fragmenting relation depth so iteration runs breadth-first top-down.
			//! \param relation Fragmenting hierarchy relation
			SystemBuilder& depth_order(Entity relation = ChildOf) {
				data().query.depth_order(relation);
				return *this;
			}

			//! Orders cached query entries by fragmenting relation depth so iteration runs breadth-first top-down.
			//! \tparam Rel Fragmenting hierarchy relation, typically ChildOf.
			template <typename Rel>
			SystemBuilder& depth_order() {
				data().query.template depth_order<Rel>();
				return *this;
			}

			//------------------------------------------------

			//! Organizes matching archetypes into groups according to the grouping function and entity.
			//! \param entity The entity to group by.
			//! \param func The function to use for grouping. Returns a GroupId to group the entities by.
			SystemBuilder& group_by(Entity entity, TGroupByFunc func = group_by_func_default) {
				data().query.group_by(entity, func);
				return *this;
			}

			//! Organizes matching archetypes into groups according to the grouping function.
			//! \tparam T Component to group by. It is registered if it hasn't been registered yet.
			//! \param func The function to use for grouping. Returns a GroupId to group the entities by.
			template <typename T>
			SystemBuilder& group_by(TGroupByFunc func = group_by_func_default) {
				data().query.group_by<T>(func);
				return *this;
			}

			//! Organizes matching archetypes into groups according to the grouping function.
			//! \tparam Rel The relation to group by. It is registered if it hasn't been registered yet.
			//! \tparam Tgt The target to group by. It is registered if it hasn't been registered yet.
			//! \param func The function to use for grouping. Returns a GroupId to group the entities by.
			template <typename Rel, typename Tgt>
			SystemBuilder& group_by(TGroupByFunc func = group_by_func_default) {
				data().query.group_by<Rel, Tgt>(func);
				return *this;
			}

			//------------------------------------------------

			//! Declares an explicit relation dependency for grouped cache invalidation.
			//! Useful for custom group_by callbacks that depend on hierarchy or relation topology.
			//! \param relation Relation the group depends on.
			SystemBuilder& group_dep(Entity relation) {
				data().query.group_dep(relation);
				return *this;
			}

			//! Declares an explicit relation dependency for grouped cache invalidation.
			//! Useful for custom group_by callbacks that depend on hierarchy or relation topology.
			//! \tparam Rel Relation the group depends on.
			template <typename Rel>
			SystemBuilder& group_dep() {
				data().query.template group_dep<Rel>();
				return *this;
			}

			//------------------------------------------------

			//! Selects the group to iterate over.
			//! \param groupId The group to iterate over.
			SystemBuilder& group_id(GroupId groupId) {
				data().query.group_id(groupId);
				return *this;
			}

			//! Selects the group to iterate over.
			//! \param entity The entity to treat as a group to iterate over.
			SystemBuilder& group_id(Entity entity) {
				GAIA_ASSERT(!entity.pair());
				data().query.group_id(entity.id());
				return *this;
			}

			//! Selects the group to iterate over.
			//! \tparam T Component to treat as a group to iterate over. It is registered if it hasn't been registered yet.
			template <typename T>
			SystemBuilder& group_id() {
				data().query.template group_id<T>();
				return *this;
			}

			//------------------------------------------------

			SystemBuilder& name(const char* name, uint32_t len = 0) {
				m_world.name(m_entity, name, len);
				return *this;
			}

			SystemBuilder& name_raw(const char* name, uint32_t len = 0) {
				m_world.name_raw(m_entity, name, len);
				return *this;
			}

			//------------------------------------------------

			SystemBuilder& mode(QueryExecType type) {
				auto& ctx = data();
				ctx.execType = type;
				return *this;
			}

			template <typename Func>
			SystemBuilder& on_each(Func func) {
				validate();

				auto& ctx = data();
				using InputArgs = decltype(core::func_args(&Func::operator()));

	#if GAIA_ASSERT_ENABLED
				// Make sure we only use components specified in the query.
				// Constness is respected. Therefore, if a type is const when registered to query,
				// it has to be const (or immutable) also in each().
				if constexpr (
						!std::is_invocable_v<Func, IterAll&> && //
						!std::is_invocable_v<Func, Iter&> && //
						!std::is_invocable_v<Func, IterDisabled&> //
				) {
					auto& queryInfo = ctx.query.fetch();
					GAIA_ASSERT(ctx.query.unpack_args_into_query_has_all(queryInfo, InputArgs{}));
				}
	#endif

				ctx.on_each_func = [func](Query& query, QueryExecType execType) {
					query.each(func, execType);
				};

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

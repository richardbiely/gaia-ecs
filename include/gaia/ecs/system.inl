#include "gaia/config/config.h"

#include <cinttypes>

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

		//! Component payload stored on each Gaia-ECS system entity.
		//!
		//! A system owns a Query plus the execution mode used when the scheduler or user runs it. The callable itself is
		//! kept in SystemRegistry so function objects stay out of component storage and serialization.
		//! \see SystemBuilder
		//! \see SystemRegistry
		struct System_ {
			//! Entity identifying the system
			Entity entity = EntityBad;
			//! Query associated with the system
			Query query;
			//! Execution type
			QueryExecType execType = QueryExecType::Default;
			//! Query job dependency handle
			mt::JobHandle jobHandle = mt::JobNull;

			//! Creates an empty system component payload.
			System_() = default;

			//! Waits for and releases any outstanding manual-delete query job.
			//!
			//! Gaia-ECS query jobs store their handle here so the system can avoid destroying the query while a worker may
			//! still access it. The destructor blocks only when a job handle was actually created.
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

			//! Runs the system runtime callback against the stored query.
			//!
			//! The callback comes from SystemRegistry and receives this system's Query and QueryExecType. The query is
			//! fetched before execution so cache state and profiler naming use the latest query metadata.
			//! \param world World that owns the system runtime data.
			//! \see SystemBuilder::on_each(Func)
			//! \see QueryImpl::each_runtime_erased(QueryExecType, void*, detail::TQueryInvokeFunc, Constraints)
			void exec(World& world) {
				[[maybe_unused]] auto& queryInfo = query.fetch();

	#if GAIA_PROFILER_CPU
				const auto name = entity_name(*queryInfo.world(), entity);
				const char* pScopeName = !name.empty() ? name.data() : sc_system_query_func_str;
				GAIA_PROF_SCOPE2(pScopeName);
	#endif

				auto* pRuntime = world.systems().data_try(entity);
				GAIA_ASSERT(pRuntime != nullptr);
				if (pRuntime == nullptr || !pRuntime->on_each_func)
					return;

				static_cast<void>(pRuntime->on_each_func(query, execType, SystemRuntimeData::RunMode::Immediate));
			}

			//! Returns the job handle associated with the system.
			//!
			//! The handle is created lazily and wraps exec(World&). Gaia-ECS stores the job as ManualDelete because System_
			//! owns the handle and releases it from the destructor once the job has completed.
			//! \param world World that owns the system runtime data.
			//! \return Job handle for the system execution job.
			//! \see exec(World&)
			GAIA_NODISCARD mt::JobHandle job_handle(World& world) {
				if (jobHandle == (mt::JobHandle)mt::JobNull_t{}) {
					auto& tp = mt::ThreadPool::get();
					mt::Job syncJob;
					syncJob.func = [this, &world]() {
						exec(world);
					};
					syncJob.flags = mt::JobCreationFlags::ManualDelete;
					jobHandle = tp.add(GAIA_MOV(syncJob));
				}
				return jobHandle;
			}

			//! Prepares this system as scheduler-agnostic deferred work.
			//!
			//! The returned wrapper is backed by the system's underlying query and the world's scheduler descriptor. Unlike
			//! job_handle(World&), this path can expose the query's own parallel-for child work to external schedulers.
			//! \param world World that owns the system runtime data.
			//! \return Deferred scheduler job for this system execution, or an empty job when no callback is registered.
			//! \warning The world, system, query cache, and callback payload must outlive the returned job.
			//! \see QueryImpl::job(Func, QueryExecType)
			//! \see SchedJob
			GAIA_NODISCARD SchedJob job(World& world) {
				auto* pRuntime = world.systems().data_try(entity);
				GAIA_ASSERT(pRuntime != nullptr);
				if (pRuntime == nullptr || !pRuntime->on_each_func)
					return {};

				return pRuntime->on_each_func(query, execType, SystemRuntimeData::RunMode::DeferredJob);
			}

			//! Disables automatic System_ serialization.
			//! \tparam Serializer Serializer adapter type selected by the caller.
			//! \param s Serializer instance. Unused because runtime systems are rebuilt by code.
			template <typename Serializer>
			void save(Serializer& s) const {
				(void)s;
			}
			//! Disables automatic System_ deserialization.
			//! \tparam Serializer Serializer adapter type selected by the caller.
			//! \param s Serializer instance. Unused because runtime systems are rebuilt by code.
			template <typename Serializer>
			void load(Serializer& s) {
				(void)s;
			}
		};

		//! Fluent builder used to configure a system entity and its underlying query.
		//!
		//! Most query-shaping calls forward directly to the embedded Query held by System_. The system-specific part of the
		//! builder is registration of the runtime callback in SystemRegistry and the execution mode used when the system is
		//! run.
		//! \see World::system()
		//! \see Query
		class SystemBuilder {
			World& m_world;
			Entity m_entity;

			//! Verifies that the builder still points at a valid system entity.
			//!
			//! \warning Asserts when the entity has been deleted or otherwise invalidated.
			void validate() const {
				GAIA_ASSERT(m_world.valid(m_entity));
			}

			//! Returns the mutable System_ component configured by this builder.
			//! \return Mutable system component payload.
			System_& data() {
				auto ss = m_world.acc_mut(m_entity);
				auto& sys = ss.smut<System_>();
				return sys;
			}

			//! Returns the immutable System_ component configured by this builder.
			//! \return Immutable system component payload.
			const System_& data() const {
				auto ss = m_world.acc(m_entity);
				const auto& sys = ss.get<System_>();
				return sys;
			}

			//! Returns the mutable runtime callback payload configured by this builder.
			//! \return Mutable system runtime data.
			SystemRuntimeData& runtime_data() {
				return m_world.systems().data(m_entity);
			}

			//! Returns the immutable runtime callback payload configured by this builder.
			//! \return Immutable system runtime data.
			const SystemRuntimeData& runtime_data() const {
				return m_world.systems().data(m_entity);
			}

		public:
			//! Constructs a builder for an existing system entity.
			//! \param world World that owns the system entity and runtime data.
			//! \param entity Entity carrying the System_ component configured by this builder.
			SystemBuilder(World& world, Entity entity): m_world(world), m_entity(entity) {}

			//------------------------------------------------

			//! \name Query cache configuration
			//! \{

			//! Sets the hard cache-kind requirement for the system query.
			//! \param kind Requested cache-kind restriction.
			//! \return Self reference.
			//! \see QueryImpl::kind(QueryCacheKind)
			SystemBuilder& kind(QueryCacheKind kind) {
				validate();
				data().query.kind(kind);
				return *this;
			}

			//! Sets the cache scope used by the system query.
			//! \param scope Requested scope.
			//! \return Self reference.
			//! \see QueryImpl::scope(QueryCacheScope)
			SystemBuilder& scope(QueryCacheScope scope) {
				validate();
				data().query.scope(scope);
				return *this;
			}
			//! \}

			//------------------------------------------------

			//! \name Query term construction
			//! \{

			//! Adds a raw query term to the system query.
			//! \param item Term descriptor to append.
			//! \return Self reference.
			//! \see QueryImpl::add(QueryInput)
			SystemBuilder& add(QueryInput item) {
				validate();
				data().query.add(item);
				return *this;
			}

			//------------------------------------------------

			//! Adds an Is relationship term that matches entities derived from @a entity.
			//! \param entity Target entity used with the built-in Is relation.
			//! \param options Query-term options applied to the generated pair term.
			//! \return Self reference.
			SystemBuilder& is(Entity entity, const QueryTermOptions& options = {}) {
				return all(Pair(Is, entity), options);
			}

			//------------------------------------------------

			//! Adds an input-only Is relationship term for @a entity.
			//! \param entity Target entity used with the built-in Is relation.
			//! \param options Query-term options applied before the input flag is forced.
			//! \return Self reference.
			SystemBuilder& in(Entity entity, QueryTermOptions options = {}) {
				options.in();
				return all(Pair(Is, entity), options);
			}

			//------------------------------------------------

			//! Adds a required term to the system query.
			//! \param entity Entity or pair id that must be present.
			//! \param options Query-term options such as source, access mode, or traversal.
			//! \return Self reference.
			SystemBuilder& all(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				data().query.all(entity, options);
				return *this;
			}

			//! Adds an any-of term to the system query.
			//! \param entity Entity or pair id participating in the current any-of set.
			//! \param options Query-term options such as source, access mode, or traversal.
			//! \return Self reference.
			SystemBuilder& any(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				data().query.any(entity, options);
				return *this;
			}

			//! Adds an or term to the system query.
			//! \param entity Entity or pair id participating in the current OR chain.
			//! \param options Query-term options such as source, access mode, or traversal.
			//! \return Self reference.
			SystemBuilder& or_(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				data().query.or_(entity, options);
				return *this;
			}

			//! Adds a negated term to the system query.
			//! \param entity Entity or pair id that must not be present.
			//! \param options Query-term options such as source or traversal.
			//! \return Self reference.
			SystemBuilder& no(Entity entity, const QueryTermOptions& options = {}) {
				validate();
				data().query.no(entity, options);
				return *this;
			}

			//! Allows the system query to match prefab entities.
			//! \return Self reference.
			SystemBuilder& match_prefab() {
				validate();
				data().query.match_prefab();
				return *this;
			}

			//! Adds a changed-filter term to the system query.
			//! \param entity Entity or pair id whose changed state is tested.
			//! \return Self reference.
			SystemBuilder& changed(Entity entity) {
				validate();
				data().query.changed(entity);
				return *this;
			}

			//! Adds a required component or pair term to the system query.
			//! \tparam T Component, entity type, or pair type to require.
			//! \param options Query-term options such as source, access mode, or traversal.
			//! \return Self reference.
			template <typename T>
			SystemBuilder& all(const QueryTermOptions& options);

			//! Adds an any-of component or pair term to the system query.
			//! \tparam T Component, entity type, or pair type participating in the any-of set.
			//! \param options Query-term options such as source, access mode, or traversal.
			//! \return Self reference.
			template <typename T>
			SystemBuilder& any(const QueryTermOptions& options);

			//! Adds an or component or pair term to the system query.
			//! \tparam T Component, entity type, or pair type participating in the OR chain.
			//! \param options Query-term options such as source, access mode, or traversal.
			//! \return Self reference.
			template <typename T>
			SystemBuilder& or_(const QueryTermOptions& options);

			//! Adds a negated component or pair term to the system query.
			//! \tparam T Component, entity type, or pair type that must not be present.
			//! \param options Query-term options such as source or traversal.
			//! \return Self reference.
			template <typename T>
			SystemBuilder& no(const QueryTermOptions& options);

			//------------------------------------------------

			//! Adds a required component or pair term to the system query.
			//! \tparam T Component, entity type, or pair type to require.
			//! \return Self reference.
			template <typename T>
			SystemBuilder& all();

			//! Adds an any-of component or pair term to the system query.
			//! \tparam T Component, entity type, or pair type participating in the any-of set.
			//! \return Self reference.
			template <typename T>
			SystemBuilder& any();

			//! Adds an or component or pair term to the system query.
			//! \tparam T Component, entity type, or pair type participating in the OR chain.
			//! \return Self reference.
			template <typename T>
			SystemBuilder& or_();

			//! Adds a negated component or pair term to the system query.
			//! \tparam T Component, entity type, or pair type that must not be present.
			//! \return Self reference.
			template <typename T>
			SystemBuilder& no();

			//! Adds a changed-filter term for a component or pair.
			//! \tparam T Component, entity type, or pair type whose changed state is tested.
			//! \return Self reference.
			template <typename T>
			SystemBuilder& changed();
			//! \}

			//------------------------------------------------

			//! \name Query ordering and grouping
			//! \{

			//! Orders cached query entries by fragmenting relation depth so iteration runs breadth-first top-down.
			//! \param relation Fragmenting hierarchy relation
			//! \return Self reference.
			//! \see QueryImpl::depth_order(Entity)
			SystemBuilder& depth_order(Entity relation = ChildOf) {
				data().query.depth_order(relation);
				return *this;
			}

			//! Orders cached query entries by fragmenting relation depth so iteration runs breadth-first top-down.
			//! \tparam Rel Fragmenting hierarchy relation, typically ChildOf.
			//! \return Self reference.
			template <typename Rel>
			SystemBuilder& depth_order();

			//------------------------------------------------

			//! Organizes matching archetypes into groups according to the grouping function and entity.
			//! \param entity The entity to group by.
			//! \param func The function to use for grouping. Returns a GroupId to group the entities by.
			//! \return Self reference.
			//! \see QueryImpl::group_by(Entity, TGroupByFunc)
			SystemBuilder& group_by(Entity entity, TGroupByFunc func = group_by_func_default) {
				data().query.group_by(entity, func);
				return *this;
			}

			//! Organizes matching archetypes into groups according to the grouping function.
			//! \tparam T Component to group by. It is registered if it hasn't been registered yet.
			//! \param func The function to use for grouping. Returns a GroupId to group the entities by.
			//! \return Self reference.
			template <typename T>
			SystemBuilder& group_by(TGroupByFunc func = group_by_func_default);

			//! Organizes matching archetypes into groups according to the grouping function.
			//! \tparam Rel The relation to group by. It is registered if it hasn't been registered yet.
			//! \tparam Tgt The target to group by. It is registered if it hasn't been registered yet.
			//! \param func The function to use for grouping. Returns a GroupId to group the entities by.
			//! \return Self reference.
			template <typename Rel, typename Tgt>
			SystemBuilder& group_by(TGroupByFunc func = group_by_func_default);

			//------------------------------------------------

			//! Declares an explicit relation dependency for grouped cache invalidation.
			//! Useful for custom group_by callbacks that depend on hierarchy or relation topology.
			//! \param relation Relation the group depends on.
			//! \return Self reference.
			SystemBuilder& group_dep(Entity relation) {
				data().query.group_dep(relation);
				return *this;
			}

			//! Declares an explicit relation dependency for grouped cache invalidation.
			//! Useful for custom group_by callbacks that depend on hierarchy or relation topology.
			//! \tparam Rel Relation the group depends on.
			//! \return Self reference.
			template <typename Rel>
			SystemBuilder& group_dep();

			//------------------------------------------------

			//! Selects the group to iterate over.
			//! \param groupId The group to iterate over.
			//! \return Self reference.
			SystemBuilder& group_id(GroupId groupId) {
				data().query.group_id(groupId);
				return *this;
			}

			//! Selects the group to iterate over.
			//! \param entity The entity to treat as a group to iterate over.
			//! \return Self reference.
			SystemBuilder& group_id(Entity entity) {
				GAIA_ASSERT(!entity.pair());
				data().query.group_id(entity.id());
				return *this;
			}

			//! Selects the group to iterate over.
			//! \tparam T Component to treat as a group to iterate over. It is registered if it hasn't been registered yet.
			//! \return Self reference.
			template <typename T>
			SystemBuilder& group_id();
			//! \}

			//------------------------------------------------

			//! \name System metadata
			//! \{

			//! Assigns a human-readable name to the system entity.
			//! \param name UTF-8 name. Gaia-ECS copies or interns the string through World::name().
			//! \param len Optional byte length. 0 lets Gaia-ECS measure the null-terminated string.
			//! \return Self reference.
			SystemBuilder& name(const char* name, uint32_t len = 0) {
				m_world.name(m_entity, name, len);
				return *this;
			}

			//! Assigns a raw name to the system entity without additional name normalization.
			//! \param name UTF-8 name. Gaia-ECS copies or interns the string through World::name_raw().
			//! \param len Optional byte length. 0 lets Gaia-ECS measure the null-terminated string.
			//! \return Self reference.
			SystemBuilder& name_raw(const char* name, uint32_t len = 0) {
				m_world.name_raw(m_entity, name, len);
				return *this;
			}

			//! Adds the system to a phase entity used by World::systems_run() ordering.
			//!
			//! The phase is represented with existing Gaia relationships: `(ChildOf, phase)` for grouping and enabled-state
			//! inheritance, plus `(DependsOn, phase)` so the system joins the phase's depth-first postorder path. During
			//! World::systems_run(), phased systems are batched by phase and phase subtrees run before their DependsOn
			//! target phase. Explicit DependsOn edges between systems use the same child-before-target postorder inside
			//! the same phase.
			//!
			//! \param phaseEntity Entity representing the phase this system belongs to.
			//! \return Self reference.
			//! \warning This appends relationships and does not remove an earlier phase assignment.
			//! \see World::systems_run()
			//! \see DependsOn
			//! \see ChildOf
			SystemBuilder& phase(Entity phaseEntity) {
				validate();
				GAIA_ASSERT(m_world.valid(phaseEntity));
				GAIA_ASSERT(!phaseEntity.pair());
				m_world.add(m_entity, {ChildOf, phaseEntity});
				m_world.add(m_entity, {DependsOn, phaseEntity});
				return *this;
			}
			//! \}

			//------------------------------------------------

			//! \name System execution
			//! \{

			//! Sets the execution mode used when the system runs.
			//! \param type Query execution mode, for example sequential or parallel execution.
			//! \return Self reference.
			SystemBuilder& mode(QueryExecType type) {
				auto& ctx = data();
				ctx.execType = type;
				return *this;
			}

			//! \name System context
			//! \{
			//! Stores a raw, user-owned pointer on the underlying query and exposes it through Iter::ctx().
			//! \see QueryImpl::ctx(void*)
			//! \see Iter::ctx() const

			//! Sets user-owned data passed to system iterator callbacks.
			//! The system stores this pointer on its underlying query, exactly like QueryImpl::ctx(void*). It does not own,
			//! allocate, copy, or destroy the pointed-to data.
			//! \note If the system runs through parallel query execution, every worker observes the same pointer. The caller
			//! owns synchronization for mutable data referenced by the context.
			//! \param pCtx Context pointer. May be null.
			//! \return Self reference.
			SystemBuilder& ctx(void* pCtx) {
				validate();
				data().query.ctx(pCtx);
				return *this;
			}

			//! Returns the user-owned context pointer attached to the system.
			//! \return Context pointer stored on the underlying query, or null when none was attached.
			GAIA_NODISCARD void* ctx() const {
				validate();
				return data().query.ctx();
			}
			//! \}

			//! \name System scheduling declarations
			//! \{
			//! Marks whether this system must run on the main thread/serial path.
			//! \param required True when the system requires the main thread/serial path.
			//! \return Self reference.
			//! \see QueryImpl::main_thread(bool)
			SystemBuilder& main_thread(bool required = true) {
				validate();
				data().query.main_thread(required);
				return *this;
			}
			//! \}

			//! \name System access declarations
			//! \{
			//! Declares an additional id read by this system callback.
			//! \param entity Component/entity id read by user code.
			//! \return Self reference.
			//! \see QueryImpl::reads(Entity)
			SystemBuilder& reads(Entity entity) {
				validate();
				data().query.reads(entity);
				return *this;
			}

			//! Declares an additional component or pair type read by this system callback.
			//! \tparam T Component, entity type, or pair type to mark as read.
			//! \return Self reference.
			//! \see QueryImpl::reads()
			template <typename T>
			SystemBuilder& reads() {
				validate();
				data().query.template reads<T>();
				return *this;
			}

			//! Declares an additional id written by this system callback.
			//! \param entity Component/entity id written by user code.
			//! \return Self reference.
			//! \see QueryImpl::writes(Entity)
			SystemBuilder& writes(Entity entity) {
				validate();
				data().query.writes(entity);
				return *this;
			}

			//! Declares an additional component or pair type written by this system callback.
			//! \tparam T Component, entity type, or pair type to mark as written.
			//! \return Self reference.
			//! \see QueryImpl::writes()
			template <typename T>
			SystemBuilder& writes() {
				validate();
				data().query.template writes<T>();
				return *this;
			}
			//! \}

			//! Registers an iterator-style callback executed when the system runs.
			//! \tparam Func Callable type invocable with Iter&.
			//! \param func Callback copied into the system runtime payload.
			//! \return Self reference.
			//! \see Iter::ctx() const
			template <typename Func, std::enable_if_t<detail::is_query_iter_callback_v<Func>, int> = 0>
			SystemBuilder& on_each(Func func) {
				validate();

				auto& runtime = runtime_data();
				runtime.on_each_func = [func](Query& query, QueryExecType execType, SystemRuntimeData::RunMode mode) mutable {
					if (mode == SystemRuntimeData::RunMode::DeferredJob)
						return query.job(func, execType);

					query.each_runtime_erased(
							execType, static_cast<void*>(&func), &detail::QueryImpl::template invoke_runtime_iter<Func, Iter>,
							Constraints::EnabledOnly);
					return SchedJob{};
				};

				return (SystemBuilder&)*this;
			}

			//! Registers a typed component callback executed when the system runs.
			//! \tparam Func Callable type accepted by the typed query callback adapter.
			//! \param func Callback copied into the system runtime payload.
			//! \return Self reference.
			//! \note Typed component callbacks use Gaia-ECS typed query dispatch and do not receive Iter directly. Use an
			//! iterator-style callback when the system needs access to Iter::ctx().
			template <typename Func, std::enable_if_t<!detail::is_query_iter_callback_v<Func>, int> = 0>
			SystemBuilder& on_each(Func func);

			//! Returns the entity configured by this builder.
			//! \return System entity.
			GAIA_NODISCARD Entity entity() const {
				return m_entity;
			}

			//! Runs the system immediately using its configured query and execution mode.
			//! \see mode(QueryExecType)
			//! \see job_handle()
			void exec() {
				auto& ctx = data();
				ctx.exec(m_world);
			}

			//! Prepares the system as scheduler-agnostic deferred work.
			//!
			//! The returned wrapper uses the system's underlying query. Parallel systems therefore add query child work
			//! directly instead of wrapping a blocking exec() call.
			//! \return Deferred scheduler job for this system execution.
			//! \warning The world, system, and callback payload must outlive the returned job.
			//! \see QueryImpl::job(Func, QueryExecType)
			//! \see SchedJob
			GAIA_NODISCARD SchedJob job() {
				auto& ctx = data();
				return ctx.job(m_world);
			}

			//! Returns the active job handle for systems scheduled through Gaia-ECS jobs.
			//! \return Job handle associated with this system runtime data.
			GAIA_NODISCARD mt::JobHandle job_handle() {
				auto& ctx = data();
				return ctx.job_handle(m_world);
			}
			//! \}
		};

	} // namespace ecs
} // namespace gaia

	#include "gaia/ecs/system_typed.inl"

#endif

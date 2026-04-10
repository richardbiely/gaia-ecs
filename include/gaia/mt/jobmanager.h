#pragma once

#include "gaia/config/config.h"
#include "gaia/config/profiler.h"

#include <atomic>
#include <cinttypes>

#include "gaia/cnt/ilist.h"
#include "gaia/core/span.h"
#include "gaia/core/utility.h"
#include "gaia/mem/mem_alloc.h"
#include "gaia/mt/jobcommon.h"
#include "gaia/mt/jobhandle.h"

#define GAIA_LOG_JOB_STATES 0

namespace gaia {
	namespace mt {
		enum JobState : uint32_t {
			DEP_BITS_START = 0,
			DEP_BITS = 27,
			DEP_BITS_MASK = (uint32_t)((1u << DEP_BITS) - 1),

			STATE_BITS_START = DEP_BITS_START + DEP_BITS,
			STATE_BITS = 4,
			STATE_BITS_MASK = (uint32_t)(((1u << STATE_BITS) - 1) << STATE_BITS_START),

			// STATE

			//! Submitted
			Submitted = 0x01 << STATE_BITS_START,
			//! Processing has begun, the job is in one of the job buffers
			Processing = 0x02 << STATE_BITS_START,
			//! Being executed
			Executing = 0x03 << STATE_BITS_START,
			//! Finished executing
			Done = 0x04 << STATE_BITS_START,
			//! Released
			Released = 0x05 << STATE_BITS_START
		};

		struct JobContainer;

		namespace detail {
			inline void signal_edge(JobContainer& jobData);
		}

		struct JobEdges {
			//! Dependency or an array of dependencies.
			//! depCnt decides which one is used.
			union {
				JobHandle dep;
				JobHandle* pDeps;
			};
			//! Number of dependencies
			uint32_t depCnt = 0;

			JobEdges() {
				dep = {};
			}
		};

		struct JobContainer: cnt::ilist_item {
			//! Current state of the job
			//! Consist of upper and bottom part.
			//! Least significant bits = special purpose.
			//! Most significant bits = state.
			//! JobCore states:
			//!           x  | edge count
			//!   Submitted  | edge count, must be 0 in order to move to Processing
			//!   Processing | 0, pushed into one of the worker jobs queues
			//!   Executing  | worker_idx of the worker executing the job
			//!   Done       | 0, wait() stops when job reaches this state
			std::atomic_uint32_t state;
			//! Job priority
			JobPriority prio;
			//! Job flags
			JobCreationFlags flags;
			//! Dependency graph
			JobEdges edges;
			//! Function to execute when running the job
			util::SmallFunc func;

			JobContainer() = default;
			~JobContainer() = default;

			JobContainer(const JobContainer& other) = delete;
			JobContainer& operator=(const JobContainer& other) = delete;

			JobContainer(JobContainer&& other): cnt::ilist_item(GAIA_MOV(other)) {
				state = other.state.load();
				prio = other.prio;
				flags = other.flags;
				func = GAIA_MOV(other.func);

				// if (edges.depCnt > 0)
				// 	detail::signal_edge(*this);
				edges = other.edges;
				other.edges.depCnt = 0;
			}
			JobContainer& operator=(JobContainer&& other) {
				GAIA_ASSERT(core::addressof(other) != this);
				cnt::ilist_item::operator=(GAIA_MOV(other));
				state = other.state.load();
				prio = other.prio;
				flags = other.flags;
				func = GAIA_MOV(other.func);

				// if (edges.depCnt > 0)
				// 	detail::signal_edge(*this);
				edges = other.edges;
				other.edges.depCnt = 0;

				return *this;
			}

			GAIA_NODISCARD static JobContainer create(uint32_t index, uint32_t generation, void* pCtx) {
				auto* ctx = (JobAllocCtx*)pCtx;

				JobContainer jc{};
				jc.idx = index;
				jc.data.gen = generation;
				jc.prio = ctx->priority;

				return jc;
			}

			GAIA_NODISCARD static JobHandle handle(const JobContainer& jc) {
				return JobHandle(jc.idx, jc.data.gen, (jc.prio == JobPriority::Low) != 0);
			}
		};

		struct ParallelCallbackHandle {
			static constexpr uint32_t IdMask = uint32_t(-1);

			uint32_t m_id = IdMask;
			uint32_t m_gen = 0;

			ParallelCallbackHandle() = default;
			ParallelCallbackHandle(uint32_t id, uint32_t gen): m_id(id), m_gen(gen) {}

			GAIA_NODISCARD uint32_t id() const {
				return m_id;
			}
			
			GAIA_NODISCARD uint32_t gen() const {
				return m_gen;
			}

			GAIA_NODISCARD bool operator==(const ParallelCallbackHandle& other) const {
				return m_id == other.m_id && m_gen == other.m_gen;
			}
		};

		struct ParallelCallbackAllocCtx {
			JobArgsFunc callback;
			uint32_t refs = 0;
		};

		struct ParallelCallbackRecord: cnt::ilist_item {
			JobArgsFunc callback;
			std::atomic_uint32_t refs = 0;

			ParallelCallbackRecord() = default;
			~ParallelCallbackRecord() = default;

			ParallelCallbackRecord(const ParallelCallbackRecord&) = delete;
			ParallelCallbackRecord& operator=(const ParallelCallbackRecord&) = delete;

			ParallelCallbackRecord(ParallelCallbackRecord&& other) noexcept:
					cnt::ilist_item(GAIA_MOV(other)), callback(GAIA_MOV(other.callback)) {
				refs.store(other.refs.load(std::memory_order_relaxed), std::memory_order_relaxed);
			}

			ParallelCallbackRecord& operator=(ParallelCallbackRecord&& other) noexcept {
				GAIA_ASSERT(core::addressof(other) != this);
				cnt::ilist_item::operator=(GAIA_MOV(other));
				callback = GAIA_MOV(other.callback);
				refs.store(other.refs.load(std::memory_order_relaxed), std::memory_order_relaxed);
				return *this;
			}

			GAIA_NODISCARD static ParallelCallbackRecord create(uint32_t index, uint32_t generation, void* pCtx) {
				auto* ctx = (ParallelCallbackAllocCtx*)pCtx;

				ParallelCallbackRecord record{};
				record.idx = index;
				record.data.gen = generation;
				record.callback = GAIA_MOV(ctx->callback);
				record.refs.store(ctx->refs, std::memory_order_relaxed);
				return record;
			}

			GAIA_NODISCARD static ParallelCallbackHandle handle(const ParallelCallbackRecord& record) {
				return ParallelCallbackHandle(record.idx, record.data.gen);
			}
		};

		class JobManager {
			//! Implicit list of jobs. Page allocated, memory addresses are always fixed.
			cnt::ilist<JobContainer, JobHandle> m_jobData;
			//! Shared callback records for parallel jobs.
			cnt::ilist<ParallelCallbackRecord, ParallelCallbackHandle> m_parallelCallbacks;

		public:
			JobContainer& data(JobHandle jobHandle) {
				return m_jobData[jobHandle.id()];
			}
			const JobContainer& data(JobHandle jobHandle) const {
				return m_jobData[jobHandle.id()];
			}

			//! Allocates a new job container identified by a unique JobHandle.
			//! \return JobHandle
			//! \warning Must be used from the main thread.
			GAIA_NODISCARD JobHandle alloc_job(Job job) {
				JobAllocCtx ctx{job.priority};

				auto handle = m_jobData.alloc(&ctx);
				auto& j = m_jobData[handle.id()];

				// Make sure there is not state yet
				GAIA_ASSERT(j.state == 0 || j.state == JobState::Released);

				j.edges = {};
				j.prio = ctx.priority;
				j.state.store(0);
				j.func = GAIA_MOV(job.func);
				j.flags = job.flags;
				return handle;
			}

			GAIA_NODISCARD ParallelCallbackHandle alloc_parallel_callback(JobArgsFunc callback, uint32_t refs) {
				ParallelCallbackAllocCtx ctx{};
				ctx.callback = GAIA_MOV(callback);
				ctx.refs = refs;
				return m_parallelCallbacks.alloc(&ctx);
			}

			//! Invalidates @a jobHandle by resetting its index in the job pool.
			//! Every time a job is deallocated its generation is increased by one.
			//! \param jobHandle Job handle.
			//! \warning Must be used from the main thread.
			void free_job(JobHandle jobHandle) {
				auto& jobData = m_jobData.free(jobHandle);
				GAIA_ASSERT(done(jobData));
				jobData.state.store(JobState::Released);
			}

			void free_parallel_callback(ParallelCallbackHandle handle) {
				auto& record = m_parallelCallbacks[handle.id()];
				record.callback.reset();
				record.refs.store(0, std::memory_order_relaxed);
				m_parallelCallbacks.free(handle);
			}

			//! Resets the job pool.
			void reset() {
				m_jobData.clear();
				m_parallelCallbacks.clear();
			}

			//! Execute the functor associated with the job container
			//! \param jobData Container with internal job specific data
			static void run(JobContainer& jobData) {
				if (jobData.func.operator bool())
					jobData.func();

				finalize(jobData);
			}

			static bool signal_edge(JobContainer& jobData) {
				// Subtract from dependency counter
				const auto state = jobData.state.fetch_sub(1) - 1;

				// If the job is not submitted, we can't accept it
				const auto s = state & JobState::STATE_BITS_MASK;
				if (s != JobState::Submitted)
					return false;

				// If the job still has some dependencies left we can't accept it
				const auto deps = state & JobState::DEP_BITS_MASK;
				return deps == 0;
			}

			static void free_edges(JobContainer& jobData) {
				// We only allocate an array for 2 and more dependencies
				if (jobData.edges.depCnt <= 1)
					return;

				mem::AllocHelper::free(jobData.edges.pDeps);
				// jobData.edges.depCnt = 0;
				// jobData.edges.pDeps = nullptr;
			}

			//! Makes @a jobSecond depend on @a jobFirst.
			//! This means @a jobSecond will not run until @a jobFirst finishes.
			//! \param jobFirst The job that must complete first.
			//! \param jobSecond The job that will run after @a jobFirst.
			//! \warning This must be called before any of the listed jobs are scheduled.
			void dep(JobHandle jobFirst, JobHandle jobSecond) {
				dep(std::span(&jobFirst, 1), jobSecond);
			}

			//! Makes @a jobSecond depend on the jobs listed in @a jobsFirst.
			//! This means @a jobSecond will not run until all jobs from @a jobsFirst finish.
			//! \param jobsFirst Jobs that must complete first.
			//! \param jobSecond The job that will run after @a jobsFirst.
			//! \warning This must must to be called before any of the listed jobs are scheduled.
			void dep(std::span<JobHandle> jobsFirst, JobHandle jobSecond) {
				GAIA_ASSERT(!jobsFirst.empty());

				GAIA_PROF_SCOPE(JobManager::dep);

				auto& secondData = data(jobSecond);

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(!busy(const_cast<const JobContainer&>(secondData)));
				for (auto jobFirst: jobsFirst) {
					const auto& firstData = data(jobFirst);
					GAIA_ASSERT(!busy(firstData));
				}
#endif

				for (auto jobFirst: jobsFirst)
					dep_internal(jobFirst, jobSecond);

				// Tell jobSecond that it has new dependencies
				// secondData.canWait = true;
				const uint32_t cnt = (uint32_t)jobsFirst.size();
				[[maybe_unused]] const uint32_t statePrev = secondData.state.fetch_add(cnt);
				GAIA_ASSERT((statePrev & JobState::DEP_BITS_MASK) < DEP_BITS_MASK - 1);
			}

			//! Makes @a jobSecond depend on the jobs listed in @a jobsFirst.
			//! This means @a jobSecond will not run until all jobs from @a jobsFirst finish.
			//! \param jobsFirst Jobs that must complete first.
			//! \param jobSecond The job that will run after @a jobsFirst.
			//! \note Unlike dep() this function needs to be called when job handles are reused.
			//! \warning This must be called before any of the listed jobs are scheduled.
			void dep_refresh(std::span<JobHandle> jobsFirst, JobHandle jobSecond) {
				GAIA_ASSERT(!jobsFirst.empty());

				GAIA_PROF_SCOPE(JobManager::dep_refresh);

				auto& secondData = data(jobSecond);

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(!busy(const_cast<const JobContainer&>(secondData)));
				for (auto jobFirst: jobsFirst) {
					const auto& firstData = data(jobFirst);
					GAIA_ASSERT(!busy(firstData));
				}

				for (auto jobFirst: jobsFirst)
					dep_refresh_internal(jobFirst, jobSecond);
#endif

				// Tell jobSecond that it has new dependencies
				// secondData.canWait = true;
				const uint32_t cnt = (uint32_t)jobsFirst.size();
				[[maybe_unused]] const uint32_t statePrev = secondData.state.fetch_add(cnt);
				GAIA_ASSERT((statePrev & JobState::DEP_BITS_MASK) < DEP_BITS_MASK - 1);
			}

			static uint32_t submit(JobContainer& jobData) {
				[[maybe_unused]] const auto state = jobData.state.load() & JobState::STATE_BITS_MASK;
				GAIA_ASSERT(state < JobState::Submitted);
				const auto val = jobData.state.fetch_add(JobState::Submitted) + (uint32_t)JobState::Submitted;
#if GAIA_LOG_JOB_STATES
				GAIA_LOG_N("JobHandle %u.%u - SUBMITTED", jobData.idx, jobData.gen);
#endif
				return val;
			}

			static void processing(JobContainer& jobData) {
				GAIA_ASSERT(submitted(const_cast<const JobContainer&>(jobData)));
				jobData.state.store(JobState::Processing);
#if GAIA_LOG_JOB_STATES
				GAIA_LOG_N("JobHandle %u.%u - PROCESSING", jobData.idx, jobData.gen);
#endif
			}

			static void executing(JobContainer& jobData, uint32_t workerIdx) {
				GAIA_ASSERT(processing(const_cast<const JobContainer&>(jobData)));
				jobData.state.store(JobState::Executing | workerIdx);
#if GAIA_LOG_JOB_STATES
				GAIA_LOG_N("JobHandle %u.%u - EXECUTING", jobData.idx, jobData.gen);
#endif
			}

			static void finalize(JobContainer& jobData) {
				jobData.state.store(JobState::Done);
#if GAIA_LOG_JOB_STATES
				GAIA_LOG_N("JobHandle %u.%u - DONE", jobData.idx, jobData.gen);
#endif
			}

			static void reset_state(JobContainer& jobData) {
				[[maybe_unused]] const auto state = jobData.state.load() & JobState::STATE_BITS_MASK;
				// The job needs to be either clear or finalize for us to allow a reset
				GAIA_ASSERT(state == 0 || state == JobState::Done);
				jobData.state.store(0);
#if GAIA_LOG_JOB_STATES
				GAIA_LOG_N("JobHandle %u.%u - RESET_STATE", jobData.idx, jobData.gen);
#endif
			}

			GAIA_NODISCARD bool is_clear(JobHandle jobHandle) const {
				const auto& jobData = data(jobHandle);
				const auto state = jobData.state.load();
				return state == 0;
			}

			GAIA_NODISCARD static bool is_clear(JobContainer& jobData) {
				const auto state = jobData.state.load();
				return state == 0;
			}

			GAIA_NODISCARD static bool submitted(const JobContainer& jobData) {
				const auto state = jobData.state.load() & JobState::STATE_BITS_MASK;
				return state == JobState::Submitted;
			}

			GAIA_NODISCARD static bool processing(const JobContainer& jobData) {
				const auto state = jobData.state.load() & JobState::STATE_BITS_MASK;
				return state == JobState::Processing;
			}

			GAIA_NODISCARD static bool busy(const JobContainer& jobData) {
				const auto state = jobData.state.load() & JobState::STATE_BITS_MASK;
				return state == JobState::Executing || state == JobState::Processing;
			}

			GAIA_NODISCARD static bool done(const JobContainer& jobData) {
				const auto state = jobData.state.load() & JobState::STATE_BITS_MASK;
				return state == JobState::Done;
			}

			void invoke_parallel_callback(ParallelCallbackHandle handle, const JobArgs& args) {
				auto& record = m_parallelCallbacks[handle.id()];
				GAIA_ASSERT(record.data.gen == handle.gen());
				record.callback(args);
			}

			GAIA_NODISCARD bool release_parallel_callback_ref(ParallelCallbackHandle handle) {
				auto& record = m_parallelCallbacks[handle.id()];
				GAIA_ASSERT(record.data.gen == handle.gen());
				return record.refs.fetch_sub(1, std::memory_order_acq_rel) == 1;
			}

		private:
			void dep_internal(JobHandle jobFirst, JobHandle jobSecond) {
				GAIA_ASSERT(jobFirst != (JobHandle)JobNull_t{});
				GAIA_ASSERT(jobSecond != (JobHandle)JobNull_t{});

				auto& firstData = data(jobFirst);
				const auto depCnt0 = firstData.edges.depCnt;
				const auto depCnt1 = ++firstData.edges.depCnt;

#if GAIA_LOG_JOB_STATES
				GAIA_LOG_N(
						"DEP %u.%u, %u -> %u.%u", jobFirst.id(), jobFirst.gen(), firstData.edges.depCnt, jobSecond.id(),
						jobSecond.gen());
#endif

				if (depCnt1 <= 1) {
					firstData.edges.dep = jobSecond;
				} else if (depCnt1 == 2) {
					auto prev = firstData.edges.dep;
					// TODO: Use custom allocator
					firstData.edges.pDeps = mem::AllocHelper::alloc<JobHandle>(depCnt1);
					firstData.edges.pDeps[0] = prev;
					firstData.edges.pDeps[1] = jobSecond;
				} else {
					// Reallocate if the previous value was a power of 2
					const bool isPow2 = core::is_pow2(depCnt0);
					if (isPow2) {
						const auto nextPow2 = depCnt0 << 1;
						auto* pPrev = firstData.edges.pDeps;
						// TODO: Use custom allocator
						firstData.edges.pDeps = mem::AllocHelper::alloc<JobHandle>(nextPow2);
						if (pPrev != nullptr) {
							GAIA_FOR(depCnt0) firstData.edges.pDeps[i] = pPrev[i];
							mem::AllocHelper::free(pPrev);
						}
					}

					// Append new dependencies
					firstData.edges.pDeps[depCnt0] = jobSecond;
				}
			}

#if GAIA_ASSERT_ENABLED
			void dep_refresh_internal(JobHandle jobFirst, JobHandle jobSecond) const {
				GAIA_ASSERT(jobFirst != (JobHandle)JobNull_t{});
				GAIA_ASSERT(jobSecond != (JobHandle)JobNull_t{});

				const auto& firstData = data(jobFirst);
				const auto depCnt = firstData.edges.depCnt;

				if (depCnt <= 1) {
					GAIA_ASSERT(firstData.edges.dep == jobSecond);
				} else {
					GAIA_ASSERT(firstData.edges.pDeps != nullptr);
					bool found = false;
					GAIA_FOR(firstData.edges.depCnt) {
						if (firstData.edges.pDeps[i] == jobSecond) {
							found = true;
							break;
						}
					}
					GAIA_ASSERT(found);
				}
			}
#endif
		};

		namespace detail {
			void signal_edge(JobContainer& jobData) {
				JobManager::signal_edge(jobData);
			}
		} // namespace detail
	} // namespace mt
} // namespace gaia

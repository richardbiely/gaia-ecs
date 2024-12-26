#pragma once

#include "../config/config.h"
#include "../config/profiler.h"

#include <atomic>
#include <cinttypes>
#include <functional>

#include "../cnt/ilist.h"
#include "../core/span.h"
#include "../core/utility.h"
#include "../mem/mem_alloc.h"
#include "jobcommon.h"
#include "jobhandle.h"

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
			uint32_t depCnt;
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
			std::function<void()> func;

			JobContainer() = default;
			~JobContainer() = default;

			JobContainer(const JobContainer& other): cnt::ilist_item(other) {
				state = other.state.load();
				prio = other.prio;
				flags = other.flags;
				edges = other.edges;
				func = other.func;
			}
			JobContainer& operator=(const JobContainer& other) {
				GAIA_ASSERT(core::addressof(other) != this);
				cnt::ilist_item::operator=(other);
				state = other.state.load();
				prio = other.prio;
				flags = other.flags;
				edges = other.edges;
				func = other.func;
				return *this;
			}

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
				jc.gen = generation;
				jc.prio = ctx->priority;
				jc.state.store(0);

				return jc;
			}

			GAIA_NODISCARD static JobHandle handle(const JobContainer& jc) {
				return JobHandle(jc.idx, jc.gen, (jc.prio == JobPriority::Low) != 0);
			}
		};

		class JobManager {
			//! Implicit list of jobs. Page allocated, memory addresses are always fixed.
			cnt::ilist<JobContainer, JobHandle> m_jobData;

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
			GAIA_NODISCARD JobHandle alloc_job(const Job& job) {
				JobAllocCtx ctx{job.priority};

				auto handle = m_jobData.alloc(&ctx);
				auto& j = m_jobData[handle.id()];

				// Make sure there is not state yet
				GAIA_ASSERT(j.state == 0 || j.state == JobState::Released);

				j.edges = {};
				j.prio = ctx.priority;
				j.state.store(0);
				j.func = job.func;
				j.flags = job.flags;
				return handle;
			}

			//! Invalidates \param jobHandle by resetting its index in the job pool.
			//! Every time a job is deallocated its generation is increased by one.
			//! \warning Must be used from the main thread.
			void free_job(JobHandle jobHandle) {
				auto& jobData = m_jobData.free(jobHandle);
				GAIA_ASSERT(done(jobData));
				jobData.state.store(JobState::Released);
			}

			//! Resets the job pool.
			void reset() {
				m_jobData.clear();
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
				if (jobData.edges.depCnt > 1)
					mem::AllocHelper::free(jobData.edges.pDeps);
			}

			//! Makes \param jobSecond depend on \param jobFirst.
			//! This means \param jobSecond will run only after \param jobFirst finishes.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void dep(JobHandle jobFirst, JobHandle jobSecond) {
				dep(std::span(&jobFirst, 1), jobSecond);
			}

			//! Makes \param jobsFirst depend on the jobs listed in \param jobSecond.
			//! This means \param jobSecond will run only after all \param jobsFirst finish.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
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

			static uint32_t submit(JobContainer& jobData) {
				[[maybe_unused]] const auto state = jobData.state.load() & JobState::STATE_BITS_MASK;
				GAIA_ASSERT(state < JobState::Submitted);
				const auto val = jobData.state.fetch_add(JobState::Submitted) + (uint32_t)JobState::Submitted;
#if GAIA_LOG_JOB_STATES
				GAIA_LOG_N("%u.%u - SUBMITTED", jobData.idx, jobData.gen);
#endif
				return val;
			}

			static void processing(JobContainer& jobData) {
				GAIA_ASSERT(submitted(const_cast<const JobContainer&>(jobData)));
				jobData.state.store(JobState::Processing);
#if GAIA_LOG_JOB_STATES
				GAIA_LOG_N("%u.%u - PROCESSING", jobData.idx, jobData.gen);
#endif
			}

			static void executing(JobContainer& jobData, uint32_t workerIdx) {
				GAIA_ASSERT(processing(const_cast<const JobContainer&>(jobData)));
				jobData.state.store(JobState::Executing | workerIdx);
#if GAIA_LOG_JOB_STATES
				GAIA_LOG_N("%u.%u - EXECUTING", jobData.idx, jobData.gen);
#endif
			}

			static void finalize(JobContainer& jobData) {
				jobData.state.store(JobState::Done);
#if GAIA_LOG_JOB_STATES
				GAIA_LOG_N("%u.%u - DONE", jobData.idx, jobData.gen);
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

		private:
			void dep_internal(JobHandle jobFirst, JobHandle jobSecond) {
				GAIA_ASSERT(jobFirst != (JobHandle)JobNull_t{});
				GAIA_ASSERT(jobSecond != (JobHandle)JobNull_t{});

				auto& firstData = data(jobFirst);
				const auto depCnt0 = firstData.edges.depCnt;
				const auto depCnt1 = ++firstData.edges.depCnt;

				if (depCnt1 <= 1) {
#if GAIA_LOG_JOB_STATES
					GAIA_LOG_N(
							"DEP %u.%u, %u -> %u.%u", jobFirst.id(), jobFirst.gen(), firstData.edges.depCnt, jobSecond.id(),
							jobSecond.gen());
#endif
					firstData.edges.dep = jobSecond;
				} else {
					// Reallocate on a power of two
					const bool isPow2 = core::is_pow2(depCnt1);
					if (isPow2) {
						if (depCnt0 == 1) {
							firstData.edges.pDeps = mem::AllocHelper::alloc<JobHandle>(depCnt1);
							firstData.edges.pDeps[0] = firstData.edges.dep;
						} else {
							auto* pPrev = firstData.edges.pDeps;
							// TODO: Use custom allocator
							firstData.edges.pDeps = mem::AllocHelper::alloc<JobHandle>(depCnt1);
							if (pPrev != nullptr) {
								GAIA_FOR2(0, depCnt0) firstData.edges.pDeps[i] = pPrev[i];
								mem::AllocHelper::free(pPrev);
							}
						}
					}

					// Append new dependencies
					firstData.edges.pDeps[depCnt0] = jobSecond;
				}
			}
		};

		namespace detail {
			void signal_edge(JobContainer& jobData) {
				JobManager::signal_edge(jobData);
			}
		} // namespace detail
	} // namespace mt
} // namespace gaia
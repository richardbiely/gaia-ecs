#pragma once

#include "../config/config.h"

#include <functional>
#include <inttypes.h>
#include <mutex>

#include "../cnt/ilist.h"
#include "../config/profiler.h"
#include "../core/span.h"
#include "jobcommon.h"
#include "jobhandle.h"

namespace gaia {
	namespace mt {
		enum JobInternalState : uint32_t {
			DEP_BITS_START = 0,
			DEP_BITS = 27,
			DEP_BITS_MASK = (uint32_t)((1llu << DEP_BITS) - 1),

			STATE_BITS_START = DEP_BITS_START + DEP_BITS,
			STATE_BITS = 4,
			STATE_BITS_MASK = (uint32_t)(((1llu << STATE_BITS) - 1) << STATE_BITS_START),

			PRIORITY_BITS_START = STATE_BITS_START + STATE_BITS,
			PRIORITY_BITS = 1,
			PRIORITY_BIT_MASK = (uint32_t)(((1llu << PRIORITY_BITS) - 1) << PRIORITY_BITS_START),

			// STATE

			//! Submitted
			Submitted = 0x01 << STATE_BITS_START,
			//! Being executed
			Running = 0x02 << STATE_BITS_START,
			//! Finished executing
			Done = 0x04 << STATE_BITS_START,
			//! Job released. Not to be used anymore
			Released = 0x08 << STATE_BITS_START,
			//! Scheduled or being executed
			Busy = Submitted | Running,

			// PRIORITY

			LowPriority = (uint32_t)(1llu << PRIORITY_BITS_START)
		};

		struct JobContainer: cnt::ilist_item {
			uint32_t dependencyIdx;
			uint32_t state;
			std::function<void()> func;

			JobContainer() = default;

			GAIA_NODISCARD static JobContainer create(uint32_t index, uint32_t generation, void* pCtx) {
				auto* ctx = (JobAllocCtx*)pCtx;

				JobContainer jc{};
				jc.idx = index;
				jc.gen = generation;
				jc.state = ctx->priority == JobPriority::High ? 0 : JobInternalState::LowPriority;
				// The rest of the values are set later on:
				//   jc.dependencyIdx
				//   jc.func
				return jc;
			}

			GAIA_NODISCARD static JobHandle handle(const JobContainer& jc) {
				return JobHandle(jc.idx, jc.gen, (jc.state & JobInternalState::LowPriority) != 0);
			}
		};

		using DepHandle = JobHandle;

		struct JobDependency: cnt::ilist_item {
			uint32_t dependencyIdxNext;
			JobHandle dependsOn;

			JobDependency() = default;

			GAIA_NODISCARD static JobDependency create(uint32_t index, uint32_t generation, [[maybe_unused]] void* pCtx) {
				JobDependency jd{};
				jd.idx = index;
				jd.gen = generation;
				// The rest of the values are set later on:
				//   jc.dependencyIdxNext
				//   jc.dependsOn
				return jd;
			}

			GAIA_NODISCARD static DepHandle handle(const JobDependency& jd) {
				return DepHandle(
						jd.idx, jd.gen,
						// It does not matter what value we set for priority on dependencies,
						// it is always going to be ignored.
						1);
			}
		};

		class JobManager {
			GAIA_PROF_MUTEX(std::mutex, m_mtx);

			//! Implicit list of jobs
			cnt::ilist<JobContainer, JobHandle> m_jobs;
			//! Implicit list of job dependencies
			cnt::ilist<JobDependency, DepHandle> m_deps;

		public:
			//! Cleans up any job allocations and dependencies associated with \param jobHandle
			void wait(JobHandle jobHandle) {
				// We need to release any dependencies related to this job
				auto& job = m_jobs[jobHandle.id()];

				// No need to wait for a dead job
				if ((job.state & JobInternalState::Released) != 0)
					return;

				uint32_t depIdx = job.dependencyIdx;
				while (depIdx != BadIndex) {
					auto& dep = m_deps[depIdx];
					const uint32_t depIdxNext = dep.dependencyIdxNext;
					// const uint32_t depPrio = dep.;
					wait(dep.dependsOn);
					free_dep(DepHandle{depIdx, 0, jobHandle.prio()});
					depIdx = depIdxNext;
				}

				// Deallocate the job itself
				free_job(jobHandle);
			}

			//! Allocates a new job container identified by a unique JobHandle.
			//! \return JobHandle
			//! \warning Must be used from the main thread.
			GAIA_NODISCARD JobHandle alloc_job(const Job& job) {
				JobAllocCtx ctx{job.priority};

				auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, m_mtx);
				std::scoped_lock lock(mtx);

				auto handle = m_jobs.alloc(&ctx);
				auto& j = m_jobs[handle.id()];
				GAIA_ASSERT(
						// No state yet
						((j.state & JobInternalState::STATE_BITS_MASK) == 0) ||
						// Released already
						((j.state & JobInternalState::Released) != 0));
				j.dependencyIdx = BadIndex;
				j.state = ctx.priority == JobPriority::High ? 0 : JobInternalState::LowPriority;
				j.func = job.func;
				return handle;
			}

			//! Invalidates \param jobHandle by resetting its index in the job pool.
			//! Every time a job is deallocated its generation is increased by one.
			//! \warning Must be used from the main thread.
			void free_job(JobHandle jobHandle) {
				// No need to lock. Called from the main thread only when the job has finished already.
				// --> auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, m_mtx);
				// --> std::scoped_lock lock(mtx);
				auto& job = m_jobs.free(jobHandle);
				job.state = (job.state & (~JobInternalState::STATE_BITS_MASK)) | JobInternalState::Released;
				GAIA_ASSERT((job.state & JobInternalState::STATE_BITS_MASK) == JobInternalState::Released);
			}

			//! Allocates a new dependency identified by a unique DepHandle.
			//! \return DepHandle
			//! \warning Must be used from the main thread.
			GAIA_NODISCARD DepHandle alloc_dep() {
				JobAllocCtx dummyCtx{};
				return m_deps.alloc(&dummyCtx);
			}

			//! Invalidates \param depHandle by resetting its index in the dependency pool.
			//! Every time a dependency is deallocated its generation is increased by one.
			//! \warning Must be used from the main thread.
			void free_dep(DepHandle depHandle) {
				m_deps.free(depHandle);
			}

			//! Resets the job pool.
			void reset() {
				// No need to lock. Called from the main thread only when all jobs have finished already.
				// --> auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, m_mtx);
				// --> std::scoped_lock lock(mtx);
				m_jobs.clear();
				m_deps.clear();
			}

			void run(JobHandle jobHandle) {
				std::function<void()> func;

				{
					auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, m_mtx);
					std::scoped_lock lock(mtx);

					auto& job = m_jobs[jobHandle.id()];
					job.state = (job.state & (~JobInternalState::STATE_BITS_MASK)) | JobInternalState::Running;
					GAIA_ASSERT((job.state & JobInternalState::STATE_BITS_MASK) == JobInternalState::Running);
					func = job.func;
				}

				if (func.operator bool())
					func();

				{
					auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, m_mtx);
					std::scoped_lock lock(mtx);

					auto& job = m_jobs[jobHandle.id()];
					job.state = (job.state & (~JobInternalState::STATE_BITS_MASK)) | JobInternalState::Done;
					GAIA_ASSERT((job.state & JobInternalState::STATE_BITS_MASK) == JobInternalState::Done);
				}
			}

			//! Evaluates job dependencies.
			//! \return True if job dependencies are met. False otherwise
			GAIA_NODISCARD bool handle_deps(JobHandle jobHandle) {
				GAIA_PROF_SCOPE(JobManager::handle_deps);

				auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, m_mtx);
				std::scoped_lock lock(mtx);

				auto& job = m_jobs[jobHandle.id()];
				if (job.dependencyIdx == BadIndex)
					return true;

				uint32_t depsId = job.dependencyIdx;
				{
					// Iterate over all dependencies.
					// The first busy dependency breaks the loop. At this point we also update
					// the initial dependency index because we know all previous dependencies
					// have already finished and there's no need to check them.
					do {
						JobDependency dep = m_deps[depsId];
						if (!done(dep.dependsOn)) {
							m_jobs[jobHandle.id()].dependencyIdx = depsId;
							return false;
						}

						depsId = dep.dependencyIdxNext;
					} while (depsId != BadIndex);
				}

				// No need to update the index because once we return true we execute the job.
				// --> job.dependencyIdx = JobHandleInvalid.idx;
				return true;
			}

			//! Makes \param jobHandle depend on \param dependsOn.
			//! This means \param jobHandle will run only after \param dependsOn finishes.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void dep(JobHandle jobHandle, JobHandle dependsOn) {
				GAIA_PROF_SCOPE(JobManager::dep);

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(jobHandle != dependsOn);
				GAIA_ASSERT(!busy(jobHandle));
				GAIA_ASSERT(!busy(dependsOn));
#endif

				auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, m_mtx);
				std::scoped_lock lock(mtx);

				auto depHandle = alloc_dep();
				auto& dep = m_deps[depHandle.id()];
				dep.dependsOn = dependsOn;

				auto& job = m_jobs[jobHandle.id()];
				if (job.dependencyIdx == BadIndex)
					// First time adding a dependency to this job. Point it to the first allocated handle
					dep.dependencyIdxNext = BadIndex;
				else
					// We have existing dependencies. Point the last known one to the first allocated handle
					dep.dependencyIdxNext = job.dependencyIdx;

				job.dependencyIdx = depHandle.id();
			}

			//! Makes \param jobHandle depend on the jobs listed in \param dependsOnSpan.
			//! This means \param jobHandle will run only after all \param dependsOnSpan jobs finish.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void dep(JobHandle jobHandle, std::span<const JobHandle> dependsOnSpan) {
				if (dependsOnSpan.empty())
					return;

				GAIA_PROF_SCOPE(JobManager::depMany);

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(!busy(jobHandle));
				for (auto dependsOn: dependsOnSpan) {
					GAIA_ASSERT(jobHandle != dependsOn);
					GAIA_ASSERT(!busy(dependsOn));
				}
#endif

				auto& mtx = GAIA_PROF_EXTRACT_MUTEX(std::mutex, m_mtx);
				std::scoped_lock lock(mtx);

				auto& job = m_jobs[jobHandle.id()];
				for (auto dependsOn: dependsOnSpan) {
					auto depHandle = alloc_dep();
					auto& dep = m_deps[depHandle.id()];
					dep.dependsOn = dependsOn;

					if (job.dependencyIdx == BadIndex)
						// First time adding a dependency to this job. Point it to the first allocated handle
						dep.dependencyIdxNext = BadIndex;
					else
						// We have existing dependencies. Point the last known one to the first allocated handle
						dep.dependencyIdxNext = job.dependencyIdx;

					job.dependencyIdx = depHandle.id();
				}
			}

			void submit(JobHandle jobHandle) {
				auto& job = m_jobs[jobHandle.id()];
				GAIA_ASSERT((job.state & JobInternalState::STATE_BITS_MASK) < JobInternalState::Submitted);
				job.state = (job.state & (~JobInternalState::STATE_BITS_MASK)) | JobInternalState::Submitted;
				GAIA_ASSERT((job.state & JobInternalState::STATE_BITS_MASK) == JobInternalState::Submitted);
			}

			void resubmit(JobHandle jobHandle) {
				auto& job = m_jobs[jobHandle.id()];
				GAIA_ASSERT((job.state & JobInternalState::STATE_BITS_MASK) <= JobInternalState::Submitted);
				job.state = (job.state & (~JobInternalState::STATE_BITS_MASK)) | JobInternalState::Submitted;
				GAIA_ASSERT((job.state & JobInternalState::STATE_BITS_MASK) == JobInternalState::Submitted);
			}

			GAIA_NODISCARD bool busy(JobHandle jobHandle) const {
				const auto& job = m_jobs[jobHandle.id()];
				return (job.state & JobInternalState::Busy) != 0;
			}

			GAIA_NODISCARD bool done(JobHandle jobHandle) const {
				const auto& job = m_jobs[jobHandle.id()];
				return (job.state & JobInternalState::Done) != 0;
			}
		};
	} // namespace mt
} // namespace gaia
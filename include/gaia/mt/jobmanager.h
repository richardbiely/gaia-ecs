#pragma once

#include "../config/config.h"

#include <functional>
#include <inttypes.h>
#include <mutex>

#include "../cnt/darray.h"
#include "../cnt/ilist.h"
#include "../cnt/sarray.h"
#include "../config/profiler.h"
#include "../core/span.h"
#include "jobcommon.h"
#include "jobhandle.h"

namespace gaia {
	namespace mt {
		enum class JobInternalState : uint32_t {
			//! No scheduled
			Idle = 0,
			//! Scheduled
			Submitted = 0x01,
			//! Being executed
			Running = 0x02,
			//! Finished executing
			Done = 0x04,
			//! Job released. Not to be used anymore
			Released = 0x08,

			//! Scheduled or being executed
			Busy = Submitted | Running,
		};

		struct JobContainer: cnt::ilist_item {
			uint32_t dependencyIdx;
			JobPriority priority : 1;
			JobInternalState state : 31;
			std::function<void()> func;

			JobContainer() = default;

			static JobContainer create(uint32_t index, uint32_t generation, void* pCtx) {
				auto* ctx = (JobAllocCtx*)pCtx;

				JobContainer jc{};
				jc.idx = index;
				jc.gen = generation;
				jc.priority = ctx->priority;
				jc.state = JobInternalState::Idle;
				// The rest of the values are set later on:
				//   jc.dependencyIdx
				//   jc.func
				return jc;
			}

			static JobHandle create(const JobContainer& jc) {
				return JobHandle(jc.idx, jc.gen, (uint32_t)jc.priority);
			}
		};

		using DepHandle = JobHandle;

		struct JobDependency: cnt::ilist_item {
			uint32_t dependencyIdxNext;
			JobHandle dependsOn;

			JobDependency() = default;

			static JobDependency create(uint32_t index, uint32_t generation, [[maybe_unused]] void* pCtx) {
				JobDependency jd{};
				jd.idx = index;
				jd.gen = generation;
				// The rest of the values are set later on:
				//   jc.dependencyIdxNext
				//   jc.dependsOn
				return jd;
			}

			static DepHandle create(const JobDependency& jd) {
				return DepHandle(
						jd.idx, jd.gen,
						// It does not matter what value we set for priority on dependencies,
						// it is always going to be ignored.
						1);
			}
		};

		class JobManager {
			std::mutex m_jobsLock;
			//! Implicit list of jobs
			cnt::ilist<JobContainer, JobHandle> m_jobs;

			std::mutex m_depsLock;
			//! List of job dependencies
			cnt::ilist<JobDependency, DepHandle> m_deps;

		public:
			//! Cleans up any job allocations and dependicies associated with \param jobHandle
			void wait(JobHandle jobHandle) {
				// We need to release any dependencies related to this job
				auto& job = m_jobs[jobHandle.id()];

				if (job.state == JobInternalState::Released)
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
				std::scoped_lock<std::mutex> lock(m_jobsLock);
				auto handle = m_jobs.alloc(&ctx);
				auto& j = m_jobs[handle.id()];
				GAIA_ASSERT(j.state == JobInternalState::Idle || j.state == JobInternalState::Released);
				j.dependencyIdx = BadIndex;
				j.state = JobInternalState::Idle;
				j.func = job.func;
				return handle;
			}

			//! Invalidates \param jobHandle by resetting its index in the job pool.
			//! Everytime a job is deallocated its generation is increased by one.
			//! \warning Must be used from the main thread.
			void free_job(JobHandle jobHandle) {
				// No need to lock. Called from the main thread only when the job has finished already.
				// --> std::scoped_lock<std::mutex> lock(m_jobsLock);
				auto& job = m_jobs.free(jobHandle);
				job.state = JobInternalState::Released;
			}

			//! Allocates a new dependency identified by a unique DepHandle.
			//! \return DepHandle
			//! \warning Must be used from the main thread.
			GAIA_NODISCARD DepHandle alloc_dep() {
				JobAllocCtx dummyCtx{};
				return m_deps.alloc(&dummyCtx);
			}

			//! Invalidates \param depHandle by resetting its index in the dependency pool.
			//! Everytime a dependency is deallocated its generation is increased by one.
			//! \warning Must be used from the main thread.
			void free_dep(DepHandle depHandle) {
				m_deps.free(depHandle);
			}

			//! Resets the job pool.
			void reset() {
				{
					// No need to lock. Called from the main thread only when all jobs have finished already.
					// --> std::scoped_lock<std::mutex> lock(m_jobsLock);
					m_jobs.clear();
				}
				{
					// No need to lock. Called from the main thread only when all jobs must have ended already.
					// --> std::scoped_lock<std::mutex> lock(m_depsLock);
					m_deps.clear();
				}
			}

			void run(JobHandle jobHandle) {
				std::function<void()> func;

				{
					std::scoped_lock<std::mutex> lock(m_jobsLock);
					auto& job = m_jobs[jobHandle.id()];
					job.state = JobInternalState::Running;
					func = job.func;
				}
				if (func.operator bool())
					func();
				{
					std::scoped_lock<std::mutex> lock(m_jobsLock);
					auto& job = m_jobs[jobHandle.id()];
					job.state = JobInternalState::Done;
				}
			}

			//! Evaluates job dependencies.
			//! \return True if job dependencies are met. False otherwise
			GAIA_NODISCARD bool handle_deps(JobHandle jobHandle) {
				GAIA_PROF_SCOPE(JobManager::handle_deps);
				std::scoped_lock<std::mutex> lockJobs(m_jobsLock);
				auto& job = m_jobs[jobHandle.id()];
				if (job.dependencyIdx == BadIndex)
					return true;

				uint32_t depsId = job.dependencyIdx;
				{
					std::scoped_lock<std::mutex> lockDeps(m_depsLock);

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
				std::scoped_lock<std::mutex> lockJobs(m_jobsLock);
				auto& job = m_jobs[jobHandle.id()];

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(jobHandle != dependsOn);
				GAIA_ASSERT(!busy(jobHandle));
				GAIA_ASSERT(!busy(dependsOn));
#endif

				{
					GAIA_PROF_SCOPE(JobManager::dep);
					std::scoped_lock<std::mutex> lockDeps(m_depsLock);

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

			//! Makes \param jobHandle depend on the jobs listed in \param dependsOnSpan.
			//! This means \param jobHandle will run only after all \param dependsOnSpan jobs finish.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void dep(JobHandle jobHandle, std::span<const JobHandle> dependsOnSpan) {
				if (dependsOnSpan.empty())
					return;

				auto& job = m_jobs[jobHandle.id()];

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(!busy(jobHandle));
				for (auto dependsOn: dependsOnSpan) {
					GAIA_ASSERT(jobHandle != dependsOn);
					GAIA_ASSERT(!busy(dependsOn));
				}
#endif

				GAIA_PROF_SCOPE(JobManager::deps);
				std::scoped_lock<std::mutex> lockJobs(m_jobsLock);
				{
					std::scoped_lock<std::mutex> lockDeps(m_depsLock);

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
			}

			void submit(JobHandle jobHandle) {
				auto& job = m_jobs[jobHandle.id()];
				GAIA_ASSERT(job.state < JobInternalState::Submitted);
				job.state = JobInternalState::Submitted;
			}

			void resubmit(JobHandle jobHandle) {
				auto& job = m_jobs[jobHandle.id()];
				GAIA_ASSERT(job.state <= JobInternalState::Submitted);
				job.state = JobInternalState::Submitted;
			}

			GAIA_NODISCARD bool busy(JobHandle jobHandle) const {
				const auto& job = m_jobs[jobHandle.id()];
				return ((uint32_t)job.state & (uint32_t)JobInternalState::Busy) != 0;
			}

			GAIA_NODISCARD bool done(JobHandle jobHandle) const {
				const auto& job = m_jobs[jobHandle.id()];
				return ((uint32_t)job.state & (uint32_t)JobInternalState::Done) != 0;
			}
		};
	} // namespace mt
} // namespace gaia
#pragma once

#include <functional>
#include <inttypes.h>
#include <mutex>

#include "../config/config_core.h"
#include "../containers/darray.h"
#include "../containers/sarray.h"
#include "../utils/span.h"
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

			//! Scheduled or being executed
			Busy = Submitted | Running,
		};

		struct ImplicitListItem {
			uint32_t idx;
			uint32_t gen;
		};

		struct JobContainer: ImplicitListItem {
			uint32_t dependencyIdx;
			JobInternalState state;
			std::function<void()> func;
		};

		struct JobDependency: ImplicitListItem {
			uint32_t dependencyIdxNext;
			JobHandle dependsOn;
		};

		using DepHandle = JobHandle;

		class JobManager {
			std::mutex m_jobsLock;
			//! Implicit list of jobs
			containers::darray<JobContainer> m_jobs;
			//! Index of the next job to recycle
			uint32_t m_nextFreeJob = (uint32_t)-1;
			//! Number of entites to recycle
			uint32_t m_freeJobs = 0;

			std::mutex m_depsLock;
			//! List of job dependencies
			containers::darray<JobDependency> m_deps;
			//! Index of the next depenedency to recycle
			uint32_t m_nextFreeDep = (uint32_t)-1;
			//! Number of entites to recycle
			uint32_t m_freeDeps = 0;

		public:
			//! Cleans up any job allocations and dependicies associated with \param jobHandle
			void Complete(JobHandle jobHandle) {
				// We need to release any dependencies related to this job
				auto& job = m_jobs[jobHandle.id()];

				uint32_t depIdx = job.dependencyIdx;
				while (depIdx != (uint32_t)-1) {
					auto& dep = m_deps[depIdx];
					GAIA_ASSERT(dep.idx == depIdx);
					const uint32_t depIdxNext = dep.dependencyIdxNext;
					Complete(dep.dependsOn);
					DeallocateDependency(DepHandle{depIdx, 0});
					depIdx = depIdxNext;
				}

				// Deallocate the job itself
				DeallocateJob(jobHandle);
			}

			//! Allocates a new job container identified by a unique JobHandle.
			//! \return JobHandle
			//! \warning Must be used from the main thread.
			GAIA_NODISCARD JobHandle AllocateJob(const Job& job) {
				if GAIA_UNLIKELY (!m_freeJobs) {
					std::scoped_lock<std::mutex> lock(m_jobsLock);

					const auto jobCnt = (uint32_t)m_jobs.size();
					// We don't want to go out of range for new jobs
					GAIA_ASSERT(jobCnt < JobHandle::IdMask && "Trying to allocate too many jobs!");

					m_jobs.emplace_back(jobCnt, 0U, (uint32_t)-1, JobInternalState::Idle, job.func);
					return {(JobId)jobCnt, 0U};
				}

				// Make sure the list is not broken
				GAIA_ASSERT(m_nextFreeJob < (uint32_t)m_jobs.size() && "Jobs recycle list broken!");

				--m_freeJobs;
				const auto index = m_nextFreeJob;
				auto& j = m_jobs[m_nextFreeJob];
				m_nextFreeJob = j.idx;
				j.dependencyIdx = (uint32_t)-1;
				j.state = JobInternalState::Idle;
				j.func = job.func;
				return {index, m_jobs[index].gen};
			}

			//! Invalidates \param jobHandle by resetting its index in the job pool.
			//! Everytime a job is deallocated its generation is increased by one.
			//! \warning Must be used from the main thread.
			void DeallocateJob(JobHandle jobHandle) {
				// No need to lock. Called from the main thread only when the job has finished already.
				// --> std::scoped_lock<std::mutex> lock(m_jobsLock);

				auto& jobContainer = m_jobs[jobHandle.id()];

				// Nothing to deallocate when the job is unused
				if (jobContainer.idx == JobHandle::IdMask)
					return;

				// New generation
				const auto gen = ++jobContainer.gen;

				// Update our implicit list
				if GAIA_UNLIKELY (m_freeJobs == 0) {
					m_nextFreeJob = jobHandle.id();
					jobContainer.idx = JobHandle::IdMask;
					jobContainer.gen = gen;
				} else {
					jobContainer.idx = m_nextFreeJob;
					jobContainer.gen = gen;
					m_nextFreeJob = jobHandle.id();
				}
				++m_freeJobs;
			}

			//! Allocates a new dependency identified by a unique DepHandle.
			//! \return DepHandle
			//! \warning Must be used from the main thread.
			GAIA_NODISCARD DepHandle AllocateDependency() {
				if GAIA_UNLIKELY (!m_freeDeps) {
					const auto depCnt = (uint32_t)m_deps.size();
					// We don't want to go out of range for new dependencies
					GAIA_ASSERT(depCnt < JobHandle::IdMask && "Trying to allocate too many dependencies!");

					m_deps.emplace_back(depCnt, 0U);
					return {(JobId)depCnt, 0U};
				}

				// Make sure the list is not broken
				GAIA_ASSERT(m_nextFreeDep < (uint32_t)m_deps.size() && "Dependency recycle list broken!");

				--m_freeDeps;
				const auto index = m_nextFreeDep;
				auto& d = m_deps[m_nextFreeDep];
				m_nextFreeDep = d.idx;
				return {index, m_deps[index].gen};
			}

			//! Invalidates \param depHandle by resetting its index in the dependency pool.
			//! Everytime a dependency is deallocated its generation is increased by one.
			//! \warning Must be used from the main thread.
			void DeallocateDependency(DepHandle depHandle) {
				auto& dep = m_deps[depHandle.id()];

				// New generation
				const auto gen = ++dep.gen;

				// Update our implicit list
				if GAIA_UNLIKELY (m_freeDeps == 0) {
					m_nextFreeDep = depHandle.id();
					dep.idx = DepHandle::IdMask;
					dep.gen = gen;
				} else {
					dep.idx = m_nextFreeDep;
					dep.gen = gen;
					m_nextFreeDep = depHandle.id();
				}
				++m_freeDeps;
			}

			//! Resets the job pool.
			void Reset() {
				{
					// No need to lock. Called from the main thread only when all jobs have finished already.
					// --> std::scoped_lock<std::mutex> lock(m_jobsLock);
					m_jobs.clear();
					m_nextFreeJob = (uint32_t)-1;
					m_freeJobs = 0;
				}
				{
					// No need to lock. Called from the main thread only when all jobs must have ended already.
					// --> std::scoped_lock<std::mutex> lock(m_depsLock);
					m_deps.clear();
					m_nextFreeDep = (uint32_t)-1;
					m_freeDeps = 0;
				}
			}

			void Run(JobHandle jobHandle) {
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
			GAIA_NODISCARD bool HandleDependencies(JobHandle jobHandle) {
				uint32_t depsId{};

				{
					std::scoped_lock<std::mutex> lock(m_jobsLock);
					auto& job = m_jobs[jobHandle.id()];
					if (job.dependencyIdx == (uint32_t)-1)
						return true;
					depsId = job.dependencyIdx;
				}

				{
					// Iterate over all dependencies.
					// The first busy dependency breaks the loop. At this point we also update
					// the initial dependency index because we know all previous dependencies
					// have already finished and there's no need to check them.
					do {
						JobDependency dep;
						{
							std::scoped_lock<std::mutex> lock(m_depsLock);
							dep = m_deps[depsId];
						}

						{
							std::scoped_lock<std::mutex> lock(m_jobsLock);
							if (IsBusy(dep.dependsOn)) {
								m_jobs[jobHandle.id()].dependencyIdx = depsId;
								return false;
							}
						}

						depsId = dep.dependencyIdxNext;
					} while (depsId != (uint32_t)-1);
				}

				// No need to update the index because once we return true we execute the job.
				// --> job.dependencyIdx = JobHandleInvalid.idx;
				return true;
			}

			//! Makes \param jobHandle depend on \param dependsOn.
			//! This means \param jobHandle will run only after \param dependsOn finishes.
			//! \warning Must be used from the main thread.
			//! \warning Needs to be called before any of the listed jobs are scheduled.
			void AddDependency(JobHandle jobHandle, JobHandle dependsOn) {
				auto& job = m_jobs[jobHandle.id()];

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(jobHandle != dependsOn);
				GAIA_ASSERT(!IsBusy(jobHandle));
				GAIA_ASSERT(!IsBusy(dependsOn));
#endif

				std::scoped_lock<std::mutex> lockJobs(m_jobsLock);
				{
					std::scoped_lock<std::mutex> lockDeps(m_depsLock);

					auto depHandle = AllocateDependency();
					auto& dep = m_deps[depHandle.id()];
					dep.dependsOn = dependsOn;

					if (job.dependencyIdx == (uint32_t)-1)
						// First time adding a dependency to this job. Point it to the first allocated handle
						dep.dependencyIdxNext = (uint32_t)-1;
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
			void AddDependencies(JobHandle jobHandle, std::span<const JobHandle> dependsOnSpan) {
				if (dependsOnSpan.empty())
					return;

				auto& job = m_jobs[jobHandle.id()];

#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(!IsBusy(jobHandle));
				for (auto dependsOn: dependsOnSpan) {
					GAIA_ASSERT(jobHandle != dependsOn);
					GAIA_ASSERT(!IsBusy(dependsOn));
				}
#endif

				std::scoped_lock<std::mutex> lockJobs(m_jobsLock);
				{
					std::scoped_lock<std::mutex> lockDeps(m_depsLock);

					for (uint32_t i = 0; i < dependsOnSpan.size(); ++i) {
						auto depHandle = AllocateDependency();
						auto& dep = m_deps[depHandle.id()];
						dep.dependsOn = dependsOnSpan[i];

						if (job.dependencyIdx == (uint32_t)-1)
							// First time adding a dependency to this job. Point it to the first allocated handle
							dep.dependencyIdxNext = (uint32_t)-1;
						else
							// We have existing dependencies. Point the last known one to the first allocated handle
							dep.dependencyIdxNext = job.dependencyIdx;

						job.dependencyIdx = depHandle.id();
					}
				}
			}

			void Submit(JobHandle jobHandle) {
				auto& job = m_jobs[jobHandle.id()];
				GAIA_ASSERT(job.state < JobInternalState::Submitted);
				job.state = JobInternalState::Submitted;
			}

			void ReSubmit(JobHandle jobHandle) {
				auto& job = m_jobs[jobHandle.id()];
				GAIA_ASSERT(job.state <= JobInternalState::Submitted);
				job.state = JobInternalState::Submitted;
			}

			GAIA_NODISCARD bool IsBusy(JobHandle jobHandle) const {
				const auto& job = m_jobs[jobHandle.id()];
				return ((uint32_t)job.state & (uint32_t)JobInternalState::Busy) != 0;
			}
		};
	} // namespace mt
} // namespace gaia
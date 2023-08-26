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

// Heavily WIP and unusable currently
#define GAIA_ENABLE_JOB_DEPENDENCIES 0

namespace gaia {
	namespace mt {
		enum class JobInternalState : uint32_t { Idle, Submitted, Done };

		struct JobContainer {
			uint32_t idx;
			uint32_t gen;
			uint32_t dependencyIdx;
			JobInternalState state;
			std::function<void()> func;
		};

		struct JobDependency {
			uint32_t dependencyIdxNext;
			JobHandle dependsOn;
		};

		class JobManager {
			std::mutex m_jobsLock;
			//! Implicit list of jobs
			containers::darray<JobContainer> m_jobs;
			//! Index of the next entity to recycle
			uint32_t m_nextFreeJob = (uint32_t)-1;
			//! Number of entites to recycle
			uint32_t m_freeJobs = 0;

			std::mutex m_depsLock;
			containers::darray<JobDependency> m_deps;

		public:
			/*!
			Allocates a new job container identified by a unique JobHandle.
			\return JobHandle
			*/
			GAIA_NODISCARD JobHandle AllocateJob(const Job& job) {
				std::scoped_lock<std::mutex> lock(m_jobsLock);

				if GAIA_UNLIKELY (!m_freeJobs) {
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
#if GAIA_ENABLE_JOB_DEPENDENCIES
				j.dependencyIdx = (uint32_t)-1;
#endif
				j.state = JobInternalState::Idle;
				j.func = job.func;
				return {index, m_jobs[index].gen};
			}

			/*!
			Deallocates a new entity.
			\param jobHandle Job to delete
			*/
			void DeallocateJob(JobHandle jobHandle) {
				std::scoped_lock<std::mutex> lock(m_jobsLock);

				auto& jobContainer = m_jobs[jobHandle.id()];

				// New generation
				const auto gen = ++jobContainer.gen;

				// Update our implicit list
				if GAIA_UNLIKELY (!m_freeJobs) {
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

			void DeallocateAllJobs() {
				{
					std::scoped_lock<std::mutex> lock(m_jobsLock);

					GAIA_ASSERT(m_nextFreeJob == (uint32_t)-1);
					GAIA_ASSERT(m_freeJobs == 0);
					m_jobs.clear();
					m_nextFreeJob = (uint32_t)-1;
					m_freeJobs = 0;
				}
				{
					std::scoped_lock<std::mutex> lock(m_depsLock);
					m_deps.clear();
				}
			}

			void Run(JobHandle jobHandle) {
				std::function<void()> func;

				{
					std::scoped_lock<std::mutex> lock(m_jobsLock);
					auto& job = m_jobs[jobHandle.id()];
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

#if GAIA_ENABLE_JOB_DEPENDENCIES
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

			//!
			//! \warning Must be used form the main thread.
			void AddDependency(JobHandle jobHandle, JobHandle dependsOn) {
	#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(jobHandle != dependsOn);
				GAIA_ASSERT(m_jobs[jobHandle.id()].state < JobInternalState::Submitted);
				GAIA_ASSERT(m_jobs[dependsOn.id()].state < JobInternalState::Submitted);
				if (jobHandle == dependsOn)
					return;
				if (m_jobs[jobHandle.id()].state >= JobInternalState::Submitted)
					return;
				if (m_jobs[dependsOn.id()].state >= JobInternalState::Submitted)
					return;
	#endif

				auto& job = m_jobs[jobHandle.id()];

				// First time adding a dependency.
				if (job.dependencyIdx == (uint32_t)-1) {
					std::scoped_lock<std::mutex> lock(m_depsLock);
					job.dependencyIdx = (uint32_t)m_deps.size();
					m_deps.emplace_back((uint32_t)-1, dependsOn);
				} else {
					std::scoped_lock<std::mutex> lock(m_depsLock);
					auto& dep = m_deps[job.dependencyIdx];
					dep.dependencyIdxNext = (uint32_t)m_deps.size();
					m_deps.emplace_back((uint32_t)-1, dependsOn);
				}
			}

			void AddDependencies(JobHandle jobHandle, std::span<const JobHandle> dependsOnSpan) {
	#if GAIA_ASSERT_ENABLED
				GAIA_ASSERT(m_jobs[jobHandle.id()].state < JobInternalState::Submitted);
				if (m_jobs[jobHandle.id()].state >= JobInternalState::Submitted)
					return;

				for (auto h: dependsOnSpan) {
					GAIA_ASSERT(jobHandle != h);
					GAIA_ASSERT(m_jobs[h.id()].state < JobInternalState::Submitted);

					if (jobHandle == h)
						return;
					if (m_jobs[h.id()].state >= JobInternalState::Submitted)
						return;
				}
	#endif

				auto& job = m_jobs[jobHandle.id()];

				// First time adding a dependency.
				if (job.dependencyIdx == (uint32_t)-1) {
					std::scoped_lock<std::mutex> lock(m_depsLock);
					job.dependencyIdx = (uint32_t)m_deps.size();
					for (auto h: dependsOnSpan)
						m_deps.emplace_back((uint32_t)m_deps.size(), h);
					m_deps.back().dependencyIdxNext = (uint32_t)-1;
				} else {
					std::scoped_lock<std::mutex> lock(m_depsLock);
					for (auto h: dependsOnSpan) {
						auto& dep = m_deps[job.dependencyIdx];
						const uint32_t depNext = dep.dependencyIdxNext;
						dep.dependencyIdxNext = job.dependencyIdx;
						m_deps.emplace_back(depNext, h);
					}
				}
			}
#endif

			void Submit(JobHandle jobHandle) {
				auto& job = m_jobs[jobHandle.id()];
				GAIA_ASSERT(job.state <= JobInternalState::Submitted);
				job.state = JobInternalState::Submitted;
			}

			GAIA_NODISCARD bool IsBusy(JobHandle jobHandle) const {
				const auto& job = m_jobs[jobHandle.id()];
				return job.state == JobInternalState::Submitted;
			}
		};
	} // namespace mt
} // namespace gaia
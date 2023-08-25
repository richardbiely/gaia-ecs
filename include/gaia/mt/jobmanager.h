#pragma once

#include <functional>
#include <inttypes.h>
#include <mutex>

#include "../config/config_core.h"
#include "../containers/darray.h"
#include "../containers/sarray.h"
#include "gaia/containers/impl/sarray_impl.h"
#include "jobcommon.h"

// Heavily WIP and unusable currently
#define GAIA_ENABLE_JOB_DEPENDENCIES 0

namespace gaia {
	namespace mt {
		struct JobInternal {
			std::function<void()> func;
#if GAIA_ENABLE_JOB_DEPENDENCIES
			uint32_t dependencyCount;
			JobHandle jobHandleNext;
#endif
			std::atomic_uint32_t leftToFinish;
		};

#if GAIA_ENABLE_JOB_DEPENDENCIES
		struct JobDependency {
			uint32_t idxNext;
			JobHandle jobHandle;
			JobHandle dependsOn;
		};
#endif

		struct JobManager {
			static constexpr uint32_t N = 1 << 12;
			static constexpr uint32_t MASK = N - 1;

		private:
			containers::sarray<JobInternal, N> m_jobs;
			uint32_t m_jobsAllocated = 0;

#if GAIA_ENABLE_JOB_DEPENDENCIES
			std::mutex m_depsLock;
			containers::darray<JobDependency> m_deps;
#endif

		public:
			GAIA_NODISCARD JobHandle AllocateJob(const Job& job) {
				const uint32_t idx = (m_jobsAllocated++) & MASK;
				auto& jobDesc = m_jobs[idx];

				// Make sure there are no pending tasks associated with this job
				GAIA_ASSERT(jobDesc.leftToFinish == 0);

				jobDesc.func = job.func;
#if GAIA_ENABLE_JOB_DEPENDENCIES
				jobDesc.jobHandleNext = JobHandleInvalid;
#endif
				jobDesc.leftToFinish = 1;
				return {idx};
			}

#if GAIA_ENABLE_JOB_DEPENDENCIES
			GAIA_NODISCARD JobHandle AllocateJob(const Job& job, JobHandle dependency) {
				const uint32_t idx = (m_jobsAllocated++) & MASK;
				auto& jobDesc = m_jobs[idx];

				// Make sure there are no pending tasks associated with this job
				GAIA_ASSERT(jobDesc.leftToFinish == 0);

				jobDesc.func = job.func;
				jobDesc.jobHandleNext = dependency;
				jobDesc.leftToFinish = 2;
				return {idx};
			}
#endif

			void Run(JobHandle jobHandle) {
				auto& job = m_jobs[jobHandle.idx];
				job.func();
				--job.leftToFinish;
			}

#if GAIA_ENABLE_JOB_DEPENDENCIES
			GAIA_NODISCARD bool HandleDependencies(JobHandle jobHandle) {

				const auto& job = m_jobs[jobHandle.idx];
				if (job.jobHandleNext == JobHandleInvalid)
					return true;

				std::lock_guard<std::mutex> guard(m_depsLock);
				auto& dep = m_deps[job.jobHandleNext.idx];
				(void)dep;

				return false;
			}

			void AddDependency(JobHandle jobHandle, JobHandle dependsOn) {
				GAIA_ASSERT(jobHandle != dependsOn);
				if (jobHandle == dependsOn)
					return;

				const auto& job = m_jobs[jobHandle.idx];

				if (job.jobHandleNext == JobHandleInvalid) {
					std::lock_guard<std::mutex> guard(m_depsLock);
					m_deps.emplace_back((uint32_t)m_deps.size(), jobHandle, dependsOn);
				} else {
					std::lock_guard<std::mutex> guard(m_depsLock);
					auto& dep = m_deps[job.jobHandleNext.idx];
					const uint32_t depNext = dep.idxNext;
					dep.idxNext = (uint32_t)m_deps.size();
					m_deps.emplace_back(depNext, jobHandle, dependsOn);
				}
			}

			GAIA_NODISCARD bool HasDependencies(JobHandle jobHandle) const {
				const auto& job = m_jobs[jobHandle.idx];
				return job.dependencyCount > 0;
			}
#endif

			GAIA_NODISCARD bool IsBusy(JobHandle jobHandle) const {
				const auto& job = m_jobs[jobHandle.idx];
				return job.leftToFinish > 0;
			}
		};
	} // namespace mt
} // namespace gaia
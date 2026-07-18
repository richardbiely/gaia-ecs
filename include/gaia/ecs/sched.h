#pragma once

#include "gaia/config/config.h"

#include <cstdint>

#include "gaia/cnt/darray.h"
#include "gaia/mem/smallblock_allocator.h"
#include "gaia/mt/jobcommon.h"
#include "gaia/mt/jobhandle.h"
#include "gaia/mt/threadpool.h"

namespace gaia {
	namespace ecs {
		enum class QueryExecType : uint32_t;

		//! Opaque synchronization token returned by a scheduler.
		//! The scheduler owns the meaning of the payload.
		struct SchedToken {
			//! Scheduler-defined token payload.
			uintptr_t value[2]{};
		};

		//! Scheduler work flags shared by ECS scheduler descriptors.
		enum class SchedFlags : uint8_t {
			//! Default frame-bound work.
			Default = 0,
			//! Work may span multiple frames and should use the scheduler's background lane.
			Background = 0x01
		};

		//! Checks whether a scheduler flag set contains a specific flag.
		//! \param flags Flag set to inspect.
		//! \param flag Flag to test for.
		//! \return True when \a flag is present in \a flags.
		GAIA_NODISCARD inline bool sched_flags_has(SchedFlags flags, SchedFlags flag) {
			return ((uint8_t)flags & (uint8_t)flag) != 0U;
		}

		//! Description of a single task submitted to a scheduler.
		struct SchedTaskDesc {
			//! Opaque callback context forwarded to invoke().
			void* pCtx = nullptr;
			//! Task entry point.
			void (*invoke)(void* pCtx) = nullptr;
			//! Execution hint selected by the scheduler caller.
			QueryExecType execType{};
			//! Scheduler flags describing non-default execution requirements.
			SchedFlags flags = SchedFlags::Default;
		};

		//! Description of a parallel-for submission to a scheduler.
		struct SchedParDesc {
			//! Opaque callback context forwarded to invoke().
			void* pCtx = nullptr;
			//! Parallel-for entry point receiving a half-open item range [idxStart, idxEnd).
			void (*invoke)(void* pCtx, uint32_t idxStart, uint32_t idxEnd) = nullptr;
			//! Total number of items to process.
			uint32_t itemCount = 0;
			//! Preferred group size. A value of 0 lets the scheduler choose.
			uint32_t groupSize = 0;
			//! Execution hint selected by the scheduler caller.
			QueryExecType execType{};
			//! Scheduler flags describing non-default execution requirements.
			SchedFlags flags = SchedFlags::Default;
		};

		//! Scheduler descriptor used by ECS runtime code.
		//! All callbacks may be null when the descriptor is only used as a placeholder and will be
		//! resolved through sched_def().
		struct Sched {
			//! Opaque scheduler-owned context passed back to every callback.
			void* pCtx = nullptr;
			//! Schedules one task for execution.
			//! \param pCtx Scheduler-owned context.
			//! \param pDesc Task description to execute.
			//! \return Opaque synchronization token for the scheduled work.
			SchedToken (*sched)(void* pCtx, const SchedTaskDesc* pDesc) = nullptr;
			//! Schedules a parallel-for workload for execution.
			//! \param pCtx Scheduler-owned context.
			//! \param pDesc Parallel-for description to execute.
			//! \return Opaque synchronization token for the scheduled work.
			SchedToken (*sched_par)(void* pCtx, const SchedParDesc* pDesc) = nullptr;
			//! Adds one task without submitting it for execution.
			//! \param pCtx Scheduler-owned context.
			//! \param pDesc Task description to add.
			//! \return Opaque token for the added task.
			SchedToken (*add)(void* pCtx, const SchedTaskDesc* pDesc) = nullptr;
			//! Adds a parallel-for workload without submitting it for execution.
			//! \param pCtx Scheduler-owned context.
			//! \param pDesc Parallel-for description to add.
			//! \return Opaque token for the added workload.
			SchedToken (*add_par)(void* pCtx, const SchedParDesc* pDesc) = nullptr;
			//! Submits a previously added token.
			//! \param pCtx Scheduler-owned context.
			//! \param token Opaque token returned by add() or add_par().
			void (*submit)(void* pCtx, SchedToken token) = nullptr;
			//! Adds a dependency edge so \a tokenSecond runs after \a tokenFirst.
			//! \param pCtx Scheduler-owned context.
			//! \param tokenFirst Work that must complete first.
			//! \param tokenSecond Work that depends on \a tokenFirst.
			void (*dep)(void* pCtx, SchedToken tokenFirst, SchedToken tokenSecond) = nullptr;
			//! Waits until the scheduled work referenced by \a token finishes.
			//! \param pCtx Scheduler-owned context.
			//! \param token Opaque synchronization token returned by sched(), sched_par(), add(), or add_par().
			void (*wait)(void* pCtx, SchedToken token) = nullptr;
			//! Deletes any scheduler-owned resources associated with \a token.
			//! \param pCtx Scheduler-owned context.
			//! \param token Opaque synchronization token returned by sched(), sched_par(), add(), or add_par().
			void (*del)(void* pCtx, SchedToken token) = nullptr;
		};

		//! Move-only wrapper for scheduler-owned ECS work.
		//!
		//! SchedJob keeps the scheduler token together with the scheduler callbacks needed to submit,
		//! wait for, and delete the work. Query/system deferred execution uses this wrapper instead of
		//! exposing gaia::mt::JobHandle directly so external job systems can provide their own token type.
		class SchedJob {
			Sched m_sched;
			SchedToken m_token{};
			void* m_pCleanupCtx = nullptr;
			void (*m_cleanup)(void* pCtx) = nullptr;
			bool m_valid = false;
			bool m_submitted = false;
			bool m_waited = false;

			void cleanup() {
				if (m_cleanup != nullptr) {
					m_cleanup(m_pCleanupCtx);
					m_cleanup = nullptr;
					m_pCleanupCtx = nullptr;
				}
			}

			GAIA_NODISCARD static bool same_sched(const Sched& a, const Sched& b) {
				return a.pCtx == b.pCtx && a.sched == b.sched && a.sched_par == b.sched_par && a.add == b.add &&
							 a.add_par == b.add_par && a.submit == b.submit && a.dep == b.dep && a.wait == b.wait && a.del == b.del;
			}

		public:
			SchedJob() = default;

			//! \param sched Scheduler descriptor that created \a token.
			//! \param token Scheduler-owned work token.
			//! \param submitted True if the scheduler already submitted \a token.
			//! \param pCleanupCtx Optional cleanup context owned by the wrapper.
			//! \param cleanup Optional cleanup callback executed after the job is waited or deleted unsubmitted.
			SchedJob(Sched sched, SchedToken token, bool submitted, void* pCleanupCtx, void (*cleanup)(void* pCtx)):
					m_sched(sched), m_token(token), m_pCleanupCtx(pCleanupCtx), m_cleanup(cleanup), m_valid(true),
					m_submitted(submitted) {}

			//! Waits for and deletes a still-owned token.
			~SchedJob() {
				del();
			}

			SchedJob(const SchedJob&) = delete;
			SchedJob& operator=(const SchedJob&) = delete;

			//! Move-constructs a scheduler job wrapper.
			//! \param other Wrapper to move from.
			SchedJob(SchedJob&& other) noexcept:
					m_sched(other.m_sched), m_token(other.m_token), m_pCleanupCtx(other.m_pCleanupCtx),
					m_cleanup(other.m_cleanup), m_valid(other.m_valid), m_submitted(other.m_submitted), m_waited(other.m_waited) {
				other.m_valid = false;
				other.m_cleanup = nullptr;
				other.m_pCleanupCtx = nullptr;
			}

			//! Move-assigns a scheduler job wrapper.
			//! \param other Wrapper to move from.
			//! \return Self reference.
			SchedJob& operator=(SchedJob&& other) noexcept {
				if (this == &other)
					return *this;
				del();
				m_sched = other.m_sched;
				m_token = other.m_token;
				m_pCleanupCtx = other.m_pCleanupCtx;
				m_cleanup = other.m_cleanup;
				m_valid = other.m_valid;
				m_submitted = other.m_submitted;
				m_waited = other.m_waited;
				other.m_valid = false;
				other.m_cleanup = nullptr;
				other.m_pCleanupCtx = nullptr;
				return *this;
			}

			//! Returns true if this wrapper owns scheduler work.
			//! \return True when a scheduler token is owned by the wrapper.
			GAIA_NODISCARD bool valid() const {
				return m_valid;
			}

			//! Returns the opaque scheduler token.
			//! \return Token owned by this wrapper.
			GAIA_NODISCARD SchedToken token() const {
				return m_token;
			}

			//! Submits the added work if it has not been submitted yet.
			void submit();
			//! Adds a dependency edge so this job runs after \a jobFirst.
			//!
			//! This member form mirrors the Gaia scheduler naming used by mt::ThreadPool::dep() and sched_dep(). It is
			//! equivalent to calling sched_dep() with \a jobFirst as the prerequisite and this job as the dependent work.
			//!
			//! \param jobFirst Job that must complete before this job can run.
			//! \warning Both jobs must use the same scheduler descriptor, and dependencies must be added before either job is
			//! submitted.
			//! \see sched_dep()
			//! \see mt::ThreadPool::dep()
			void dep(const SchedJob& jobFirst);
			//! Waits for submitted work to complete.
			//! \warning Waiting does not submit deferred work. Call submit() first.
			void wait();
			//! Deletes scheduler resources and runs wrapper cleanup.
			void del();
		};

		//! \cond INTERNAL
		namespace detail {
			inline mt::JobPriority exec_prio(QueryExecType execType) {
				// QueryExecType::ParallelEff is encoded as value 3. Keep the scheduler bridge independent
				// from the enum definition point so this header stays C-like and forward-declarable.
				return (uint32_t)execType == 3U ? mt::JobPriority::Low : mt::JobPriority::High;
			}

			inline bool sched_flags_background(SchedFlags flags) {
				return sched_flags_has(flags, SchedFlags::Background);
			}

			inline mt::JobCreationFlags job_creation_flags(SchedFlags flags) {
				uint8_t jobFlags = (uint8_t)mt::JobCreationFlags::ManualDelete;
				if (sched_flags_background(flags))
					jobFlags |= (uint8_t)mt::JobCreationFlags::Background;
				return (mt::JobCreationFlags)jobFlags;
			}

			enum class SchedTokenKind : uint32_t { None, Single, Parallel };

			struct SchedTokenDefData {
				cnt::darray<mt::JobHandle> handles;
				SchedTokenKind kind = SchedTokenKind::None;
				bool submitted = false;

				GAIA_USE_SMALLBLOCK(SchedTokenDefData)
			};

			inline SchedToken make_sched_token(SchedTokenDefData* pData) {
				SchedToken token{};
				token.value[0] = (uintptr_t)pData;
				return token;
			}

			inline SchedTokenDefData* sched_token_data(SchedToken token) {
				return reinterpret_cast<SchedTokenDefData*>(token.value[0]);
			}

			inline SchedToken add_one_def([[maybe_unused]] void* pCtx, const SchedTaskDesc* pDesc) {
				GAIA_ASSERT(pDesc != nullptr);
				GAIA_ASSERT(pDesc->invoke != nullptr);
				if (pDesc == nullptr || pDesc->invoke == nullptr)
					return {};

				auto* pData = new SchedTokenDefData();
				pData->kind = SchedTokenKind::Single;
				pData->handles.resize(1);

				mt::Job job;
				job.priority = exec_prio(pDesc->execType);
				job.flags = job_creation_flags(pDesc->flags);
				job.func = [pCtx = pDesc->pCtx, invoke = pDesc->invoke]() {
					invoke(pCtx);
				};

				pData->handles[0] = mt::ThreadPool::get().add(GAIA_MOV(job));
				return make_sched_token(pData);
			}

			inline SchedToken sched_one_def([[maybe_unused]] void* pCtx, const SchedTaskDesc* pDesc) {
				auto token = add_one_def(pCtx, pDesc);
				auto* pData = sched_token_data(token);
				if (pData != nullptr) {
					mt::ThreadPool::get().submit(pData->handles[0]);
					pData->submitted = true;
				}
				return token;
			}

			inline SchedToken add_par_def([[maybe_unused]] void* pCtx, const SchedParDesc* pDesc) {
				GAIA_ASSERT(pDesc != nullptr);
				GAIA_ASSERT(pDesc->invoke != nullptr);
				if (pDesc == nullptr || pDesc->invoke == nullptr || pDesc->itemCount == 0)
					return {};

				auto& tp = mt::ThreadPool::get();
				const auto prio = exec_prio(pDesc->execType);
				uint32_t groupSize = pDesc->groupSize;
				if (groupSize == 0) {
					const bool background = sched_flags_background(pDesc->flags);
					const auto workers =
							background ? core::get_max(1U, tp.background_workers()) : core::get_max(1U, tp.workers() + 1U);
					groupSize = (pDesc->itemCount + workers - 1) / workers;
					constexpr uint32_t maxUnitsOfWorkPerGroup = 8;
					groupSize = groupSize / maxUnitsOfWorkPerGroup;
					if (groupSize == 0)
						groupSize = 1;
				}

				const auto jobs = (pDesc->itemCount + groupSize - 1) / groupSize;
				auto* pData = new SchedTokenDefData();
				pData->kind = SchedTokenKind::Parallel;
				pData->handles.resize(jobs + 1);

				for (uint32_t jobIndex = 0; jobIndex < jobs; ++jobIndex) {
					const uint32_t idxStart = jobIndex * groupSize;
					const uint32_t idxEnd = core::get_min(idxStart + groupSize, pDesc->itemCount);

					mt::Job job;
					job.priority = prio;
					job.flags = job_creation_flags(pDesc->flags);
					job.func = [desc = *pDesc, idxStart, idxEnd]() {
						desc.invoke(desc.pCtx, idxStart, idxEnd);
					};
					pData->handles[jobIndex] = tp.add(GAIA_MOV(job));
				}
				{
					mt::Job syncJob;
					syncJob.priority = prio;
					syncJob.flags = job_creation_flags(pDesc->flags);
					pData->handles[jobs] = tp.add(GAIA_MOV(syncJob));
				}
				tp.dep(std::span(pData->handles.data(), jobs), pData->handles[jobs]);
				return make_sched_token(pData);
			}

			inline SchedToken sched_par_def([[maybe_unused]] void* pCtx, const SchedParDesc* pDesc) {
				auto token = add_par_def(pCtx, pDesc);
				auto* pData = sched_token_data(token);
				if (pData != nullptr) {
					mt::ThreadPool::get().submit(std::span(pData->handles.data(), pData->handles.size()));
					pData->submitted = true;
				}
				return token;
			}

			inline void sched_submit_def([[maybe_unused]] void* pCtx, SchedToken token) {
				auto* pData = sched_token_data(token);
				if (pData == nullptr || pData->submitted)
					return;
				auto& tp = mt::ThreadPool::get();
				tp.submit(std::span(pData->handles.data(), pData->handles.size()));
				pData->submitted = true;
			}

			inline void sched_dep_def([[maybe_unused]] void* pCtx, SchedToken tokenFirst, SchedToken tokenSecond) {
				auto* pFirst = sched_token_data(tokenFirst);
				auto* pSecond = sched_token_data(tokenSecond);
				if (pFirst == nullptr || pSecond == nullptr || pFirst->handles.empty() || pSecond->handles.empty())
					return;
				auto& tp = mt::ThreadPool::get();
				const auto firstDone = pFirst->handles.back();
				if (pSecond->kind == SchedTokenKind::Parallel && pSecond->handles.size() > 1) {
					const auto childCount = (uint32_t)pSecond->handles.size() - 1;
					for (uint32_t i = 0; i < childCount; ++i)
						tp.dep(firstDone, pSecond->handles[i]);
					return;
				}
				tp.dep(firstDone, pSecond->handles.back());
			}

			inline void sched_wait_def([[maybe_unused]] void* pCtx, SchedToken token) {
				auto* pData = sched_token_data(token);
				if (pData == nullptr || pData->handles.empty())
					return;
				mt::ThreadPool::get().wait(pData->handles.back());
			}

			inline void sched_del_def([[maybe_unused]] void* pCtx, [[maybe_unused]] SchedToken token) {
				auto* pData = sched_token_data(token);
				if (pData == nullptr)
					return;
				auto& tp = mt::ThreadPool::get();
				for (auto handle: pData->handles) {
					if (handle != mt::JobNull)
						tp.del(handle);
				}
				delete pData;
			}
		} // namespace detail
		//! \endcond

		//! Returns the default ECS scheduler backed by gaia::mt::ThreadPool.
		//! \return Default scheduler descriptor.
		GAIA_NODISCARD inline const Sched& sched_def() {
			static const Sched sched = [] {
				Sched b{};
				b.sched = &detail::sched_one_def;
				b.sched_par = &detail::sched_par_def;
				b.add = &detail::add_one_def;
				b.add_par = &detail::add_par_def;
				b.submit = &detail::sched_submit_def;
				b.dep = &detail::sched_dep_def;
				b.wait = &detail::sched_wait_def;
				b.del = &detail::sched_del_def;
				return b;
			}();
			return sched;
		}

		//! Resolves \a sched to the default scheduler when it has no callbacks installed.
		//! \param sched Scheduler descriptor to resolve.
		//! \return Either \a sched or the default scheduler when \a sched is empty.
		GAIA_NODISCARD inline const Sched& sched_resolve(const Sched& sched) {
			if (sched.sched == nullptr && sched.sched_par == nullptr && sched.add == nullptr && sched.add_par == nullptr &&
					sched.submit == nullptr && sched.dep == nullptr && sched.wait == nullptr && sched.del == nullptr)
				return sched_def();
			return sched;
		}

		//! Schedules one task through \a sched.
		//! \param sched Scheduler descriptor.
		//! \param desc Task description.
		//! \return Opaque synchronization token for the scheduled work.
		GAIA_NODISCARD inline SchedToken sched_one(const Sched& sched, const SchedTaskDesc& desc) {
			const auto& resolved = sched_resolve(sched);
			if (resolved.sched != nullptr)
				return resolved.sched(resolved.pCtx, &desc);
			GAIA_ASSERT(resolved.add != nullptr);
			GAIA_ASSERT(resolved.submit != nullptr);
			const auto token = resolved.add != nullptr ? resolved.add(resolved.pCtx, &desc) : SchedToken{};
			if (resolved.submit != nullptr)
				resolved.submit(resolved.pCtx, token);
			return token;
		}

		//! Adds one task through \a sched without submitting it.
		//! \param sched Scheduler descriptor.
		//! \param desc Task description.
		//! \param pCleanupCtx Optional cleanup context owned by the returned wrapper.
		//! \param cleanup Optional cleanup callback executed after wait or unsubmitted deletion.
		//! \return Deferred scheduler job wrapper.
		//! \warning Deferred scheduler descriptors must provide submit(), wait(), and del() when add() is present.
		GAIA_NODISCARD inline SchedJob
		sched_add(const Sched& sched, const SchedTaskDesc& desc, void* pCleanupCtx, void (*cleanup)(void* pCtx)) {
			const auto& resolved = sched_resolve(sched);
			if (resolved.add != nullptr) {
				if (resolved.submit == nullptr || resolved.wait == nullptr || resolved.del == nullptr) {
					GAIA_ASSERT(false);
					if (cleanup != nullptr)
						cleanup(pCleanupCtx);
					return {};
				}
				return SchedJob(resolved, resolved.add(resolved.pCtx, &desc), false, pCleanupCtx, cleanup);
			}

			if (resolved.sched == nullptr || resolved.wait == nullptr || resolved.del == nullptr) {
				GAIA_ASSERT(false);
				if (cleanup != nullptr)
					cleanup(pCleanupCtx);
				return {};
			}
			return SchedJob(resolved, resolved.sched(resolved.pCtx, &desc), true, pCleanupCtx, cleanup);
		}

		//! Schedules a parallel-for workload through \a sched.
		//! \param sched Scheduler descriptor.
		//! \param desc Parallel-for description.
		//! \return Opaque synchronization token for the scheduled work.
		GAIA_NODISCARD inline SchedToken sched_par(const Sched& sched, const SchedParDesc& desc) {
			const auto& resolved = sched_resolve(sched);
			if (resolved.sched_par != nullptr)
				return resolved.sched_par(resolved.pCtx, &desc);
			GAIA_ASSERT(resolved.add_par != nullptr);
			GAIA_ASSERT(resolved.submit != nullptr);
			const auto token = resolved.add_par != nullptr ? resolved.add_par(resolved.pCtx, &desc) : SchedToken{};
			if (resolved.submit != nullptr)
				resolved.submit(resolved.pCtx, token);
			return token;
		}

		//! Adds a parallel-for workload through \a sched without submitting it.
		//! \param sched Scheduler descriptor.
		//! \param desc Parallel-for description.
		//! \param pCleanupCtx Optional cleanup context owned by the returned wrapper.
		//! \param cleanup Optional cleanup callback executed after wait or unsubmitted deletion.
		//! \return Deferred scheduler job wrapper.
		//! \warning Deferred scheduler descriptors must provide submit(), wait(), and del() when add() is present.
		GAIA_NODISCARD inline SchedJob
		sched_add_par(const Sched& sched, const SchedParDesc& desc, void* pCleanupCtx, void (*cleanup)(void* pCtx)) {
			const auto& resolved = sched_resolve(sched);
			if (resolved.add_par != nullptr) {
				if (resolved.submit == nullptr || resolved.wait == nullptr || resolved.del == nullptr) {
					GAIA_ASSERT(false);
					if (cleanup != nullptr)
						cleanup(pCleanupCtx);
					return {};
				}
				return SchedJob(resolved, resolved.add_par(resolved.pCtx, &desc), false, pCleanupCtx, cleanup);
			}

			if (resolved.sched_par == nullptr || resolved.wait == nullptr || resolved.del == nullptr) {
				GAIA_ASSERT(false);
				if (cleanup != nullptr)
					cleanup(pCleanupCtx);
				return {};
			}
			return SchedJob(resolved, resolved.sched_par(resolved.pCtx, &desc), true, pCleanupCtx, cleanup);
		}

		//! Submits an added scheduler token.
		//! \param sched Scheduler descriptor.
		//! \param token Opaque synchronization token returned by sched_add() or sched_add_par().
		inline void sched_submit(const Sched& sched, SchedToken token) {
			const auto& resolved = sched_resolve(sched);
			if (resolved.submit != nullptr)
				resolved.submit(resolved.pCtx, token);
		}

		//! Adds a scheduler dependency edge.
		//! \param sched Scheduler descriptor.
		//! \param tokenFirst Work that must complete first.
		//! \param tokenSecond Work that depends on \a tokenFirst.
		inline void sched_dep(const Sched& sched, SchedToken tokenFirst, SchedToken tokenSecond) {
			const auto& resolved = sched_resolve(sched);
			if (resolved.dep != nullptr)
				resolved.dep(resolved.pCtx, tokenFirst, tokenSecond);
		}

		//! Waits until the scheduled work referenced by \a token finishes.
		//! \param sched Scheduler descriptor.
		//! \param token Opaque synchronization token returned by sched_one() or sched_par().
		inline void sched_wait(const Sched& sched, SchedToken token) {
			const auto& resolved = sched_resolve(sched);
			if (resolved.wait != nullptr)
				resolved.wait(resolved.pCtx, token);
		}

		//! Deletes any scheduler-owned resources associated with \a token.
		//! \param sched Scheduler descriptor.
		//! \param token Opaque synchronization token returned by sched_one() or sched_par().
		inline void sched_del(const Sched& sched, SchedToken token) {
			const auto& resolved = sched_resolve(sched);
			if (resolved.del != nullptr)
				resolved.del(resolved.pCtx, token);
		}

		inline void SchedJob::submit() {
			if (!m_valid || m_submitted)
				return;
			GAIA_ASSERT(m_sched.submit != nullptr);
			if (m_sched.submit == nullptr)
				return;
			sched_submit(m_sched, m_token);
			m_submitted = true;
		}

		inline void SchedJob::dep(const SchedJob& jobFirst) {
			if (!m_valid || !jobFirst.m_valid)
				return;
			GAIA_ASSERT(same_sched(m_sched, jobFirst.m_sched));
			GAIA_ASSERT(!m_submitted && !jobFirst.m_submitted);
			if (!same_sched(m_sched, jobFirst.m_sched) || m_submitted || jobFirst.m_submitted)
				return;
			sched_dep(m_sched, jobFirst.m_token, m_token);
		}

		inline void SchedJob::wait() {
			if (!m_valid || m_waited)
				return;
			GAIA_ASSERT(m_submitted);
			if (!m_submitted)
				return;
			sched_wait(m_sched, m_token);
			m_waited = true;
			cleanup();
		}

		inline void SchedJob::del() {
			if (!m_valid)
				return;
			if (m_submitted && !m_waited)
				wait();
			sched_del(m_sched, m_token);
			cleanup();
			m_valid = false;
			m_submitted = false;
			m_waited = false;
		}
	} // namespace ecs
} // namespace gaia

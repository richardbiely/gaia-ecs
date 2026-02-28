#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <type_traits>

namespace gaia {
	namespace mt {
		using JobInternalType = uint32_t;
		using JobId = JobInternalType;
		using JobGenId = JobInternalType;

		struct GAIA_API JobHandle final {
			static constexpr JobInternalType IdBits = 20;
			static constexpr JobInternalType GenBits = 11;
			static constexpr JobInternalType PrioBits = 1;
			static constexpr JobInternalType AllBits = IdBits + GenBits + PrioBits;
			static constexpr JobInternalType IdMask = (uint32_t)(uint64_t(1) << IdBits) - 1;
			static constexpr JobInternalType GenMask = (uint32_t)(uint64_t(1) << GenBits) - 1;
			static constexpr JobInternalType PrioMask = (uint32_t)(uint64_t(1) << PrioBits) - 1;

			using JobSizeType = std::conditional_t<(AllBits > 32), uint64_t, uint32_t>;

			static_assert(AllBits <= 64, "Job IdBits and GenBits must fit inside 64 bits");
			static_assert(IdBits <= 31, "Job IdBits must be at most 31 bits long");
			static_assert(GenBits > 10, "Job GenBits must be at least 10 bits long");

		private:
			struct JobData {
				//! Index in entity array
				JobInternalType id : IdBits;
				//! Generation index. Incremented every time an item is deleted
				JobInternalType gen : GenBits;
				//! Job priority. 1-priority, 0-background
				JobInternalType prio : PrioBits;
			};

			union {
				JobData data;
				JobSizeType val;
			};

		public:
			JobHandle() {
				data.id = JobHandle::IdMask;
				data.gen = JobHandle::GenMask;
				data.prio = JobHandle::PrioMask;
			}
			JobHandle(JobId id, JobGenId gen, JobGenId prio) {
				data.id = id;
				data.gen = gen;
				data.prio = prio;
			}
			explicit JobHandle(uint32_t value) {
				val = value;
			}
			~JobHandle() = default;

			JobHandle(JobHandle&&) noexcept = default;
			JobHandle(const JobHandle&) = default;
			JobHandle& operator=(JobHandle&&) noexcept = default;
			JobHandle& operator=(const JobHandle&) = default;

			GAIA_NODISCARD constexpr bool operator==(const JobHandle& other) const noexcept {
				return val == other.val;
			}
			GAIA_NODISCARD constexpr bool operator!=(const JobHandle& other) const noexcept {
				return val != other.val;
			}

			GAIA_NODISCARD auto id() const {
				return data.id;
			}
			GAIA_NODISCARD auto gen() const {
				return data.gen;
			}
			GAIA_NODISCARD auto prio() const {
				return data.prio;
			}
			GAIA_NODISCARD auto value() const {
				return val;
			}
		};

		struct JobNull_t {
			GAIA_NODISCARD operator JobHandle() const noexcept {
				return JobHandle(JobHandle::IdMask, JobHandle::GenMask, JobHandle::PrioMask);
			}

			GAIA_NODISCARD constexpr bool operator==([[maybe_unused]] const JobNull_t& null) const noexcept {
				return true;
			}
			GAIA_NODISCARD constexpr bool operator!=([[maybe_unused]] const JobNull_t& null) const noexcept {
				return false;
			}
		};

		GAIA_NODISCARD inline bool operator==(const JobNull_t& null, const JobHandle& entity) noexcept {
			return static_cast<JobHandle>(null).id() == entity.id();
		}

		GAIA_NODISCARD inline bool operator!=(const JobNull_t& null, const JobHandle& entity) noexcept {
			return static_cast<JobHandle>(null).id() != entity.id();
		}

		GAIA_NODISCARD inline bool operator==(const JobHandle& entity, const JobNull_t& null) noexcept {
			return null == entity;
		}

		GAIA_NODISCARD inline bool operator!=(const JobHandle& entity, const JobNull_t& null) noexcept {
			return null != entity;
		}

		inline constexpr JobNull_t JobNull{};
	} // namespace mt
} // namespace gaia

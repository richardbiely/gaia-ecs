#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <type_traits>

namespace gaia {
	namespace mt {
		//! Unsigned storage type used by packed job handles.
		using JobInternalType = uint32_t;
		//! Job-pool slot identifier type.
		using JobId = JobInternalType;
		//! Job generation and priority field type.
		using JobGenId = JobInternalType;

		//! Packed identifier for a job-pool slot, generation, and priority.
		struct GAIA_API JobHandle final {
			//! Number of bits reserved for the slot identifier.
			static constexpr JobInternalType IdBits = 20;
			//! Number of bits reserved for the generation.
			static constexpr JobInternalType GenBits = 11;
			//! Number of bits reserved for the priority.
			static constexpr JobInternalType PrioBits = 1;
			//! Total number of bits used by a packed handle.
			static constexpr JobInternalType AllBits = IdBits + GenBits + PrioBits;
			//! Mask selecting the slot identifier.
			static constexpr JobInternalType IdMask = (uint32_t)(uint64_t(1) << IdBits) - 1;
			//! Mask selecting the unshifted generation.
			static constexpr JobInternalType GenMask = (uint32_t)(uint64_t(1) << GenBits) - 1;
			//! Mask selecting the unshifted priority.
			static constexpr JobInternalType PrioMask = (uint32_t)(uint64_t(1) << PrioBits) - 1;

			//! Integer type large enough to hold all packed fields.
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
				//! Encoded job priority bit.
				JobInternalType prio : PrioBits;
			};

			union {
				JobData data;
				JobSizeType val;
			};

		public:
			//! Creates the null job handle.
			JobHandle() {
				data.id = JobHandle::IdMask;
				data.gen = JobHandle::GenMask;
				data.prio = JobHandle::PrioMask;
			}
			//! Creates a handle from its component fields.
			//! \param id Job-pool slot identifier.
			//! \param gen Slot generation.
			//! \param prio Encoded priority bit.
			JobHandle(JobId id, JobGenId gen, JobGenId prio) {
				data.id = id;
				data.gen = gen;
				data.prio = prio;
			}
			//! Creates a handle from its packed representation.
			//! \param value Packed handle value.
			explicit JobHandle(uint32_t value) {
				val = value;
			}
			~JobHandle() = default;

			JobHandle(JobHandle&&) noexcept = default;
			JobHandle(const JobHandle&) = default;
			JobHandle& operator=(JobHandle&&) noexcept = default;
			JobHandle& operator=(const JobHandle&) = default;

			//! Compares packed handle values.
			//! \param other Handle to compare.
			//! \return True when both handles are identical.
			GAIA_NODISCARD constexpr bool operator==(const JobHandle& other) const noexcept {
				return val == other.val;
			}
			//! Compares packed handle values.
			//! \param other Handle to compare.
			//! \return True when the handles differ.
			GAIA_NODISCARD constexpr bool operator!=(const JobHandle& other) const noexcept {
				return val != other.val;
			}

			//! Returns the job-pool slot identifier.
			//! \return Slot identifier.
			GAIA_NODISCARD auto id() const {
				return data.id;
			}
			//! Returns the slot generation.
			//! \return Generation value.
			GAIA_NODISCARD auto gen() const {
				return data.gen;
			}
			//! Returns the encoded priority bit.
			//! \return Priority bit.
			GAIA_NODISCARD auto prio() const {
				return data.prio;
			}
			//! Returns the packed handle representation.
			//! \return Packed value.
			GAIA_NODISCARD auto value() const {
				return val;
			}
		};

		//! Sentinel type representing an invalid job handle.
		struct JobNull_t {
			//! Converts the sentinel to a packed null handle.
			//! \return Null job handle.
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

		//! Global null-job sentinel.
		inline constexpr JobNull_t JobNull{};
	} // namespace mt
} // namespace gaia

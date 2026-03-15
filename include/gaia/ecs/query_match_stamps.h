#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <cstring>

#include "gaia/cnt/darray.h"
#include "gaia/core/utility.h"
#include "gaia/mem/mem_alloc.h"

namespace gaia {
	namespace ecs {
		struct ArchetypeMatchStamps {
			//! Keep stamp pages small enough to avoid dense high-water-mark allocations while
			//! still preserving O(1) indexing by world-local archetype id.
			static constexpr uint32_t PageBits = 10;
			static constexpr uint32_t PageSize = 1U << PageBits;
			static constexpr uint32_t PageMask = PageSize - 1;

			//! Lazily allocated pages keyed by archetype id >> PageBits.
			cnt::darray<uint32_t*> pages;

			ArchetypeMatchStamps() = default;
			ArchetypeMatchStamps(const ArchetypeMatchStamps&) = delete;
			ArchetypeMatchStamps& operator=(const ArchetypeMatchStamps&) = delete;

			ArchetypeMatchStamps(ArchetypeMatchStamps&& other) noexcept: pages(GAIA_MOV(other.pages)) {
				other.pages = {};
			}

			ArchetypeMatchStamps& operator=(ArchetypeMatchStamps&& other) noexcept {
				if (this == &other)
					return *this;

				free_pages();
				pages = GAIA_MOV(other.pages);
				other.pages = {};
				return *this;
			}

			~ArchetypeMatchStamps() {
				free_pages();
			}

			GAIA_NODISCARD bool has(uint32_t sid) const {
				const auto pid = sid >> PageBits;
				return pid < pages.size() && pages[pid] != nullptr;
			}

			GAIA_NODISCARD uint32_t get(uint32_t sid) const {
				GAIA_ASSERT(has(sid));
				const auto pid = sid >> PageBits;
				const auto did = sid & PageMask;
				return pages[pid][did];
			}

			void set(uint32_t sid, uint32_t version) {
				const auto pid = sid >> PageBits;
				const auto did = sid & PageMask;
				auto* page = ensure_page(pid);
				page[did] = version;
			}

			void clear() {
				for (auto* page: pages) {
					if (page == nullptr)
						continue;
					//! Reuse allocated pages across matcher runs. Only the stored stamp values
					//! need to be reset; freeing the pages here would put heap churn back into
					//! the hot path.
					std::memset(page, 0, sizeof(uint32_t) * PageSize);
				}
			}

		private:
			GAIA_NODISCARD uint32_t* ensure_page(uint32_t pid) {
				if (pid >= pages.size())
					pages.resize(pid + 1, nullptr);

				auto*& page = pages[pid];
				if (page == nullptr) {
					page = mem::AllocHelper::alloc<uint32_t>("ArchetypeMatchStampPage", PageSize);
					std::memset(page, 0, sizeof(uint32_t) * PageSize);
				}

				return page;
			}

			void free_pages() {
				for (auto* page: pages) {
					if (page == nullptr)
						continue;
					mem::AllocHelper::free("ArchetypeMatchStampPage", page);
				}
				pages = {};
			}
		};
	} // namespace ecs
} // namespace gaia

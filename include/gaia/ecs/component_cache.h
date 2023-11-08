#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../cnt/darray.h"
#include "../cnt/map.h"
#include "../config/logging.h"
#include "../meta/type_info.h"
#include "component.h"
#include "component_desc.h"

namespace gaia {
	namespace ecs {
		class ComponentCache {
			cnt::darray<const ComponentDesc*> m_descByIndex;

			ComponentCache() {
				// Reserve enough storage space for most use-cases
				constexpr uint32_t DefaultComponentCacheSize = 2048;
				m_descByIndex.reserve(DefaultComponentCacheSize);

				// Make sure unused memory is initialized to nullptr
				GAIA_EACH(m_descByIndex) m_descByIndex[i] = nullptr;
			}

		public:
			static ComponentCache& get() {
				static ComponentCache cache;
				return cache;
			}

			~ComponentCache() {
				clear();
			}

			ComponentCache(ComponentCache&&) = delete;
			ComponentCache(const ComponentCache&) = delete;
			ComponentCache& operator=(ComponentCache&&) = delete;
			ComponentCache& operator=(const ComponentCache&) = delete;

			//! Registers the component info for \tparam T. If it already exists it is returned.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD GAIA_FORCEINLINE const ComponentDesc& goc_comp_desc() {
				using U = typename component_type_t<T>::Type;
				const auto compId = comp_id<T>();

				auto createDesc = [&]() GAIA_LAMBDAINLINE -> const ComponentDesc& {
					const auto* pDesc = ComponentDesc::create<U>();
					m_descByIndex[compId] = pDesc;
					return *pDesc;
				};

				if GAIA_UNLIKELY (compId >= m_descByIndex.size()) {
					const auto oldSize = m_descByIndex.size();
					const auto newSize = compId + 1U;

					// Increase the capacity by multiples of CapacityIncreaseSize
					constexpr uint32_t CapacityIncreaseSize = 128;
					const auto oldCapacity = m_descByIndex.capacity();
					const auto newCapacity = (newSize / CapacityIncreaseSize) * CapacityIncreaseSize + CapacityIncreaseSize;
					if (oldCapacity < newCapacity) {
						m_descByIndex.reserve(newCapacity);
						// Make sure unused memory is initialized to nullptr
						GAIA_FOR2(oldSize, newCapacity) m_descByIndex[i] = nullptr;
					}

					// Update the size
					m_descByIndex.resize(newSize);

					return createDesc();
				}

				if GAIA_UNLIKELY (m_descByIndex[compId] == nullptr) {
					return createDesc();
				}

				return *m_descByIndex[compId];
			}

			//! Returns the component creation info given the \param compId.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const ComponentDesc& comp_desc(ComponentId compId) const noexcept {
				GAIA_ASSERT(compId < m_descByIndex.size());
				return *m_descByIndex[compId];
			}

			//! Returns the component info for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD const ComponentDesc& comp_desc() const noexcept {
				const auto compId = comp_id<T>();
				return comp_desc(compId);
			}

			void diag() const {
				const auto registeredTypes = m_descByIndex.size();
				GAIA_LOG_N("Registered comps: %u", registeredTypes);

				for (const auto* desc: m_descByIndex)
					GAIA_LOG_N("  id:%010u, %.*s", desc->comp.id(), (uint32_t)desc->name.size(), desc->name.data());
			}

		private:
			void clear() {
				for (const auto* pDesc: m_descByIndex)
					delete pDesc;
				m_descByIndex.clear();
			}
		};
	} // namespace ecs
} // namespace gaia

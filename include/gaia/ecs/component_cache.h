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
#include "component_info.h"

namespace gaia {
	namespace ecs {
		class ComponentCache {
			cnt::darray<const ComponentInfo*> m_infoByIndex;
			cnt::darray<ComponentDesc> m_descByIndex;

			ComponentCache() {
				// Reserve enough storage space for most use-cases
				constexpr uint32_t DefaultComponentCacheSize = 2048;
				m_infoByIndex.reserve(DefaultComponentCacheSize);
				m_descByIndex.reserve(DefaultComponentCacheSize);
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
			GAIA_NODISCARD GAIA_FORCEINLINE const ComponentInfo& goc_comp_info() {
				using U = typename component_kind_t<T>::Kind;
				const auto compId = comp_id<T>();

				auto createInfo = [&]() GAIA_LAMBDAINLINE -> const ComponentInfo& {
					const auto* pInfo = ComponentInfo::create<U>();
					m_infoByIndex[compId] = pInfo;
					m_descByIndex[compId] = ComponentDesc::create<U>();
					return *pInfo;
				};

				if GAIA_UNLIKELY (compId >= m_infoByIndex.size()) {
					const auto oldSize = m_infoByIndex.size();
					const auto newSize = compId + 1U;

					// Increase the capacity by multiples of CapacityIncreaseSize
					constexpr uint32_t CapacityIncreaseSize = 128;
					const auto newCapacity = (newSize / CapacityIncreaseSize) * CapacityIncreaseSize + CapacityIncreaseSize;
					m_infoByIndex.reserve(newCapacity);

					// Update the size
					m_infoByIndex.resize(newSize);
					m_descByIndex.resize(newSize);

					// Make sure that unused memory is initialized to nullptr
					for (uint32_t i = oldSize; i < newSize; ++i)
						m_infoByIndex[i] = nullptr;

					return createInfo();
				}

				if GAIA_UNLIKELY (m_infoByIndex[compId] == nullptr) {
					return createInfo();
				}

				return *m_infoByIndex[compId];
			}

			//! Returns the component info given the \param compId.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD GAIA_FORCEINLINE const ComponentInfo& comp_info(ComponentId compId) const noexcept {
				GAIA_ASSERT(compId < m_infoByIndex.size());
				const auto* pInfo = m_infoByIndex[compId];
				GAIA_ASSERT(pInfo != nullptr);
				return *pInfo;
			}

			//! Returns the component creation info given the \param compId.
			//! \warning It is expected the component info with a given component id exists! Undefined behavior otherwise.
			//! \return Component info
			GAIA_NODISCARD const ComponentDesc& comp_desc(ComponentId compId) const noexcept {
				GAIA_ASSERT(compId < m_descByIndex.size());
				return m_descByIndex[compId];
			}

			//! Returns the component info for \tparam T.
			//! \warning It is expected the component already exists! Undefined behavior otherwise.
			//! \return Component info
			template <typename T>
			GAIA_NODISCARD const ComponentInfo& comp_info() const noexcept {
				const auto compId = comp_id<T>();
				return comp_info(compId);
			}

			void diag() const {
				const auto registeredTypes = (uint32_t)m_descByIndex.size();
				GAIA_LOG_N("Registered infos: %u", registeredTypes);

				for (const auto& desc: m_descByIndex)
					GAIA_LOG_N("  id:%010u, %.*s", desc.compId, (uint32_t)desc.name.size(), desc.name.data());
			}

		private:
			void clear() {
				for (const auto* pInfo: m_infoByIndex)
					delete pInfo;
				m_infoByIndex.clear();
				m_descByIndex.clear();
			}
		};
	} // namespace ecs
} // namespace gaia

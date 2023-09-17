#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "chunk_header.h"
#include "component.h"
#include "query_info.h"

namespace gaia {
	namespace ecs {
		struct Iterator {
		private:
			query::QueryInfo& m_info;
			archetype::Chunk& m_chunk;
			uint32_t m_pos;

		public:
			Iterator(query::QueryInfo& info, archetype::Chunk& chunk): m_info(info), m_chunk(chunk), m_pos(0) {}
			Iterator(query::QueryInfo& info, archetype::Chunk& chunk, uint32_t pos):
					m_info(info), m_chunk(chunk), m_pos(pos) {}

			uint32_t operator*() const {
				return m_pos;
			}

			uint32_t operator->() const {
				return m_pos;
			}

			Iterator& operator++() {
				++m_pos;
				return *this;
			}

			GAIA_NODISCARD Iterator operator++(int) {
				Iterator temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const Iterator& other) const {
				return m_pos == other.m_pos;
			}

			GAIA_NODISCARD bool operator!=(const Iterator& other) const {
				return m_pos != other.m_pos;
			}

			GAIA_NODISCARD Iterator begin() const {
				return *this;
			}

			GAIA_NODISCARD Iterator end() const {
				return {m_info, m_chunk, m_chunk.GetEntityCount()};
			}

			GAIA_NODISCARD uint32_t size() const {
				return m_chunk.GetEntityCount();
			}

			//! Checks if the entity at the current iterator index is enabled.
			//! \return True it the entity is enabled. False otherwise.
			bool IsEntityEnabled() const {
				return !m_chunk.GetDisabledEntityMask().test(m_pos);
			}

			//! Checks if component \tparam T is present in the chunk.
			//! \tparam T Component
			//! \return True if the component is present. False otherwise.
			template <typename T>
			GAIA_NODISCARD bool HasComponent() const {
				return m_chunk.HasComponent<T>();
			}

			//! Returns a read-only entity or component view.
			//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \return Entity of component view with read-only access
			template <typename T>
			GAIA_NODISCARD auto View() const {
#if GAIA_DEBUG
				if constexpr (component::IsGenericComponent<T>) {
					using U = typename component::DeduceComponent<T>::Type;
					using UConst = std::add_const_t<U>;
					GAIA_ASSERT(m_info.Has<UConst>());
				} else {
					using U = typename component::DeduceComponent<T>::Type;
					using UConst = std::add_const_t<U>;
					using UChunk = AsChunk<UConst>;
					GAIA_ASSERT(m_info.Has<UChunk>());
				}
#endif

				return m_chunk.View<T>();
			}

			//! Returns a mutable entity or component view.
			//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \return Entity or component view with read-write access
			template <typename T>
			GAIA_NODISCARD auto ViewRW() {
				GAIA_ASSERT(m_info.Has<T>());
				return m_chunk.ViewRW<T>();
			}

			//! Returns a mutable component view.
			//! Doesn't update the world version when the access is aquired.
			//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \return Component view with read-write access
			template <typename T>
			GAIA_NODISCARD auto ViewRWSilent() {
				GAIA_ASSERT(m_info.Has<T>());
				return m_chunk.ViewRWSilent<T>();
			}
		};
	} // namespace ecs
} // namespace gaia
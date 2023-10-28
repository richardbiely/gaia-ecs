#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../core/iterator.h"
#include "chunk.h"

namespace gaia {
	namespace ecs {
		//! QueryImpl constraints
		enum class Constraints : uint8_t { EnabledOnly, DisabledOnly, AcceptAll };

		namespace detail {
			template <Constraints IterConstraint>
			class ChunkIteratorImpl {
			protected:
				Chunk& m_chunk;

			public:
				ChunkIteratorImpl(Chunk& chunk): m_chunk(chunk) {}

				//! Returns a read-only entity or component view.
				//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity of component view with read-only access
				template <typename T>
				GAIA_NODISCARD auto view() const {
					return m_chunk.view<T>(from(), to());
				}

				//! Returns a mutable entity or component view.
				//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto view_mut() {
					return m_chunk.view_mut<T>(from(), to());
				}

				//! Returns a mutable component view.
				//! Doesn't update the world version when the access is aquired.
				//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				//! \tparam T Component
				//! \return Component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto sview_mut() {
					return m_chunk.sview_mut<T>(from(), to());
				}

				//! Returns either a mutable or immutable entity/component view based on the requested type.
				//! Value and const types are considered immutable. Anything else is mutable.
				//! \warning If \tparam T is a component it is expected to be present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view
				template <typename T>
				GAIA_NODISCARD auto view_auto() {
					return m_chunk.view_auto<T>(from(), to());
				}

				//! Returns either a mutable or immutable entity/component view based on the requested type.
				//! Value and const types are considered immutable. Anything else is mutable.
				//! Doesn't update the world version when read-write access is aquired.
				//! \warning If \tparam T is a component it is expected to be present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view
				template <typename T>
				GAIA_NODISCARD auto sview_auto() {
					return m_chunk.sview_auto<T>(from(), to());
				}

				//! Checks if the entity at the current iterator index is enabled.
				//! \return True it the entity is enabled. False otherwise.
				GAIA_NODISCARD bool enabled(uint32_t index) const {
					const auto entityIdx = from() + index;
					return m_chunk.enabled(entityIdx);
				}

				//! Checks if component \tparam T is present in the chunk.
				//! \tparam T Component
				//! \return True if the component is present. False otherwise.
				template <typename T>
				GAIA_NODISCARD bool has() const {
					return m_chunk.has<T>();
				}

				//! Returns the number of entities accessible via the iterator
				GAIA_NODISCARD uint32_t size() const noexcept {
					if constexpr (IterConstraint == Constraints::EnabledOnly)
						return m_chunk.size_enabled();
					else if constexpr (IterConstraint == Constraints::DisabledOnly)
						return m_chunk.size_disabled();
					else
						return m_chunk.size();
				}

			private:
				//! Returns the starting index of the iterator
				GAIA_NODISCARD uint32_t from() const noexcept {
					if constexpr (IterConstraint == Constraints::EnabledOnly)
						return m_chunk.size_disabled();
					else
						return 0;
				}

				//! Returns the ending index of the iterator (one past the last valid index)
				GAIA_NODISCARD uint32_t to() const noexcept {
					if constexpr (IterConstraint == Constraints::DisabledOnly)
						return m_chunk.size_disabled();
					else
						return m_chunk.size();
				}
			};
		} // namespace detail

		using Iterator = detail::ChunkIteratorImpl<Constraints::EnabledOnly>;
		using IteratorDisabled = detail::ChunkIteratorImpl<Constraints::DisabledOnly>;
		using IteratorAll = detail::ChunkIteratorImpl<Constraints::AcceptAll>;
	} // namespace ecs
} // namespace gaia
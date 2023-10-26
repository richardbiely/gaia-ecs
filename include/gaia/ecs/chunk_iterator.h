#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../core/iterator.h"
#include "chunk.h"

namespace gaia {
	namespace ecs {
		namespace detail {
			class IteratorBase {
			protected:
				Chunk& m_chunk;

			public:
				IteratorBase(Chunk& chunk): m_chunk(chunk) {}

				//! Checks if component \tparam T is present in the chunk.
				//! \tparam T Component
				//! \return True if the component is present. False otherwise.
				template <typename T>
				GAIA_NODISCARD bool has() const {
					return m_chunk.has<T>();
				}
			};

			template <typename Func>
			void each(Func func, const uint32_t size) noexcept {
				if constexpr (std::is_invocable_v<Func, uint32_t>) {
					for (uint32_t i = 0; i < size; ++i)
						func(i);
				} else {
					for (uint32_t i = 0; i < size; ++i)
						func();
				}
			}
		} // namespace detail

		struct Iterator: public detail::IteratorBase {
			using Iter = core::index_iterator;

			Iterator(Chunk& chunk) noexcept: detail::IteratorBase(chunk) {}

			//! Returns the number of entities accessible via the iterator
			GAIA_NODISCARD uint32_t size() const noexcept {
				return m_chunk.size_enabled();
			}

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

			//! Runs the functor for each item handled by the iterator
			//! \tparam func Function to execute
			template <typename Func>
			void each(Func func) noexcept {
				detail::each(func, size());
			}

			//! Checks if the entity at the current iterator index is enabled.
			//! \return True it the entity is enabled. False otherwise.
			GAIA_NODISCARD bool enabled(uint32_t index) const {
				const auto entityIdx = from() + index;
				return from() + entityIdx >= m_chunk.size_disabled() && entityIdx < m_chunk.size();
			}

		private:
			//! Returns the starting index of the iterator
			GAIA_NODISCARD uint32_t from() const noexcept {
				return m_chunk.size_disabled();
			}

			//! Returns the ending index of the iterator (one past the last valid index)
			GAIA_NODISCARD uint32_t to() const noexcept {
				return m_chunk.size();
			}
		};

		struct IteratorDisabled: public detail::IteratorBase {
			using Iter = core::index_iterator;

			IteratorDisabled(Chunk& chunk) noexcept: detail::IteratorBase(chunk) {}

			//! Returns the number of entities accessible via the iterator
			GAIA_NODISCARD uint32_t size() const noexcept {
				return m_chunk.size_disabled();
			}

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

			//! Runs the functor for each item handled by the iterator
			//! \tparam func Function to execute
			template <typename Func>
			void each(Func func) noexcept {
				detail::each(func, size());
			}

			//! Checks if the entity at the current iterator index is enabled.
			//! \return True it the entity is enabled. False otherwise.
			GAIA_NODISCARD bool enabled(uint32_t index) const {
				const auto entityIdx = from() + index;
				return from() + entityIdx >= m_chunk.size_disabled() && entityIdx < m_chunk.size();
			}

		private:
			//! Returns the starting index of the iterator
			GAIA_NODISCARD uint32_t from() const noexcept {
				return 0;
			}

			//! Returns the ending index of the iterator (one past the last valid index)
			GAIA_NODISCARD uint32_t to() const noexcept {
				return m_chunk.size_disabled();
			}
		};

		struct IteratorAll: public detail::IteratorBase {
			using Iter = core::index_iterator;

			IteratorAll(Chunk& chunk) noexcept: detail::IteratorBase(chunk) {}

			//! Returns the number of entities accessible via the iterator
			GAIA_NODISCARD uint32_t size() const noexcept {
				return m_chunk.size();
			}

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

			//! Runs the functor for each item handled by the iterator
			//! \tparam func Function to execute
			template <typename Func>
			void each(Func func) noexcept {
				detail::each(func, size());
			}

			//! Checks if the entity at the current iterator index is enabled.
			//! \return True it the entity is enabled. False otherwise.
			GAIA_NODISCARD bool enabled(uint32_t index) const {
				const auto entityIdx = from() + index;
				return from() + entityIdx >= m_chunk.size_disabled() && entityIdx < m_chunk.size();
			}

		private:
			//! Returns the starting index of the iterator
			GAIA_NODISCARD uint32_t from() const noexcept {
				return 0;
			}

			//! Returns the ending index of the iterator (one past the last valid index)
			GAIA_NODISCARD uint32_t to() const noexcept {
				return m_chunk.size();
			}
		};
	} // namespace ecs
} // namespace gaia
#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../containers/bitset.h"
#include "chunk.h"
#include "component.h"
#include "component_getter.h"
#include "component_setter.h"

namespace gaia {
	namespace ecs {
		namespace detail {
			using ChunkAccessorMask = archetype::ChunkHeader::DisabledEntityMask;
			using ChunkAccessorIter = ChunkAccessorMask::const_iterator;
			using ChunkAccessorIterInverse = ChunkAccessorMask::const_iterator_inverse;

			class ChunkAccessorCommon {
			protected:
				archetype::Chunk& m_chunk;

			public:
				ChunkAccessorCommon(archetype::Chunk& chunk): m_chunk(chunk) {}

				//! Checks if component \tparam T is present in the chunk.
				//! \tparam T Component
				//! \return True if the component is present. False otherwise.
				template <typename T>
				GAIA_NODISCARD bool HasComponent() const {
					return m_chunk.HasComponent<T>();
				}

				//! Checks if the entity at the current iterator index is enabled.
				//! \return True it the entity is enabled. False otherwise.
				GAIA_NODISCARD bool IsEntityEnabled(uint32_t entityIdx) const {
					return !m_chunk.GetDisabledEntityMask().test(entityIdx);
				}

				//! Returns a read-only entity or component view.
				//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity of component view with read-only access
				template <typename T>
				GAIA_NODISCARD auto View() const {
					return m_chunk.View<T>();
				}

				//! Returns a mutable entity or component view.
				//! \warning If \tparam T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto ViewRW() {
					return m_chunk.ViewRW<T>();
				}

				//! Returns a mutable component view.
				//! Doesn't update the world version when the access is aquired.
				//! \warning It is expected the component \tparam T is present. Undefined behavior otherwise.
				//! \tparam T Component
				//! \return Component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto ViewRWSilent() {
					return m_chunk.ViewRWSilent<T>();
				}
			};
		} // namespace detail

		class ChunkAccessor: public detail::ChunkAccessorCommon {
		protected:
			uint32_t m_pos;

		public:
			ChunkAccessor(archetype::Chunk& chunk, uint32_t pos): ChunkAccessorCommon(chunk), m_pos(pos) {}
		};

		struct ChunkAccessorIt {
		public:
			using value_type = uint32_t;

		protected:
			value_type m_pos;

		public:
			ChunkAccessorIt(value_type pos): m_pos(pos) {}

			GAIA_NODISCARD value_type operator*() const {
				return m_pos;
			}

			GAIA_NODISCARD value_type operator->() const {
				return m_pos;
			}

			ChunkAccessorIt operator++() {
				++m_pos;
				return *this;
			}

			GAIA_NODISCARD ChunkAccessorIt operator++(int) {
				ChunkAccessorIt temp(*this);
				++*this;
				return temp;
			}

			GAIA_NODISCARD bool operator==(const ChunkAccessorIt& other) const {
				return m_pos == other.m_pos;
			}

			GAIA_NODISCARD bool operator!=(const ChunkAccessorIt& other) const {
				return m_pos != other.m_pos;
			}
		};

		class ChunkAccessorWithMask: public detail::ChunkAccessorCommon {
		public:
			using Mask = detail::ChunkAccessorMask;

		protected:
			const Mask& m_mask;

		public:
			ChunkAccessorWithMask(archetype::Chunk& chunk, const Mask& mask): ChunkAccessorCommon(chunk), m_mask(mask) {}
		};

		using ChunkAccessorWithMaskIt = detail::ChunkAccessorIter;
		using ChunkAccessorWithMaskItInverse = detail::ChunkAccessorIterInverse;
	} // namespace ecs
} // namespace gaia
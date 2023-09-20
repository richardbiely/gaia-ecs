#pragma once
#include "../config/config.h"

#include <cinttypes>
#include <type_traits>

#include "../containers/bitset.h"
#include "chunk.h"
#include "chunk_header.h"
#include "component.h"
#include "component_getter.h"
#include "component_setter.h"

namespace gaia {
	namespace ecs {
		namespace detail {
			using ChunkAccessorMask = archetype::ChunkHeader::DisabledEntityMask;
			using ChunkAccessorIter = ChunkAccessorMask::const_iterator;

			struct ChunkAccessorIndexIter {
			public:
				using value_type = uint32_t;

			private:
				value_type m_pos;

			public:
				ChunkAccessorIndexIter(value_type pos): m_pos(pos) {}

				GAIA_NODISCARD value_type operator*() const {
					return m_pos;
				}

				ChunkAccessorIndexIter& operator++() {
					++m_pos;
					return *this;
				}

				GAIA_NODISCARD ChunkAccessorIndexIter operator++(int) {
					ChunkAccessorIndexIter temp(*this);
					++*this;
					return temp;
				}

				GAIA_NODISCARD bool operator==(const ChunkAccessorIndexIter& other) const {
					return m_pos == other.m_pos;
				}

				GAIA_NODISCARD bool operator!=(const ChunkAccessorIndexIter& other) const {
					return m_pos != other.m_pos;
				}
			};
		} // namespace detail

		class ChunkAccessor {
		protected:
			archetype::Chunk& m_chunk;
			uint32_t m_pos;

		public:
			ChunkAccessor(archetype::Chunk& chunk, uint32_t pos): m_chunk(chunk), m_pos(pos) {}

			GAIA_NODISCARD uint32_t index() const {
				return m_pos;
			}

			//! Checks if component \tparam T is present in the chunk.
			//! \tparam T Component
			//! \return True if the component is present. False otherwise.
			template <typename T>
			GAIA_NODISCARD bool HasComponent() const {
				return m_chunk.HasComponent<T>();
			}

			//! Returns the value stored in the component \tparam T on \param entity.
			//! \tparam T Component
			//! \return Value stored in the component.
			template <typename T>
			GAIA_NODISCARD auto GetComponent() const {
				return ComponentGetter{&m_chunk, index()}.GetComponent<T>();
			}

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			GAIA_NODISCARD U& SetComponent() {
				return ComponentSetter{&m_chunk, index()}.SetComponent<T, U>();
			}

			//! Sets the value of the component \tparam T on \param entity.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter& SetComponent(U&& data) {
				return ComponentSetter{&m_chunk, index()}.SetComponent<T, U>(std::forward<U>(data));
			}

			//! Sets the value of the component \tparam T on \param entity without trigger a world version update.
			//! \tparam T Component
			//! \param value Value to set for the component
			//! \return ComponentSetter
			template <typename T, typename U = typename component::DeduceComponent<T>::Type>
			ComponentSetter& SetComponentSilent(U&& data) {
				return ComponentSetter{&m_chunk, index()}.SetComponentSilent<T, U>(std::forward<U>(data));
			}
		};

		class ChunkAccessorExt: public ChunkAccessor {
		public:
			ChunkAccessorExt(archetype::Chunk& chunk, uint32_t pos): ChunkAccessor(chunk, pos) {}

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

			//! Checks if the entity at the current iterator index is enabled.
			//! \return True it the entity is enabled. False otherwise.
			GAIA_NODISCARD bool IsEntityEnabled() const {
				return IsEntityEnabled(m_pos);
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

		struct ChunkAccessorIt: public detail::ChunkAccessorIter {
			using Mask = detail::ChunkAccessorMask;
			using Iter = detail::ChunkAccessorIter;

		private:
			archetype::Chunk& m_chunk;

			ChunkAccessorIt(archetype::Chunk& chunk, const Mask& mask, uint32_t pos, bool fwd):
					Iter(mask, pos, fwd), m_chunk(chunk) {}

		public:
			GAIA_NODISCARD ChunkAccessor operator*() const {
				return ChunkAccessor(m_chunk, index());
			}

			GAIA_NODISCARD ChunkAccessor operator->() const {
				return ChunkAccessor(m_chunk, index());
			}

			GAIA_NODISCARD static ChunkAccessorIt CreateBegin(archetype::Chunk& chunk, const Mask& mask) {
				return ChunkAccessorIt(chunk, mask, 0, true);
			}

			GAIA_NODISCARD static ChunkAccessorIt CreateEnd(archetype::Chunk& chunk, const Mask& mask) {
				return ChunkAccessorIt(chunk, mask, chunk.GetEntityCount(), false);
			}

			GAIA_NODISCARD uint32_t index() const {
				return *((Iter&)*this);
			}
		};

		struct ChunkAccessorExtIt: public detail::ChunkAccessorIndexIter {
			using Iter = detail::ChunkAccessorIndexIter;

		private:
			archetype::Chunk& m_chunk;

			ChunkAccessorExtIt(archetype::Chunk& chunk, uint32_t pos): Iter(pos), m_chunk(chunk) {}

		public:
			GAIA_NODISCARD ChunkAccessorExt operator*() const {
				return ChunkAccessorExt(m_chunk, index());
			}

			GAIA_NODISCARD ChunkAccessorExt operator->() const {
				return ChunkAccessorExt(m_chunk, index());
			}

			GAIA_NODISCARD static ChunkAccessorExtIt CreateBegin(archetype::Chunk& chunk) {
				return ChunkAccessorExtIt(chunk, 0);
			}

			GAIA_NODISCARD static ChunkAccessorExtIt CreateEnd(archetype::Chunk& chunk) {
				return ChunkAccessorExtIt(chunk, chunk.GetEntityCount());
			}

			GAIA_NODISCARD uint32_t index() const {
				return *((Iter&)*this);
			}
		};

		struct ChunkAccessorExtByIndexIt: public detail::ChunkAccessorIndexIter {
			using Iter = detail::ChunkAccessorIndexIter;

		private:
			archetype::Chunk& m_chunk;

			ChunkAccessorExtByIndexIt(archetype::Chunk& chunk, uint32_t pos): Iter(pos), m_chunk(chunk) {}

			GAIA_NODISCARD uint32_t index() const {
				return *((Iter&)*this);
			}

		public:
			uint32_t operator*() const {
				return index();
			}
			uint32_t operator->() const {
				return index();
			}

			static ChunkAccessorExtByIndexIt CreateBegin(archetype::Chunk& chunk) {
				return ChunkAccessorExtByIndexIt(chunk, 0);
			}

			static ChunkAccessorExtByIndexIt CreateEnd(archetype::Chunk& chunk) {
				return ChunkAccessorExtByIndexIt(chunk, chunk.GetEntityCount());
			}
		};
	} // namespace ecs
} // namespace gaia
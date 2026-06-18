#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <cstring>

#include "gaia/ecs/component_cache.h"
#include "gaia/ecs/component_cache_item.h"
#include "gaia/ecs/id.h"
#include "gaia/util/str.h"

namespace gaia {
	namespace ecs {
		class World;
		void world_finish_write(World& world, Entity term, Entity entity);

		//! Flags describing the state of a raw component byte view.
		//! Kept as a plain bitmask so raw views can stay compact and cheap to return by value.
		enum ComponentRawViewFlags : uint32_t {
			//! No payload was resolved.
			ComponentRawViewFlag_None = 0,
			//! The view resolved to an existing supported payload or tag.
			ComponentRawViewFlag_Valid = 1U << 0
		};

		//! Non-owning read-only view over raw component bytes on an entity.
		//!
		//! The view intentionally stores only a pointer, byte size, and flags. The component id is
		//! omitted because callers already pass it to `World::get_raw(...)`; keeping it out of the view
		//! keeps the return value compact. A valid tag is represented by `data == nullptr`, `size == 0`,
		//! and `ComponentRawViewFlag_Valid` set.
		//! \note Treat the view as a short-lived borrow. Structural changes can invalidate `data`.
		//! \note Use valid() instead of checking `data`: tags are valid views with a null pointer.
		struct ComponentRawView {
			//! Raw component payload bytes. Null for tags and invalid views.
			const void* data = nullptr;
			//! Payload size in bytes.
			uint32_t size = 0;
			//! Bitmask of ComponentRawViewFlags values.
			uint32_t flags = ComponentRawViewFlag_None;

			//! \return True if the view resolved to an existing supported component payload or tag.
			GAIA_NODISCARD bool valid() const noexcept {
				return (flags & ComponentRawViewFlag_Valid) != 0;
			}
		};
		static_assert(sizeof(ComponentRawView) == 16, "ComponentRawView must stay compact");

		//! Non-owning mutable view over raw component bytes on an entity.
		//!
		//! The view intentionally stores only a pointer, byte size, and flags. The component id is
		//! omitted because callers already pass it to `World::mut_raw(...)`; keeping it out of the view
		//! keeps the return value compact. A valid tag is represented by `data == nullptr`, `size == 0`,
		//! and `ComponentRawViewFlag_Valid` set.
		//! \note Treat the view as a short-lived borrow. Structural changes can invalidate `data`.
		//! \note `mut_raw(...)` is a silent mutation path. Call `World::modify_raw(...)` after writing
		//! through `data` directly. ComponentCursor::write_bytes() finishes the root component write itself.
		struct ComponentRawMutView {
			//! Raw component payload bytes. Null for tags and invalid views.
			void* data = nullptr;
			//! Payload size in bytes.
			uint32_t size = 0;
			//! Bitmask of ComponentRawViewFlags values.
			uint32_t flags = ComponentRawViewFlag_None;

			//! \return True if the view resolved to an existing supported component payload or tag.
			GAIA_NODISCARD bool valid() const noexcept {
				return (flags & ComponentRawViewFlag_Valid) != 0;
			}
		};
		static_assert(sizeof(ComponentRawMutView) == 16, "ComponentRawMutView must stay compact");

		//! Result of writing through a ComponentCursor.
		enum class ComponentWriteResult : uint8_t { Ok, Invalid, ReadOnly, OutOfRange };

		//! Stack-only cursor over raw component bytes and runtime field metadata.
		//!
		//! The cursor is a short-lived non-owning view. It never owns component bytes or metadata and is
		//! invalidated by structural changes that can move component storage. Field navigation is strict:
		//! missing fields, unsupported type metadata, and out-of-bounds field ranges leave the cursor unchanged.
		struct ComponentCursor {
			//! Maximum nested field depth tracked by the cursor.
			static constexpr uint32_t MaxDepth = 32;

			//! Creates an invalid cursor.
			ComponentCursor() = default;

			//! Creates a read-only cursor from a raw component view.
			//! \param components Component metadata registry used for field/type lookup.
			//! \param component Component/type entity represented by @a view.
			//! \param view Raw component view returned by World::get_raw().
			//! \return Cursor positioned at the root component payload, or invalid cursor when @a view is invalid.
			GAIA_NODISCARD static ComponentCursor
			from_raw(const ComponentCache& components, Entity component, ComponentRawView view) {
				ComponentCursor cursor{};
				if (!view.valid())
					return cursor;

				cursor.m_components = &components;
				cursor.m_stack[0].type = component;
				cursor.m_stack[0].data = view.data;
				cursor.m_stack[0].size = view.size;
				cursor.m_valid = true;
				return cursor;
			}

			//! Creates a mutable cursor from a raw component view.
			//! \param world World owning the mutated chunk storage.
			//! \param components Component metadata registry used for field/type lookup.
			//! \param entity Entity whose component payload is represented by @a view.
			//! \param component Component/type entity represented by @a view.
			//! \param view Raw component view returned by World::mut_raw().
			//! \return Cursor positioned at the root component payload, or invalid cursor when @a view is invalid.
			GAIA_NODISCARD static ComponentCursor from_raw(
					World& world, const ComponentCache& components, Entity entity, Entity component, ComponentRawMutView view) {
				ComponentCursor cursor{};
				if (!view.valid())
					return cursor;

				cursor.m_world = &world;
				cursor.m_components = &components;
				cursor.m_entity = entity;
				cursor.m_rootType = component;
				cursor.m_stack[0].type = component;
				cursor.m_stack[0].data = view.data;
				cursor.m_stack[0].mutData = view.data;
				cursor.m_stack[0].size = view.size;
				cursor.m_stack[0].writable = true;
				cursor.m_valid = true;
				return cursor;
			}

			//! \return True when the cursor points at a valid component payload, tag, or field.
			GAIA_NODISCARD bool valid() const noexcept {
				return m_valid;
			}

			//! \return Current nesting depth. Root payload is depth 0.
			GAIA_NODISCARD uint32_t depth() const noexcept {
				return m_depth;
			}

			//! \return Current component/type entity, or EntityBad for an invalid cursor.
			GAIA_NODISCARD Entity type() const noexcept {
				return m_valid ? m_stack[m_depth].type : EntityBad;
			}

			//! \return Current payload or field size in bytes.
			GAIA_NODISCARD uint32_t size() const noexcept {
				return m_valid ? m_stack[m_depth].size : 0;
			}

			//! \return Read-only pointer at the current cursor position. Null for tags and invalid cursors.
			GAIA_NODISCARD const void* ptr() const noexcept {
				return m_valid ? m_stack[m_depth].data : nullptr;
			}

			//! \return Mutable pointer at the current cursor position. Null for read-only cursors, tags, and invalid cursors.
			GAIA_NODISCARD void* mut_ptr() const noexcept {
				return m_valid ? m_stack[m_depth].mutData : nullptr;
			}

			//! \return Number of reflected fields on the current type.
			GAIA_NODISCARD uint32_t field_count() const {
				const auto* pItem = current_item();
				return pItem != nullptr ? pItem->field_count() : 0;
			}

			//! Descends into the reflected field at @a index.
			//! \param index Reflected field index on the current type.
			//! \return True when the cursor moved to the field.
			bool field(uint32_t index) {
				const auto* pItem = current_item();
				if (pItem == nullptr || index >= pItem->field_count())
					return false;
				return descend(pItem->fields[index]);
			}

			//! Descends into the reflected field named @a name.
			//! \param name Reflected field name on the current type.
			//! \return True when the cursor moved to the field.
			bool field(util::str_view name) {
				const auto* pItem = current_item();
				if (pItem == nullptr)
					return false;

				const RuntimeField* pField = nullptr;
				if (!pItem->field(name, &pField) || pField == nullptr)
					return false;
				return descend(*pField);
			}

			//! Moves back to the parent cursor scope.
			//! \return True when the cursor moved to the parent; false at root or when invalid.
			bool parent() noexcept {
				if (!m_valid || m_depth == 0)
					return false;
				--m_depth;
				return true;
			}

			//! Reads the current cursor bytes as a scalar f32 convenience value.
			//! \param value Receives the read value.
			//! \return True when the current cursor position is exactly one f32 value.
			GAIA_NODISCARD bool get_f32(float* value) const noexcept {
				if (value == nullptr || !m_valid || size() != sizeof(float) || ptr() == nullptr)
					return false;
				memcpy(value, ptr(), sizeof(float));
				return true;
			}

			//! Writes exact bytes to the current cursor position.
			//!
			//! This is the cursor write path for runtime component data. It performs no type conversion and
			//! requires @a byteCount to match the current cursor scope size. Use field navigation to choose
			//! the address being written.
			//! Successful writes finish the root component write, so normal chunk write tracking and OnSet observers run.
			//! \param data Bytes to copy. Must be non-null when @a byteCount is non-zero.
			//! \param byteCount Number of bytes to copy.
			//! Returns Ok when bytes were copied, otherwise the reason the write failed.
			ComponentWriteResult write_bytes(const void* data, uint32_t byteCount) noexcept {
				if (!m_valid)
					return ComponentWriteResult::Invalid;
				if (!m_stack[m_depth].writable || m_world == nullptr || m_entity == EntityBad || m_rootType == EntityBad)
					return ComponentWriteResult::ReadOnly;
				if (byteCount != size())
					return ComponentWriteResult::OutOfRange;
				if (byteCount == 0)
					return ComponentWriteResult::Ok;
				if (data == nullptr || mut_ptr() == nullptr)
					return ComponentWriteResult::Invalid;

				memcpy(mut_ptr(), data, byteCount);
				world_finish_write(*m_world, m_rootType, m_entity);
				return ComponentWriteResult::Ok;
			}

		private:
			struct Scope {
				Entity type = EntityBad;
				const void* data = nullptr;
				void* mutData = nullptr;
				uint32_t size = 0;
				bool writable = false;
			};

			World* m_world = nullptr;
			const ComponentCache* m_components = nullptr;
			Entity m_entity = EntityBad;
			Entity m_rootType = EntityBad;
			Scope m_stack[MaxDepth]{};
			uint32_t m_depth = 0;
			bool m_valid = false;

			GAIA_NODISCARD const ComponentCacheItem* current_item() const noexcept {
				if (!m_valid || m_components == nullptr)
					return nullptr;
				return m_components->find(m_stack[m_depth].type);
			}

			bool descend(const RuntimeField& field) noexcept {
				if (!m_valid || m_components == nullptr || m_depth + 1 >= MaxDepth)
					return false;

				const auto* pType = m_components->find(field.type);
				if (pType == nullptr)
					return false;

				const auto elemSize = pType->comp.size();
				const auto elemCount = ComponentCacheItem::field_element_count(field);
				const auto fieldSize64 = (uint64_t)elemSize * (uint64_t)elemCount;
				const auto end = (uint64_t)field.offset + fieldSize64;
				if (fieldSize64 > UINT32_MAX || end > m_stack[m_depth].size)
					return false;

				Scope next{};
				next.type = field.type;
				next.size = (uint32_t)fieldSize64;
				next.writable = m_stack[m_depth].writable;
				if (m_stack[m_depth].data != nullptr)
					next.data = (const uint8_t*)m_stack[m_depth].data + field.offset;
				if (m_stack[m_depth].mutData != nullptr)
					next.mutData = (uint8_t*)m_stack[m_depth].mutData + field.offset;

				m_stack[++m_depth] = next;
				return true;
			}
		};
	} // namespace ecs
} // namespace gaia

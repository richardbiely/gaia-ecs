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
		//! through `data` directly. ComponentCursor::set_raw() finishes the root component write itself.
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

		//! Result status for ComponentCursor operations.
		enum class CursorStatus : uint8_t { Ok, Invalid, ReadOnly, TypeMismatch, OutOfRange };

		//! Result of reading or writing through a ComponentCursor.
		//! \tparam T Value type returned by the operation.
		template <typename T>
		struct CursorResult final {
			//! Operation status.
			CursorStatus status = CursorStatus::Invalid;
			//! Value returned by a successful read operation.
			T value{};

			//! \return True when the operation succeeded.
			GAIA_NODISCARD bool ok() const noexcept {
				return status == CursorStatus::Ok;
			}

			//! \return True when the operation succeeded.
			GAIA_NODISCARD explicit operator bool() const noexcept {
				return ok();
			}
		};

		//! Result of writing through a ComponentCursor.
		template <>
		struct CursorResult<void> final {
			//! Operation status.
			CursorStatus status = CursorStatus::Invalid;

			//! \return True when the operation succeeded.
			GAIA_NODISCARD bool ok() const noexcept {
				return status == CursorStatus::Ok;
			}

			//! \return True when the operation succeeded.
			GAIA_NODISCARD explicit operator bool() const noexcept {
				return ok();
			}
		};

		//! \return True when @a result has status @a status.
		GAIA_NODISCARD inline bool operator==(CursorResult<void> result, CursorStatus status) noexcept {
			return result.status == status;
		}

		//! \return True when @a result has status @a status.
		GAIA_NODISCARD inline bool operator==(CursorStatus status, CursorResult<void> result) noexcept {
			return result.status == status;
		}

		//! \return True when @a result does not have status @a status.
		GAIA_NODISCARD inline bool operator!=(CursorResult<void> result, CursorStatus status) noexcept {
			return result.status != status;
		}

		//! \return True when @a result does not have status @a status.
		GAIA_NODISCARD inline bool operator!=(CursorStatus status, CursorResult<void> result) noexcept {
			return result.status != status;
		}

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
				cursor.m_stack[0].elemSize = view.size;
				cursor.m_stack[0].elemCount = 1;
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
				cursor.m_stack[0].elemSize = view.size;
				cursor.m_stack[0].elemCount = 1;
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
				if (!m_valid || m_stack[m_depth].elemCount != 1)
					return 0;
				const auto* pItem = current_item();
				return pItem != nullptr ? pItem->field_count() : 0;
			}

			//! Descends into the reflected field at @a index.
			//! \param index Reflected field index on the current type.
			//! \return True when the cursor moved to the field.
			bool field(uint32_t index) {
				if (!m_valid || m_stack[m_depth].elemCount != 1)
					return false;
				const auto* pItem = current_item();
				if (pItem == nullptr)
					return false;

				const RuntimeField* pField = pItem->field(index);
				return pField != nullptr ? descend(*pField) : false;
			}

			//! Descends into the reflected field named @a name.
			//! \param name Reflected field name on the current type.
			//! \return True when the cursor moved to the field.
			bool field(util::str_view name) {
				if (!m_valid || m_stack[m_depth].elemCount != 1)
					return false;
				const auto* pItem = current_item();
				if (pItem == nullptr)
					return false;

				const RuntimeField* pField = pItem->field(name);
				return pField != nullptr ? descend(*pField) : false;
			}

			//! Descends into element @a index of the current fixed inline array field.
			//! \param index Element index inside the current field.
			//! \return True when the cursor moved to the element.
			bool elem(uint32_t index) noexcept {
				if (!m_valid || m_depth + 1 >= MaxDepth)
					return false;

				const auto& current = m_stack[m_depth];
				if (current.elemCount <= 1 || index >= current.elemCount || current.elemSize == 0)
					return false;

				Scope next{};
				next.type = current.type;
				next.size = current.elemSize;
				next.elemSize = current.elemSize;
				next.elemCount = 1;
				next.writable = current.writable;
				if (current.data != nullptr)
					next.data = (const uint8_t*)current.data + (uint64_t)current.elemSize * index;
				if (current.mutData != nullptr)
					next.mutData = (uint8_t*)current.mutData + (uint64_t)current.elemSize * index;

				m_stack[++m_depth] = next;
				return true;
			}

			//! Moves back to the parent cursor scope.
			//! \return True when the cursor moved to the parent; false at root or when invalid.
			bool parent() noexcept {
				if (!m_valid || m_depth == 0)
					return false;
				--m_depth;
				return true;
			}

			//! Reads the current cursor value as a bool.
			//! \return Read result and value.
			GAIA_NODISCARD CursorResult<bool> b() const noexcept {
				return read_primitive<bool>(Bool);
			}

			//! Writes @a value to the current cursor value as a bool.
			//! \param value Value to write.
			//! \return Write result.
			GAIA_NODISCARD CursorResult<void> b(bool value) noexcept {
				return write_primitive(Bool, value);
			}

			//! Reads the current cursor value as a scalar c8.
			//! \return Read result and value.
			GAIA_NODISCARD CursorResult<char> c8() const noexcept {
				return read_primitive<char>(Char8);
			}

			//! Writes @a value to the current cursor value as a scalar c8.
			//! \param value Value to write.
			//! \return Write result.
			GAIA_NODISCARD CursorResult<void> c8(char value) noexcept {
				return write_primitive(Char8, value);
			}

			//! Reads the current fixed c8 buffer.
			//! \param dst Destination buffer receiving bytes.
			//! \param cap Destination buffer size in bytes. Must be at least the reflected field count.
			//! \return Read result and copied byte count.
			GAIA_NODISCARD CursorResult<uint32_t> c8(char* dst, uint32_t cap) noexcept {
				return const_cast<const ComponentCursor&>(*this).c8(dst, cap);
			}

			//! Reads the current fixed c8 buffer.
			//! \param dst Destination buffer receiving bytes.
			//! \param cap Destination buffer size in bytes. Must be at least the reflected field count.
			//! \return Read result and copied byte count.
			GAIA_NODISCARD CursorResult<uint32_t> c8(char* dst, uint32_t cap) const noexcept {
				CursorResult<uint32_t> result{};
				const auto status = validate_primitive(Char8, false, false);
				if (status != CursorStatus::Ok) {
					result.status = status;
					return result;
				}
				if (m_stack[m_depth].elemCount <= 1) {
					result.status = CursorStatus::TypeMismatch;
					return result;
				}
				if (dst == nullptr) {
					result.status = CursorStatus::Invalid;
					return result;
				}
				if (cap < size()) {
					result.status = CursorStatus::OutOfRange;
					return result;
				}

				memcpy(dst, ptr(), size());
				result.status = CursorStatus::Ok;
				result.value = size();
				return result;
			}

			//! Writes bytes to the current fixed c8 buffer.
			//! \param src Source bytes to copy.
			//! \param len Source byte count. Must fit in the reflected field count.
			//! \return Write result.
			GAIA_NODISCARD CursorResult<void> c8(const char* src, uint32_t len) noexcept {
				const auto status = validate_primitive(Char8, true, false);
				if (status != CursorStatus::Ok)
					return {status};
				if (m_stack[m_depth].elemCount <= 1)
					return {CursorStatus::TypeMismatch};
				if (len > size())
					return {CursorStatus::OutOfRange};
				if (len != 0 && src == nullptr)
					return {CursorStatus::Invalid};
				if (len != 0)
					memcpy(mut_ptr(), src, len);
				world_finish_write(*m_world, m_rootType, m_entity);
				return {CursorStatus::Ok};
			}

			//! Reads the current cursor value as a scalar c16.
			//! \return Read result and value.
			GAIA_NODISCARD CursorResult<char16_t> c16() const noexcept {
				return read_primitive<char16_t>(Char16);
			}

			//! Writes @a value to the current cursor value as a scalar c16.
			//! \param value Value to write.
			//! \return Write result.
			GAIA_NODISCARD CursorResult<void> c16(char16_t value) noexcept {
				return write_primitive(Char16, value);
			}

			//! Reads the current cursor value as a scalar c32.
			//! \return Read result and value.
			GAIA_NODISCARD CursorResult<char32_t> c32() const noexcept {
				return read_primitive<char32_t>(Char32);
			}

			//! Writes @a value to the current cursor value as a scalar c32.
			//! \param value Value to write.
			//! \return Write result.
			GAIA_NODISCARD CursorResult<void> c32(char32_t value) noexcept {
				return write_primitive(Char32, value);
			}

			//! Reads the current cursor value as an s8.
			GAIA_NODISCARD CursorResult<int8_t> s8() const noexcept {
				return read_primitive<int8_t>(S8);
			}

			//! Writes @a value to the current cursor value as an s8.
			GAIA_NODISCARD CursorResult<void> s8(int8_t value) noexcept {
				return write_primitive(S8, value);
			}

			//! Reads the current cursor value as a u8.
			GAIA_NODISCARD CursorResult<uint8_t> u8() const noexcept {
				return read_primitive<uint8_t>(U8);
			}

			//! Writes @a value to the current cursor value as a u8.
			GAIA_NODISCARD CursorResult<void> u8(uint8_t value) noexcept {
				return write_primitive(U8, value);
			}

			//! Reads the current cursor value as an s16.
			GAIA_NODISCARD CursorResult<int16_t> s16() const noexcept {
				return read_primitive<int16_t>(S16);
			}

			//! Writes @a value to the current cursor value as an s16.
			GAIA_NODISCARD CursorResult<void> s16(int16_t value) noexcept {
				return write_primitive(S16, value);
			}

			//! Reads the current cursor value as a u16.
			GAIA_NODISCARD CursorResult<uint16_t> u16() const noexcept {
				return read_primitive<uint16_t>(U16);
			}

			//! Writes @a value to the current cursor value as a u16.
			GAIA_NODISCARD CursorResult<void> u16(uint16_t value) noexcept {
				return write_primitive(U16, value);
			}

			//! Reads the current cursor value as an s32.
			GAIA_NODISCARD CursorResult<int32_t> s32() const noexcept {
				return read_primitive<int32_t>(S32);
			}

			//! Writes @a value to the current cursor value as an s32.
			GAIA_NODISCARD CursorResult<void> s32(int32_t value) noexcept {
				return write_primitive(S32, value);
			}

			//! Reads the current cursor value as a u32.
			GAIA_NODISCARD CursorResult<uint32_t> u32() const noexcept {
				return read_primitive<uint32_t>(U32);
			}

			//! Writes @a value to the current cursor value as a u32.
			GAIA_NODISCARD CursorResult<void> u32(uint32_t value) noexcept {
				return write_primitive(U32, value);
			}

			//! Reads the current cursor value as an s64.
			GAIA_NODISCARD CursorResult<int64_t> s64() const noexcept {
				return read_primitive<int64_t>(S64);
			}

			//! Writes @a value to the current cursor value as an s64.
			GAIA_NODISCARD CursorResult<void> s64(int64_t value) noexcept {
				return write_primitive(S64, value);
			}

			//! Reads the current cursor value as a u64.
			GAIA_NODISCARD CursorResult<uint64_t> u64() const noexcept {
				return read_primitive<uint64_t>(U64);
			}

			//! Writes @a value to the current cursor value as a u64.
			GAIA_NODISCARD CursorResult<void> u64(uint64_t value) noexcept {
				return write_primitive(U64, value);
			}

			//! Reads the current cursor value as an f32.
			GAIA_NODISCARD CursorResult<float> f32() const noexcept {
				return read_primitive<float>(F32);
			}

			//! Writes @a value to the current cursor value as an f32.
			GAIA_NODISCARD CursorResult<void> f32(float value) noexcept {
				return write_primitive(F32, value);
			}

			//! Reads the current cursor value as an f64.
			GAIA_NODISCARD CursorResult<double> f64() const noexcept {
				return read_primitive<double>(F64);
			}

			//! Writes @a value to the current cursor value as an f64.
			GAIA_NODISCARD CursorResult<void> f64(double value) noexcept {
				return write_primitive(F64, value);
			}

			//! Copies exact bytes from the current cursor position.
			//!
			//! This is the cursor read path for runtime component data. It performs no type conversion and
			//! copies the whole current cursor scope. Use field navigation to choose the address being read.
			//! \param data Destination bytes. Must be non-null when the current scope size is non-zero.
			//! \param byteCount Destination capacity in bytes. Must be at least the current scope size.
			//! \return Ok and copied byte count when bytes were copied, otherwise the reason the read failed.
			GAIA_NODISCARD CursorResult<uint32_t> get_raw(void* data, uint32_t byteCount) const noexcept {
				CursorResult<uint32_t> result{};
				if (!m_valid) {
					result.status = CursorStatus::Invalid;
					return result;
				}
				if (byteCount < size()) {
					result.status = CursorStatus::OutOfRange;
					return result;
				}
				if (size() != 0 && (data == nullptr || ptr() == nullptr)) {
					result.status = CursorStatus::Invalid;
					return result;
				}
				if (size() != 0)
					memcpy(data, ptr(), size());
				result.status = CursorStatus::Ok;
				result.value = size();
				return result;
			}

			//! Writes exact bytes to the current cursor position.
			//!
			//! This is the cursor write path for runtime component data. It performs no type conversion and
			//! requires @a byteCount to match the current cursor scope size. Use field navigation to choose
			//! the address being written.
			//! Successful writes finish the root component write, so normal chunk write tracking and OnSet observers run.
			//! \param data Bytes to copy. Must be non-null when @a byteCount is non-zero.
			//! \param byteCount Number of bytes to copy.
			//! \return Ok when bytes were copied, otherwise the reason the write failed.
			CursorResult<void> set_raw(const void* data, uint32_t byteCount) noexcept {
				if (!m_valid)
					return {CursorStatus::Invalid};
				if (!m_stack[m_depth].writable || m_world == nullptr || m_entity == EntityBad || m_rootType == EntityBad)
					return {CursorStatus::ReadOnly};
				if (byteCount != size())
					return {CursorStatus::OutOfRange};
				if (byteCount == 0)
					return {CursorStatus::Ok};
				if (data == nullptr || mut_ptr() == nullptr)
					return {CursorStatus::Invalid};

				memcpy(mut_ptr(), data, byteCount);
				world_finish_write(*m_world, m_rootType, m_entity);
				return {CursorStatus::Ok};
			}

		private:
			struct Scope {
				Entity type = EntityBad;
				const void* data = nullptr;
				void* mutData = nullptr;
				uint32_t size = 0;
				uint32_t elemSize = 0;
				uint32_t elemCount = 1;
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

			GAIA_NODISCARD CursorStatus
			validate_primitive(Entity expectedType, bool requireWrite, bool requireScalar) const noexcept {
				if (!m_valid)
					return CursorStatus::Invalid;
				const auto& current = m_stack[m_depth];
				if (current.type != expectedType) {
					const auto* pCurrentType = m_components != nullptr ? m_components->find(current.type) : nullptr;
					if (pCurrentType == nullptr || pCurrentType->primitive_type() != expectedType)
						return CursorStatus::TypeMismatch;
				}
				if (requireScalar && current.elemCount != 1)
					return CursorStatus::TypeMismatch;
				if (current.size != current.elemSize * current.elemCount)
					return CursorStatus::OutOfRange;
				if (current.size == 0)
					return CursorStatus::TypeMismatch;
				if (ptr() == nullptr)
					return CursorStatus::Invalid;
				if (requireWrite) {
					if (!current.writable || m_world == nullptr || m_entity == EntityBad || m_rootType == EntityBad)
						return CursorStatus::ReadOnly;
					if (mut_ptr() == nullptr)
						return CursorStatus::Invalid;
				}
				return CursorStatus::Ok;
			}

			template <typename T>
			GAIA_NODISCARD CursorResult<T> read_primitive(Entity expectedType) const noexcept {
				CursorResult<T> result{};
				const auto status = validate_primitive(expectedType, false, true);
				if (status != CursorStatus::Ok) {
					result.status = status;
					return result;
				}
				if (size() != sizeof(T)) {
					result.status = CursorStatus::TypeMismatch;
					return result;
				}

				memcpy(&result.value, ptr(), sizeof(T));
				result.status = CursorStatus::Ok;
				return result;
			}

			template <typename T>
			GAIA_NODISCARD CursorResult<void> write_primitive(Entity expectedType, const T& value) noexcept {
				const auto status = validate_primitive(expectedType, true, true);
				if (status != CursorStatus::Ok)
					return {status};
				if (size() != sizeof(T))
					return {CursorStatus::TypeMismatch};

				memcpy(mut_ptr(), &value, sizeof(T));
				world_finish_write(*m_world, m_rootType, m_entity);
				return {CursorStatus::Ok};
			}

			bool descend(const RuntimeField& field) noexcept {
				if (!m_valid || m_components == nullptr || m_depth + 1 >= MaxDepth)
					return false;

				const auto* pType = m_components->find(field.type);
				if (pType == nullptr)
					return false;

				Entity scopeType = field.type;
				uint32_t elemSize = pType->comp.size();
				uint32_t elemCount = ComponentCacheItem::field_element_count(field);
				if (pType->typeKind == RuntimeTypeKind::Array) {
					if (field.count != 0)
						return false;
					const auto elementType = pType->array_element_type();
					const auto* pElementType = m_components->find(elementType);
					if (pElementType == nullptr || pElementType->typeKind == RuntimeTypeKind::Array ||
							pType->array_element_count() == 0)
						return false;
					scopeType = elementType;
					elemSize = pElementType->comp.size();
					elemCount = pType->array_element_count();
				}
				const auto fieldSize64 = (uint64_t)elemSize * (uint64_t)elemCount;
				const auto end = (uint64_t)field.offset + fieldSize64;
				if (fieldSize64 > UINT32_MAX || end > m_stack[m_depth].size)
					return false;

				Scope next{};
				next.type = scopeType;
				next.size = (uint32_t)fieldSize64;
				next.elemSize = elemSize;
				next.elemCount = elemCount;
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

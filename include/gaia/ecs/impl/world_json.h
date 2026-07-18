#pragma once
#include "gaia/config/config.h"

#include <cstdio>
#include <cstring>

#include "gaia/ser/ser_json.h"

namespace gaia {
	namespace ecs {
		//! \cond INTERNAL
		namespace detail {
			static constexpr uint32_t RuntimeJsonMaxDepth = 32;

			GAIA_NODISCARD inline bool
			runtime_type_json_type(const ComponentCacheItem& typeItem, ser::serialization_type_id& out) noexcept {
				const auto primitiveType = typeItem.primitive_type();
				if (primitiveType != EntityBad)
					return runtime_primitive_serialization_type(primitiveType, out);
				out = ser::serialization_type_id::ignore;
				return false;
			}

			GAIA_NODISCARD inline bool
			runtime_json_is_char8_type(const ComponentCacheItem* pType, Entity typeEntity) noexcept {
				ser::serialization_type_id type = ser::serialization_type_id::ignore;
				if (pType != nullptr)
					return runtime_type_json_type(*pType, type) && type == ser::serialization_type_id::c8;
				return runtime_primitive_serialization_type(typeEntity, type) && type == ser::serialization_type_id::c8;
			}

			GAIA_NODISCARD inline const ComponentCacheItem*
			find_runtime_json_type(const ComponentCache* pCache, Entity typeEntity) noexcept {
				return pCache != nullptr ? pCache->find(typeEntity) : nullptr;
			}

			GAIA_NODISCARD inline bool
			runtime_json_type_size(const ComponentCacheItem* pType, Entity typeEntity, uint32_t& outSize) noexcept {
				if (pType != nullptr) {
					outSize = pType->comp.size();
					return true;
				}
				outSize = ComponentCacheItem::primitive_type_size(typeEntity);
				return outSize != 0;
			}

			struct RuntimeJsonFieldLayout final {
				const ComponentCacheItem* pType = nullptr;
				Entity type = EntityBad;
				uint32_t elemSize = 0;
				uint32_t elemCount = 0;
			};

			GAIA_NODISCARD inline bool resolve_runtime_json_field_layout(
					const ComponentCache* pCache, const RuntimeField& field, RuntimeJsonFieldLayout& out) noexcept {
				out = {};
				const auto* pFieldType = find_runtime_json_type(pCache, field.type);
				out.type = field.type;
				out.pType = pFieldType;
				out.elemCount = ComponentCacheItem::field_element_count(field);

				if (pFieldType != nullptr && pFieldType->typeKind == RuntimeTypeKind::Array) {
					if (field.count != 0)
						return false;
					out.type = pFieldType->element_type();
					out.elemCount = pFieldType->element_count();
					out.pType = find_runtime_json_type(pCache, out.type);
				}

				return out.elemCount != 0 && runtime_json_type_size(out.pType, out.type, out.elemSize);
			}

			//! Reads an integer runtime value as its width-preserving unsigned bit pattern.
			//! \param pData Runtime value bytes.
			//! \param type Reflected integer storage type.
			//! \param valueSize Size of \a pData in bytes.
			//! \param out Receives the normalized bit pattern.
			//! \return True when \a type is a supported integer type and \a valueSize matches it.
			GAIA_NODISCARD inline bool runtime_json_integer_bits(
					const uint8_t* pData, ser::serialization_type_id type, uint32_t valueSize, uint64_t& out) noexcept {
				switch (type) {
					case ser::serialization_type_id::s8:
					case ser::serialization_type_id::u8: {
						if (valueSize != sizeof(uint8_t))
							return false;
						uint8_t value = 0;
						memcpy(&value, pData, sizeof(value));
						out = value;
						return true;
					}
					case ser::serialization_type_id::s16:
					case ser::serialization_type_id::u16: {
						if (valueSize != sizeof(uint16_t))
							return false;
						uint16_t value = 0;
						memcpy(&value, pData, sizeof(value));
						out = value;
						return true;
					}
					case ser::serialization_type_id::s32:
					case ser::serialization_type_id::u32: {
						if (valueSize != sizeof(uint32_t))
							return false;
						uint32_t value = 0;
						memcpy(&value, pData, sizeof(value));
						out = value;
						return true;
					}
					case ser::serialization_type_id::s64:
					case ser::serialization_type_id::u64: {
						if (valueSize != sizeof(uint64_t))
							return false;
						memcpy(&out, pData, sizeof(out));
						return true;
					}
					default:
						return false;
				}
			}

			//! Normalizes a registered runtime constant to its underlying integer width.
			//! \param type Reflected integer storage type.
			//! \param value Registered constant value.
			//! \param out Receives the normalized bit pattern.
			//! \return True when \a type is a supported integer type.
			GAIA_NODISCARD inline bool
			runtime_json_constant_bits(ser::serialization_type_id type, int64_t value, uint64_t& out) noexcept {
				switch (type) {
					case ser::serialization_type_id::s8:
						if (value < INT8_MIN || value > INT8_MAX)
							return false;
						out = (uint8_t)value;
						return true;
					case ser::serialization_type_id::u8:
						if (value < 0 || value > UINT8_MAX)
							return false;
						out = (uint8_t)value;
						return true;
					case ser::serialization_type_id::s16:
						if (value < INT16_MIN || value > INT16_MAX)
							return false;
						out = (uint16_t)value;
						return true;
					case ser::serialization_type_id::u16:
						if (value < 0 || value > UINT16_MAX)
							return false;
						out = (uint16_t)value;
						return true;
					case ser::serialization_type_id::s32:
						if (value < INT32_MIN || value > INT32_MAX)
							return false;
						out = (uint32_t)value;
						return true;
					case ser::serialization_type_id::u32:
						if (value < 0 || (uint64_t)value > UINT32_MAX)
							return false;
						out = (uint32_t)value;
						return true;
					case ser::serialization_type_id::s64:
						out = (uint64_t)value;
						return true;
					case ser::serialization_type_id::u64:
						if (value < 0)
							return false;
						out = (uint64_t)value;
						return true;
					default:
						return false;
				}
			}

			//! Writes a width-preserving integer bit pattern to runtime value bytes.
			//! \param pData Destination runtime value bytes.
			//! \param type Reflected integer storage type.
			//! \param valueSize Size of \a pData in bytes.
			//! \param value Normalized bit pattern to write.
			//! \return True when \a type is a supported integer type and \a valueSize matches it.
			GAIA_NODISCARD inline bool runtime_json_write_integer_bits(
					uint8_t* pData, ser::serialization_type_id type, uint32_t valueSize, uint64_t value) noexcept {
				switch (type) {
					case ser::serialization_type_id::s8:
					case ser::serialization_type_id::u8: {
						if (valueSize != sizeof(uint8_t))
							return false;
						const auto narrowed = (uint8_t)value;
						memcpy(pData, &narrowed, sizeof(narrowed));
						return true;
					}
					case ser::serialization_type_id::s16:
					case ser::serialization_type_id::u16: {
						if (valueSize != sizeof(uint16_t))
							return false;
						const auto narrowed = (uint16_t)value;
						memcpy(pData, &narrowed, sizeof(narrowed));
						return true;
					}
					case ser::serialization_type_id::s32:
					case ser::serialization_type_id::u32: {
						if (valueSize != sizeof(uint32_t))
							return false;
						const auto narrowed = (uint32_t)value;
						memcpy(pData, &narrowed, sizeof(narrowed));
						return true;
					}
					case ser::serialization_type_id::s64:
					case ser::serialization_type_id::u64:
						if (valueSize != sizeof(uint64_t))
							return false;
						memcpy(pData, &value, sizeof(value));
						return true;
					default:
						return false;
				}
			}

			//! \return Whether \a item is represented by one direct semantic JSON value rather than a keyed field object.
			GAIA_NODISCARD inline bool runtime_json_is_direct_value(const ComponentCacheItem& item) noexcept {
				switch (item.typeKind) {
					case RuntimeTypeKind::Primitive:
					case RuntimeTypeKind::Enum:
					case RuntimeTypeKind::Bitmask:
					case RuntimeTypeKind::Array:
					case RuntimeTypeKind::Vector:
						return true;
					case RuntimeTypeKind::Opaque:
						return item.opaque_adapter() != nullptr;
					default:
						return false;
				}
			}

			GAIA_NODISCARD inline ser::json_str
			make_runtime_json_child_path(ser::json_str_view parent, ser::json_str_view child) {
				if (parent.empty())
					return ser::json_str(child);
				if (child.empty())
					return ser::json_str(parent);

				ser::json_str path;
				path.reserve(parent.size() + 1 + child.size());
				path.append(parent.data(), parent.size());
				path.append('.');
				path.append(child.data(), child.size());
				return path;
			}

			GAIA_NODISCARD inline ser::json_str make_runtime_json_element_path(ser::json_str_view parent, uint32_t index) {
				ser::json_str path(parent);
				path.append('[');
				char idx[16]{};
				const auto len = (uint32_t)snprintf(idx, sizeof(idx), "%u", index);
				path.append(idx, len);
				path.append(']');
				return path;
			}

			GAIA_NODISCARD inline bool count_runtime_json_array_elements(ser::ser_json& reader, uint32_t& outCount) {
				outCount = 0;
				reader.ws();
				const auto* start = reader.pos();
				const auto* end = reader.end();
				if (start == nullptr || end == nullptr || start > end)
					return false;

				ser::ser_json counter(start, (uint32_t)(end - start));
				if (!counter.expect('['))
					return false;
				counter.ws();
				if (counter.consume(']'))
					return true;

				while (true) {
					if (!counter.skip_value())
						return false;
					++outCount;
					if (counter.consume(','))
						continue;
					return counter.consume(']');
				}
			}

			inline bool write_runtime_json_value(
					const ComponentCache* pCache, const ComponentCacheItem* pType, Entity typeEntity, const uint8_t* pData,
					uint32_t valueSize, ser::ser_json& writer, const ser::RuntimeJsonPolicy& policy, uint32_t depth);

			inline bool write_runtime_json_field(
					const ComponentCache* pCache, const ComponentCacheItem& owner, const RuntimeField& field,
					const uint8_t* pBase, ser::ser_json& writer, const ser::RuntimeJsonPolicy& policy, uint32_t depth) {
				RuntimeJsonFieldLayout layout{};
				if (!resolve_runtime_json_field_layout(pCache, field, layout)) {
					writer.value_null();
					return false;
				}

				const auto fieldSize64 = (uint64_t)layout.elemSize * (uint64_t)layout.elemCount;
				const auto end = (uint64_t)field.offset + fieldSize64;
				if (layout.elemSize == 0 || fieldSize64 > UINT32_MAX || end > owner.comp.size()) {
					writer.value_null();
					return false;
				}

				const auto* pFieldData = pBase + field.offset;
				if (layout.elemCount == 1 || runtime_json_is_char8_type(layout.pType, layout.type))
					return write_runtime_json_value(
							pCache, layout.pType, layout.type, pFieldData, (uint32_t)fieldSize64, writer, policy, depth + 1);

				bool ok = true;
				writer.begin_array();
				GAIA_FOR(layout.elemCount) {
					const auto* pElemData = pFieldData + (uintptr_t)layout.elemSize * i;
					ok = write_runtime_json_value(
									 pCache, layout.pType, layout.type, pElemData, layout.elemSize, writer, policy, depth + 1) &&
							 ok;
				}
				writer.end_array();
				return ok;
			}

			inline bool write_runtime_json_struct(
					const ComponentCache* pCache, const ComponentCacheItem& item, const uint8_t* pData, ser::ser_json& writer,
					const ser::RuntimeJsonPolicy& policy, uint32_t depth) {
				if (depth >= RuntimeJsonMaxDepth) {
					writer.value_null();
					return false;
				}

				bool ok = true;
				writer.begin_object();
				GAIA_FOR(item.field_count()) {
					const auto* pField = item.field(i);
					GAIA_ASSERT(pField != nullptr);
					const auto& field = *pField;
					writer.key(field.name);
					ok = write_runtime_json_field(pCache, item, field, pData, writer, policy, depth + 1) && ok;
				}
				writer.end_object();
				return ok;
			}

			inline bool write_runtime_json_value(
					const ComponentCache* pCache, const ComponentCacheItem* pType, Entity typeEntity, const uint8_t* pData,
					uint32_t valueSize, ser::ser_json& writer, const ser::RuntimeJsonPolicy& policy, uint32_t depth) {
				if (depth >= RuntimeJsonMaxDepth) {
					writer.value_null();
					return false;
				}

				if (pType != nullptr && pType->typeKind == RuntimeTypeKind::Array) {
					const auto elemCount = pType->element_count();
					const auto elementType = pType->element_type();
					const auto* pElementType = find_runtime_json_type(pCache, elementType);
					uint32_t elemSize = 0;
					if (elemCount == 0 || !runtime_json_type_size(pElementType, elementType, elemSize) ||
							(uint64_t)elemSize * elemCount != valueSize) {
						writer.value_null();
						return false;
					}

					bool ok = true;
					writer.begin_array();
					GAIA_FOR(elemCount) {
						const auto* pElemData = pData + (uintptr_t)elemSize * i;
						ok = write_runtime_json_value(
										 pCache, pElementType, elementType, pElemData, elemSize, writer, policy, depth + 1) &&
								 ok;
					}
					writer.end_array();
					return ok;
				}

				if (pType != nullptr && pType->typeKind == RuntimeTypeKind::Vector) {
					const auto* adapter = pType->sequence_adapter();
					const auto elementType = pType->element_type();
					const auto* pElementType = find_runtime_json_type(pCache, elementType);
					if (adapter == nullptr || adapter->count == nullptr || adapter->element == nullptr) {
						writer.value_null();
						return false;
					}
					RuntimeSequenceScope sequence{typeEntity, pData, nullptr, valueSize};
					uint32_t elemCount = 0;
					if (!adapter->count(adapter->ctx, sequence, elemCount)) {
						writer.value_null();
						return false;
					}

					bool ok = true;
					writer.begin_array();
					GAIA_FOR(elemCount) {
						RuntimeSequenceElement element{};
						element.type = elementType;
						if (!adapter->element(adapter->ctx, sequence, i, element)) {
							writer.value_null();
							ok = false;
							continue;
						}
						if (element.type == EntityBad)
							element.type = elementType;
						ok = write_runtime_json_value(
										 pCache, pElementType, element.type, (const uint8_t*)element.data, element.size, writer, policy,
										 depth + 1) &&
								 ok;
					}
					writer.end_array();
					return ok;
				}

				if (pType != nullptr && pType->typeKind == RuntimeTypeKind::Opaque) {
					const auto* adapter = pType->opaque_adapter();
					const auto semanticType = pType->opaque_as_type();
					const auto* pSemanticType = find_runtime_json_type(pCache, semanticType);
					if (adapter == nullptr || adapter->project == nullptr || pSemanticType == nullptr) {
						writer.value_null();
						return false;
					}
					RuntimeOpaqueScope opaque{typeEntity, pData, nullptr, valueSize};
					RuntimeOpaqueValue projected{};
					projected.type = semanticType;
					if (!adapter->project(adapter->ctx, opaque, projected)) {
						writer.value_null();
						return false;
					}
					if (projected.type == EntityBad)
						projected.type = semanticType;
					if (projected.type != semanticType || projected.data == nullptr) {
						writer.value_null();
						return false;
					}
					return write_runtime_json_value(
							pCache, pSemanticType, projected.type, (const uint8_t*)projected.data, projected.size, writer, policy,
							depth + 1);
				}

				if (pType != nullptr && pType->typeKind == RuntimeTypeKind::Struct)
					return write_runtime_json_struct(pCache, *pType, pData, writer, policy, depth + 1);

				ser::serialization_type_id type = ser::serialization_type_id::ignore;
				if (pType != nullptr) {
					if (!runtime_type_json_type(*pType, type)) {
						writer.value_null();
						return false;
					}
				} else if (!runtime_primitive_serialization_type(typeEntity, type)) {
					writer.value_null();
					return false;
				}

				uint64_t valueBits = 0;
				if (pType != nullptr && policy.symbolicEnums && pType->typeKind == RuntimeTypeKind::Enum &&
						runtime_json_integer_bits(pData, type, valueSize, valueBits)) {
					GAIA_FOR(pType->constant_count()) {
						const auto* pConstant = pType->constant(i);
						GAIA_ASSERT(pConstant != nullptr);
						uint64_t constantBits = 0;
						if (runtime_json_constant_bits(type, pConstant->value, constantBits) && constantBits == valueBits) {
							writer.value_string(pConstant->name);
							return true;
						}
					}
				}

				if (pType != nullptr && policy.symbolicBitmasks && pType->typeKind == RuntimeTypeKind::Bitmask &&
						runtime_json_integer_bits(pData, type, valueSize, valueBits)) {
					uint64_t remaining = valueBits;
					GAIA_FOR(pType->constant_count()) {
						const auto* pConstant = pType->constant(i);
						GAIA_ASSERT(pConstant != nullptr);
						uint64_t flagBits = 0;
						if (runtime_json_constant_bits(type, pConstant->value, flagBits) && flagBits != 0 &&
								(flagBits & (flagBits - 1)) == 0 && (remaining & flagBits) == flagBits)
							remaining &= ~flagBits;
					}

					if (remaining == 0) {
						writer.begin_array();
						remaining = valueBits;
						GAIA_FOR(pType->constant_count()) {
							const auto* pConstant = pType->constant(i);
							GAIA_ASSERT(pConstant != nullptr);
							uint64_t flagBits = 0;
							if (runtime_json_constant_bits(type, pConstant->value, flagBits) && flagBits != 0 &&
									(flagBits & (flagBits - 1)) == 0 && (remaining & flagBits) == flagBits) {
								writer.value_string(pConstant->name);
								remaining &= ~flagBits;
							}
						}
						writer.end_array();
						return true;
					}
				}

				return ser::detail::write_runtime_field_json(writer, pData, type, valueSize);
			}

			inline bool read_runtime_json_value(
					const ComponentCache* pCache, const ComponentCacheItem* pType, Entity typeEntity, uint8_t* pData,
					uint32_t valueSize, ser::ser_json& reader, ser::JsonDiagnostics& diagnostics, ser::json_str_view path,
					const ser::RuntimeJsonPolicy& policy, uint32_t depth, bool& ok);

			inline void warn_runtime_json(
					ser::JsonDiagnostics& diagnostics, ser::JsonDiagReason reason, ser::json_str_view path, const char* message) {
				diagnostics.add(ser::JsonDiagSeverity::Warning, reason, path, message);
			}

			inline bool read_runtime_json_field(
					const ComponentCache* pCache, const ComponentCacheItem& owner, const RuntimeField& field, uint8_t* pBase,
					ser::ser_json& reader, ser::JsonDiagnostics& diagnostics, ser::json_str_view path,
					const ser::RuntimeJsonPolicy& policy, uint32_t depth, bool& ok) {
				RuntimeJsonFieldLayout layout{};
				if (!resolve_runtime_json_field_layout(pCache, field, layout)) {
					ok = false;
					warn_runtime_json(
							diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path,
							"Runtime field uses an unknown reflected type.");
					return reader.skip_value();
				}

				const auto fieldSize64 = (uint64_t)layout.elemSize * (uint64_t)layout.elemCount;
				const auto end = (uint64_t)field.offset + fieldSize64;
				if (layout.elemSize == 0 || fieldSize64 > UINT32_MAX || end > owner.comp.size()) {
					ok = false;
					warn_runtime_json(
							diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path,
							"Runtime field points outside component size or uses an unsupported type.");
					return reader.skip_value();
				}

				auto* pFieldData = pBase + field.offset;
				if (layout.elemCount == 1 || runtime_json_is_char8_type(layout.pType, layout.type))
					return read_runtime_json_value(
							pCache, layout.pType, layout.type, pFieldData, (uint32_t)fieldSize64, reader, diagnostics, path, policy,
							depth + 1, ok);

				if (!reader.expect('['))
					return false;

				GAIA_FOR(layout.elemCount) {
					if (i > 0 && !reader.expect(','))
						return false;
					const auto elemPath = make_runtime_json_element_path(path, i);
					auto* pElemData = pFieldData + (uintptr_t)layout.elemSize * i;
					if (!read_runtime_json_value(
									pCache, layout.pType, layout.type, pElemData, layout.elemSize, reader, diagnostics, elemPath, policy,
									depth + 1, ok))
						return false;
				}
				return reader.expect(']');
			}

			inline bool read_runtime_json_struct(
					const ComponentCache* pCache, const ComponentCacheItem& item, uint8_t* pData, ser::ser_json& reader,
					ser::JsonDiagnostics& diagnostics, ser::json_str_view path, const ser::RuntimeJsonPolicy& policy,
					uint32_t depth, bool& ok) {
				if (depth >= RuntimeJsonMaxDepth) {
					ok = false;
					warn_runtime_json(
							diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path, "Runtime JSON nesting is too deep.");
					return reader.skip_value();
				}

				if (reader.parse_null()) {
					ok = false;
					warn_runtime_json(
							diagnostics, ser::JsonDiagReason::NullComponentPayload, path, "Runtime object payload is null.");
					return true;
				}

				if (!reader.expect('{'))
					return false;

				reader.ws();
				if (reader.consume('}'))
					return true;

				while (true) {
					ser::json_str_view key;
					bool keyFromScratch = false;
					if (!reader.parse_string_view(key, &keyFromScratch))
						return false;
					ser::json_str keyStorage;
					if (keyFromScratch) {
						keyStorage.assign(key.data(), key.size());
						key = keyStorage;
					}
					if (!reader.expect(':'))
						return false;

					const auto* pField = item.field(util::str_view(key.data(), (uint32_t)key.size()));
					const auto fieldPath = make_runtime_json_child_path(path, key);
					if (pField == nullptr) {
						ok = false;
						warn_runtime_json(diagnostics, ser::JsonDiagReason::UnknownField, fieldPath, "Unknown runtime field.");
						if (!reader.skip_value())
							return false;
					} else if (!read_runtime_json_field(
												 pCache, item, *pField, pData, reader, diagnostics, fieldPath, policy, depth + 1, ok))
						return false;

					reader.ws();
					if (reader.consume(','))
						continue;
					if (reader.consume('}'))
						break;
					return false;
				}

				return true;
			}

			inline bool read_runtime_json_value(
					const ComponentCache* pCache, const ComponentCacheItem* pType, Entity typeEntity, uint8_t* pData,
					uint32_t valueSize, ser::ser_json& reader, ser::JsonDiagnostics& diagnostics, ser::json_str_view path,
					const ser::RuntimeJsonPolicy& policy, uint32_t depth, bool& ok) {
				if (depth >= RuntimeJsonMaxDepth) {
					ok = false;
					warn_runtime_json(
							diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path, "Runtime JSON nesting is too deep.");
					return reader.skip_value();
				}

				if (pType != nullptr && pType->typeKind == RuntimeTypeKind::Array) {
					const auto elemCount = pType->element_count();
					const auto elementType = pType->element_type();
					const auto* pElementType = find_runtime_json_type(pCache, elementType);
					uint32_t elemSize = 0;
					if (elemCount == 0 || !runtime_json_type_size(pElementType, elementType, elemSize) ||
							(uint64_t)elemSize * elemCount != valueSize) {
						ok = false;
						warn_runtime_json(
								diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path,
								"Runtime array payload uses an invalid reflected element type.");
						return reader.skip_value();
					}

					if (!reader.expect('['))
						return false;

					GAIA_FOR(elemCount) {
						if (i > 0 && !reader.expect(','))
							return false;
						const auto elemPath = make_runtime_json_element_path(path, i);
						auto* pElemData = pData + (uintptr_t)elemSize * i;
						if (!read_runtime_json_value(
										pCache, pElementType, elementType, pElemData, elemSize, reader, diagnostics, elemPath, policy,
										depth + 1, ok))
							return false;
					}
					return reader.expect(']');
				}

				if (pType != nullptr && pType->typeKind == RuntimeTypeKind::Vector) {
					const auto* adapter = pType->sequence_adapter();
					const auto elementType = pType->element_type();
					const auto* pElementType = find_runtime_json_type(pCache, elementType);
					uint32_t elemCount = 0;
					if (adapter == nullptr || adapter->resize == nullptr || adapter->element == nullptr ||
							!count_runtime_json_array_elements(reader, elemCount)) {
						ok = false;
						warn_runtime_json(
								diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path,
								"Runtime vector payload cannot be resized or traversed.");
						return reader.skip_value();
					}

					RuntimeSequenceScope sequence{typeEntity, pData, pData, valueSize};
					if (!adapter->resize(adapter->ctx, sequence, elemCount)) {
						ok = false;
						warn_runtime_json(
								diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path,
								"Runtime vector adapter rejected the requested element count.");
						return reader.skip_value();
					}

					if (!reader.expect('['))
						return false;
					GAIA_FOR(elemCount) {
						if (i > 0 && !reader.expect(','))
							return false;
						RuntimeSequenceElement element{};
						element.type = elementType;
						if (!adapter->element(adapter->ctx, sequence, i, element) || element.mutData == nullptr) {
							ok = false;
							warn_runtime_json(
									diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path,
									"Runtime vector adapter rejected an element.");
							return reader.skip_value();
						}
						if (element.type == EntityBad)
							element.type = elementType;
						const auto elemPath = make_runtime_json_element_path(path, i);
						if (!read_runtime_json_value(
										pCache, pElementType, element.type, (uint8_t*)element.mutData, element.size, reader, diagnostics,
										elemPath, policy, depth + 1, ok))
							return false;
					}
					return reader.expect(']');
				}

				if (pType != nullptr && pType->typeKind == RuntimeTypeKind::Opaque) {
					const auto* adapter = pType->opaque_adapter();
					const auto semanticType = pType->opaque_as_type();
					const auto* pSemanticType = find_runtime_json_type(pCache, semanticType);
					if (adapter == nullptr || adapter->project == nullptr || pSemanticType == nullptr) {
						ok = false;
						warn_runtime_json(
								diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path,
								"Runtime opaque payload cannot be projected.");
						return reader.skip_value();
					}
					RuntimeOpaqueScope opaque{typeEntity, pData, pData, valueSize};
					RuntimeOpaqueValue projected{};
					projected.type = semanticType;
					if (!adapter->project(adapter->ctx, opaque, projected) || projected.mutData == nullptr) {
						ok = false;
						warn_runtime_json(
								diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path,
								"Runtime opaque adapter rejected projection.");
						return reader.skip_value();
					}
					if (projected.type == EntityBad)
						projected.type = semanticType;
					if (projected.type != semanticType) {
						ok = false;
						warn_runtime_json(
								diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path,
								"Runtime opaque adapter projected an unexpected semantic type.");
						return reader.skip_value();
					}
					const bool parsed = read_runtime_json_value(
							pCache, pSemanticType, projected.type, (uint8_t*)projected.mutData, projected.size, reader, diagnostics,
							path, policy, depth + 1, ok);
					if (parsed && adapter->commit != nullptr && ok) {
						if (!adapter->commit(adapter->ctx, opaque, projected)) {
							ok = false;
							warn_runtime_json(
									diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path, "Runtime opaque adapter rejected commit.");
						}
					}
					return parsed;
				}

				if (pType != nullptr && pType->typeKind == RuntimeTypeKind::Struct)
					return read_runtime_json_struct(pCache, *pType, pData, reader, diagnostics, path, policy, depth + 1, ok);

				ser::serialization_type_id type = ser::serialization_type_id::ignore;
				if (pType != nullptr) {
					if (!runtime_type_json_type(*pType, type)) {
						ok = false;
						warn_runtime_json(
								diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path,
								"Runtime field uses an unsupported reflected type.");
						return reader.skip_value();
					}
				} else if (!runtime_primitive_serialization_type(typeEntity, type)) {
					ok = false;
					warn_runtime_json(diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path, "Runtime field type is unknown.");
					return reader.skip_value();
				}

				reader.ws();
				if (pType != nullptr && policy.symbolicEnums && pType->typeKind == RuntimeTypeKind::Enum && !reader.eof() &&
						reader.peek() == '"') {
					ser::json_str_view symbol;
					if (!reader.parse_string_view(symbol))
						return false;

					const auto* pConstant = pType->constant(util::str_view(symbol.data(), (uint32_t)symbol.size()));
					uint64_t constantBits = 0;
					if (pConstant == nullptr || !runtime_json_constant_bits(type, pConstant->value, constantBits) ||
							!runtime_json_write_integer_bits(pData, type, valueSize, constantBits)) {
						ok = false;
						warn_runtime_json(
								diagnostics, ser::JsonDiagReason::UnknownRuntimeConstant, path,
								"Runtime enum symbol is unknown or incompatible with its underlying type.");
					}
					return true;
				}

				if (pType != nullptr && policy.symbolicBitmasks && pType->typeKind == RuntimeTypeKind::Bitmask &&
						!reader.eof() && reader.peek() == '[') {
					if (!reader.expect('['))
						return false;

					uint64_t valueBits = 0;
					bool symbolsOk = true;
					reader.ws();
					if (!reader.consume(']')) {
						while (true) {
							ser::json_str_view symbol;
							if (!reader.parse_string_view(symbol))
								return false;

							if (symbolsOk) {
								const auto* pConstant = pType->constant(util::str_view(symbol.data(), (uint32_t)symbol.size()));
								uint64_t flagBits = 0;
								if (pConstant == nullptr) {
									symbolsOk = false;
									ok = false;
									warn_runtime_json(
											diagnostics, ser::JsonDiagReason::UnknownRuntimeConstant, path,
											"Runtime bitmask symbol is unknown.");
								} else if (
										!runtime_json_constant_bits(type, pConstant->value, flagBits) || flagBits == 0 ||
										(flagBits & (flagBits - 1)) != 0 || (valueBits & flagBits) != 0) {
									symbolsOk = false;
									ok = false;
									warn_runtime_json(
											diagnostics, ser::JsonDiagReason::InvalidRuntimeConstant, path,
											"Runtime bitmask symbol is not a distinct one-bit flag.");
								} else {
									valueBits |= flagBits;
								}
							}

							reader.ws();
							if (reader.consume(','))
								continue;
							if (reader.consume(']'))
								break;
							return false;
						}
					}

					if (symbolsOk && !runtime_json_write_integer_bits(pData, type, valueSize, valueBits)) {
						ok = false;
						warn_runtime_json(
								diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path,
								"Runtime bitmask symbols are incompatible with the underlying field size.");
					}
					return true;
				}

				bool fieldOk = true;
				if (!ser::detail::read_runtime_field_json(reader, pData, type, valueSize, fieldOk))
					return false;
				if (!fieldOk) {
					ok = false;
					warn_runtime_json(
							diagnostics, ser::JsonDiagReason::FieldValueAdjusted, path,
							"Field value was lossy, truncated, or unsupported for the target runtime field type.");
				}
				return true;
			}
		} // namespace detail
		//! \endcond

		//! Serializes a single component instance into key/value JSON using runtime field metadata in \a item.
		//! \param item Component metadata and optional runtime fields.
		//! \param pComponentData Pointer to one component value.
		//! \param writer Destination JSON writer.
		//! \param policy Explicit symbolic presentation policy.
		//! \return False when some field types are unsupported or out of bounds. Those fields are emitted as null.
		inline bool component_to_json(
				const ComponentCacheItem& item, const void* pComponentData, ser::ser_json& writer,
				const ser::RuntimeJsonPolicy& policy = {}) {
			GAIA_ASSERT(pComponentData != nullptr);
			if (pComponentData == nullptr)
				return false;

			return detail::write_runtime_json_value(
					item.owner_cache(), &item, item.entity, reinterpret_cast<const uint8_t*>(pComponentData), item.comp.size(),
					writer, policy, 0);
		}

		//! Convenience overload returning JSON as a string.
		//! \param item Component metadata and optional runtime fields.
		//! \param pComponentData Pointer to one component value.
		//! \param ok Receives whether the component value was serialized completely.
		//! \return The serialized JSON value.
		inline ser::json_str component_to_json(const ComponentCacheItem& item, const void* pComponentData, bool& ok) {
			ser::ser_json writer;
			ok = component_to_json(item, pComponentData, writer);
			return writer.str();
		}

		//! Deserializes a single component instance from a JSON object previously emitted by component_to_json()
		//! or from a raw payload object in the form {"$raw":[...]}.
		//! \param item Component metadata and optional runtime fields.
		//! \param pComponentData Pointer to destination component memory for exactly one component instance.
		//! \param reader JSON parser positioned at the beginning of the component JSON value.
		//! \param diagnostics Receives structured warnings/errors for lossy or unsupported payload content.
		//! \param policy Explicit symbolic import policy.
		//! \param componentPath Logical component path used in diagnostics.
		//! \return False only when JSON is malformed for the expected component payload shape.
		inline bool json_to_component(
				const ComponentCacheItem& item, void* pComponentData, ser::ser_json& reader, ser::JsonDiagnostics& diagnostics,
				const ser::RuntimeJsonPolicy& policy = {}, ser::json_str_view componentPath = {}) {
			GAIA_ASSERT(pComponentData != nullptr);
			if (pComponentData == nullptr)
				return false;

			if (reader.parse_null()) {
				detail::warn_runtime_json(
						diagnostics, ser::JsonDiagReason::NullComponentPayload, componentPath, "Component payload is null.");
				return true;
			}

			if (detail::runtime_json_is_direct_value(item)) {
				bool ok = true;
				return detail::read_runtime_json_value(
						item.owner_cache(), &item, item.entity, reinterpret_cast<uint8_t*>(pComponentData), item.comp.size(),
						reader, diagnostics, componentPath, policy, 0, ok);
			}

			bool rawFound = false;
			bool fieldFound = false;
			bool ok = true;
			ser::ser_buffer_binary rawPayload;
			auto* pBase = reinterpret_cast<uint8_t*>(pComponentData);

			if (!reader.expect('{'))
				return false;

			reader.ws();
			if (reader.consume('}')) {
				detail::warn_runtime_json(
						diagnostics, ser::JsonDiagReason::MissingRuntimeFieldsOrRawPayload, componentPath,
						"Component object is empty and contains no runtime fields or $raw payload.");
				return true;
			}

			while (true) {
				ser::json_str_view key;
				bool keyFromScratch = false;
				if (!reader.parse_string_view(key, &keyFromScratch))
					return false;
				ser::json_str keyStorage;
				if (keyFromScratch) {
					keyStorage.assign(key.data(), key.size());
					key = keyStorage;
				}
				if (!reader.expect(':'))
					return false;

				const auto fieldPath = detail::make_runtime_json_child_path(componentPath, key);
				if (key == "$raw") {
					rawFound = true;
					if (!ser::detail::parse_json_byte_array(reader, rawPayload))
						return false;
				} else if (item.field_count() != 0 && item.comp.soa() == 0) {
					const auto* pField = item.field(util::str_view(key.data(), (uint32_t)key.size()));
					if (pField == nullptr) {
						ok = false;
						detail::warn_runtime_json(
								diagnostics, ser::JsonDiagReason::UnknownField, fieldPath, "Unknown runtime field.");
						if (!reader.skip_value())
							return false;
					} else {
						fieldFound = true;
						if (!detail::read_runtime_json_field(
										item.owner_cache(), item, *pField, pBase, reader, diagnostics, fieldPath, policy, 0, ok))
							return false;
					}
				} else {
					ok = false;
					detail::warn_runtime_json(
							diagnostics, ser::JsonDiagReason::MissingRuntimeFieldsOrRawPayload, fieldPath,
							"Runtime field metadata is unavailable for keyed field payloads.");
					if (!reader.skip_value())
						return false;
				}

				reader.ws();
				if (reader.consume(','))
					continue;
				if (reader.consume('}'))
					break;
				return false;
			}

			if (rawFound) {
				if (item.comp.soa() != 0) {
					detail::warn_runtime_json(
							diagnostics, ser::JsonDiagReason::SoaRawUnsupported,
							detail::make_runtime_json_child_path(componentPath, "$raw"),
							"$raw payload is not supported for SoA components.");
					return true;
				}

				auto s = ser::make_serializer(rawPayload);
				s.seek(0);
				item.load(s, pBase, 0, 1, 1);
			}

			if (!rawFound && !fieldFound)
				detail::warn_runtime_json(
						diagnostics, ser::JsonDiagReason::MissingRuntimeFieldsOrRawPayload, componentPath,
						"Component payload contains neither recognized runtime fields nor $raw data.");

			(void)ok;
			return true;
		}

		//! Convenience overload reporting best-effort status as a boolean.
		//! \param item Component metadata and optional runtime fields.
		//! \param pComponentData Pointer to destination component memory.
		//! \param reader JSON parser positioned at the component value.
		//! \param ok Receives whether import completed without diagnostics.
		//! \return False only when the JSON shape is malformed.
		inline bool
		json_to_component(const ComponentCacheItem& item, void* pComponentData, ser::ser_json& reader, bool& ok) {
			ser::JsonDiagnostics diagnostics;
			const bool parsed = json_to_component(item, pComponentData, reader, diagnostics);
			ok = !diagnostics.has_issues();
			return parsed;
		}

		//! Serializes world state into a JSON document.
		//! Components with runtime fields are emitted as structured JSON objects. Pair payload keys retain relation and
		//! target. Components with no runtime fields fallback to raw serialized bytes. Returns false when some runtime
		//! field types are unsupported (those fields are emitted as null).
		inline bool
		World::save_json(ser::ser_json& writer, ser::JsonSaveFlags flags, const ser::RuntimeJsonPolicy& policy) const {
			auto write_component_key = [&](Entity component, const ComponentCacheItem& item) {
				if (!component.pair()) {
					const auto componentName = comp_cache().symbol_name(item);
					if (componentName.empty())
						return false;
					writer.key(componentName.data(), componentName.size());
					return true;
				}

				const auto relation = pair_rel(*this, component);
				auto relationName = symbol(relation);
				if (relationName.empty())
					relationName = name(relation);
				const auto target = pair_tgt(*this, component);
				auto targetName = symbol(target);
				if (targetName.empty())
					targetName = name(target);
				if (relationName.empty() || targetName.empty())
					return false;

				util::str pairName;
				pairName.append("(");
				pairName.append(relationName.data(), relationName.size());
				pairName.append(",");
				pairName.append(targetName.data(), targetName.size());
				pairName.append(")");
				writer.key(pairName.data(), pairName.size());
				return true;
			};

			auto write_raw_component = [&](const ComponentCacheItem& item, const uint8_t* pData, uint32_t from, uint32_t to,
																		 uint32_t cap) {
				ser::ser_buffer_binary raw;
				auto s = ser::make_serializer(raw);
				item.save(s, pData, from, to, cap);

				writer.begin_object();
				writer.key("$raw");
				writer.begin_array();
				const auto* pRaw = raw.data();
				GAIA_FOR(raw.bytes()) writer.value_int(pRaw[i]);
				writer.end_array();
				writer.end_object();
			};

			bool ok = true;
			const bool includeBinarySnapshot = (flags & ser::JsonSaveFlags::BinarySnapshot) != 0;
			const bool allowRawFallback = (flags & ser::JsonSaveFlags::RawFallback) != 0;
			ser::bin_stream binarySnapshot;
			if (includeBinarySnapshot) {
				auto s = ser::make_serializer(binarySnapshot);
				s.reset();
				save_to(s);
			}

			writer.clear();
			writer.begin_object();
			writer.key("format");
			writer.value_int(WorldSerializerJSONVersion);
			writer.key("worldVersion");
			writer.value_int(m_worldVersion);
			if (includeBinarySnapshot) {
				writer.key("binary");
				writer.begin_array();
				{
					const auto* pData = (const uint8_t*)binarySnapshot.data();
					GAIA_FOR(binarySnapshot.bytes()) writer.value_int(pData[i]);
				}
				writer.end_array();
			}
			writer.key("archetypes");
			writer.begin_array();

			for (const auto* pArchetype: m_archetypes) {
				if (pArchetype == nullptr || pArchetype->chunks().empty())
					continue;

				writer.begin_object();
				writer.key("id");
				writer.value_int((uint32_t)pArchetype->id());
				writer.key("hash");
				writer.value_int((uint64_t)pArchetype->lookup_hash().hash);

				writer.key("components");
				writer.begin_array();
				{
					for (const auto entity: pArchetype->ids_view()) {
						const auto itemName = name(entity);
						if (!itemName.empty())
							writer.value_string(itemName.data(), itemName.size());
						else
							writer.value_string("<unnamed>");
					}
				}
				writer.end_array();

				writer.key("entities");
				writer.begin_array();
				{
					for (const auto* pChunk: pArchetype->chunks()) {
						if (pChunk == nullptr || pChunk->empty())
							continue;

						const auto ents = pChunk->entity_view();
						const auto recs = pChunk->comp_rec_view();
						GAIA_FOR((uint32_t)ents.size()) {
							const auto entity = ents[i];

							writer.begin_object();
							{
								writer.key("entity");
								{
									writer.begin_object();
									writer.key("id");
									writer.value_int(entity.id());
									writer.key("gen");
									writer.value_int(entity.gen());
									writer.key("pair");
									writer.value_bool(entity.pair());
									writer.key("kind");
									writer.value_string(EntityKindString[entity.kind()]);
									const auto entityName = name(entity);
									if (!entityName.empty()) {
										writer.key("name");
										writer.value_string(entityName.data(), entityName.size());
									}
									writer.end_object();
								}

								writer.key("components");
								writer.begin_object();
								{
									GAIA_FOR_((uint32_t)recs.size(), j) {
										const auto& rec = recs[j];
										const auto& item = *rec.pItem;
										const auto component = pChunk->ids_view()[j];
										if (!write_component_key(component, item)) {
											writer.key("<unnamed>");
											if (component.pair() && !includeBinarySnapshot)
												ok = false;
										}

										// Tags have no associated payload.
										if (rec.comp.size() == 0) {
											writer.value_bool(true);
											continue;
										}

										const auto row = component.kind() == EntityKind::EK_Uni ? 0U : i;

										if ((item.field_count() != 0 || detail::runtime_json_is_direct_value(item)) &&
												rec.comp.soa() == 0) {
											const auto* pCompData = pChunk->comp_ptr(j, row);
											ok = ecs::component_to_json(item, pCompData, writer, policy) && ok;
										} else {
											if (allowRawFallback)
												write_raw_component(item, rec.pData, row, row + 1, pChunk->capacity());
											else {
												writer.value_null();
												ok = false;
											}
										}
									}
									writer.end_object();
								}
							}
							writer.end_object();
						}
					}
				}

				writer.end_array();
				writer.end_object();
			}

			writer.end_array();
			writer.end_object();
			return ok;
		}

		//! Convenience overload returning JSON as a string.
		inline ser::json_str World::save_json(bool& ok, ser::JsonSaveFlags flags) const {
			ser::ser_json writer;
			ok = save_json(writer, flags);
			return writer.str();
		}

		//! Loads world state from JSON previously emitted by save_json().
		inline bool World::load_json(
				const char* json, uint32_t len, ser::JsonDiagnostics& diagnostics, const ser::RuntimeJsonPolicy& policy) {
			diagnostics.clear();
			if (json == nullptr)
				return false;
			if (len == 0) {
				diagnostics.add(
						ser::JsonDiagSeverity::Error, ser::JsonDiagReason::InvalidJson, "$",
						"Input JSON length must be provided and non-zero.");
				return false;
			}

			const auto dataLen = len;
			const auto* p = json;
			const auto* end = json + dataLen;
			auto warn = [&](ser::JsonDiagReason reason, ser::json_str_view path, const char* message) {
				diagnostics.add(ser::JsonDiagSeverity::Warning, reason, path, message);
			};
			auto error = [&](ser::JsonDiagReason reason, ser::json_str_view path, const char* message) {
				diagnostics.add(ser::JsonDiagSeverity::Error, reason, path, message);
			};

			// Validate top-level format version first.
			{
				ser::ser_json header(json, dataLen);
				if (!header.expect('{')) {
					error(ser::JsonDiagReason::InvalidJson, "$", "Root JSON value must be an object.");
					return false;
				}

				bool hasFormat = false;
				uint32_t formatValue = 0;

				header.ws();
				if (!header.consume('}')) {
					while (true) {
						ser::json_str_view key;
						if (!header.parse_string_view(key))
							return false;
						if (!header.expect(':'))
							return false;

						if (key == "format") {
							double d = 0.0;
							if (!header.parse_number(d))
								return false;
							if (d < 0.0 || d > 4294967295.0)
								return false;
							const auto v = (uint32_t)d;
							if ((double)v != d)
								return false;
							formatValue = v;
							hasFormat = true;
						} else {
							if (!header.skip_value())
								return false;
						}

						header.ws();
						if (header.consume(','))
							continue;
						if (header.consume('}'))
							break;
						return false;
					}
				}

				header.ws();
				if (!header.eof())
					return false;

				if (!hasFormat) {
					error(ser::JsonDiagReason::MissingFormatField, "$.format", "Missing required 'format' field.");
					return false;
				}

				if (formatValue != WorldSerializerJSONVersion) {
					error(
							ser::JsonDiagReason::UnsupportedFormatVersion, "$.format",
							"Unsupported format version. Expected numeric value 1.");
					return false;
				}
			}

			// Prefer fast-path: binary snapshot payload.
			{
				const char key[] = "\"binary\"";
				const uint32_t keyLen = (uint32_t)(sizeof(key) - 1);
				const char* keyPos = nullptr;
				for (const char* it = p; it + keyLen <= end; ++it) {
					if (memcmp(it, key, keyLen) == 0) {
						keyPos = it;
						break;
					}
				}
				if (keyPos != nullptr) {
					const char* arr = nullptr;
					for (const char* it = keyPos + keyLen; it < end; ++it) {
						if (*it == '[') {
							arr = it;
							break;
						}
					}
					if (arr != nullptr) {
						ser::bin_stream serializer;
						ser::ser_json binaryReader(arr, (uint32_t)(end - arr));
						if (!ser::detail::parse_json_byte_array(binaryReader, serializer))
							return false;

						return load(serializer);
					}
				}
			}

			// Fallback: semantic world JSON parser.
			ser::ser_json jp(json, dataLen);

			struct CompDataLoc {
				uint8_t* pBase = nullptr;
				uint32_t row = 0;
			};

			auto locate_component_data = [&](Entity entity, Entity component) {
				CompDataLoc loc{};
				auto& ec = fetch(entity);
				const auto compIdx = core::get_index(ec.pChunk->ids_view(), component);
				if (compIdx == BadIndex)
					return loc;

				loc.pBase = ec.pChunk->comp_ptr_mut(compIdx, 0);
				loc.row = component.kind() == EntityKind::EK_Uni ? 0U : ec.row;
				return loc;
			};

			auto parse_and_apply_component_value = [&](Entity entity, Entity component, const ComponentCacheItem& item,
																								 ser::json_str_view compPath) -> bool {
				jp.ws();
				if (jp.eof())
					return false;

				if (jp.parse_null()) {
					warn(
							ser::JsonDiagReason::NullComponentPayload, compPath,
							"Null component payload is ignored in semantic mode.");
					return true;
				}

				if (!has_direct(entity, component)) {
					if (component.pair())
						add(entity, Pair(pair_rel(*this, component), pair_tgt(*this, component)));
					else
						add(entity, component);
				}

				const auto loc = locate_component_data(entity, component);
				if (loc.pBase == nullptr) {
					warn(
							ser::JsonDiagReason::MissingComponentStorage, compPath,
							"Component storage is unavailable on the target entity.");
					return jp.skip_value();
				}

				auto* pRowData = loc.pBase + (uintptr_t)item.comp.size() * loc.row;
				if (!ecs::json_to_component(item, pRowData, jp, diagnostics, policy, compPath))
					return false;
				return true;
			};

			auto parse_entity_meta = [&](bool& isPair, ser::json_str& nameOut) -> bool {
				if (!jp.expect('{'))
					return false;

				jp.ws();
				if (jp.consume('}'))
					return true;

				while (true) {
					ser::json_str_view key;
					if (!jp.parse_string_view(key))
						return false;
					if (!jp.expect(':'))
						return false;

					if (key == "pair") {
						if (!jp.parse_bool(isPair))
							return false;
					} else if (key == "name") {
						if (!jp.parse_string(nameOut))
							return false;
					} else {
						if (!jp.skip_value())
							return false;
					}

					jp.ws();
					if (jp.consume(','))
						continue;
					if (jp.consume('}'))
						break;
					return false;
				}

				return true;
			};

			auto parse_components_for_entity = [&](Entity& entity, bool& created, bool isPair,
																						 const ser::json_str& entityName) -> bool {
				if (!jp.expect('{'))
					return false;

				jp.ws();
				if (jp.consume('}'))
					return true;

				while (true) {
					ser::json_str_view compName;
					bool compNameFromScratch = false;
					if (!jp.parse_string_view(compName, &compNameFromScratch))
						return false;
					ser::json_str compNameStorage;
					if (compNameFromScratch) {
						compNameStorage.assign(compName.data(), compName.size());
						compName = compNameStorage;
					}
					if (!jp.expect(':'))
						return false;

					const auto componentName = util::str_view(compName.data(), compName.size());
					const bool nameIsInternal = ComponentCache::is_internal_symbol(componentName);
					const auto componentEntity = nameIsInternal ? EntityBad : name_to_entity({compName.data(), compName.size()});
					const ComponentCacheItem* pItem = nullptr;
					if (componentEntity.pair())
						pItem = comp_cache().find_pair_payload(componentEntity);
					else if (componentEntity != EntityBad)
						pItem = comp_cache().find(componentEntity);
					const auto itemName = pItem != nullptr ? comp_cache().symbol_name(*pItem) : util::str_view{};
					const bool itemIsInternal = ComponentCache::is_internal_symbol(itemName);
					const auto relationName =
							componentEntity.pair() ? symbol(pair_rel(*this, componentEntity)) : util::str_view{};
					const bool relationIsInternal = ComponentCache::is_internal_symbol(relationName);
					if (isPair || nameIsInternal || itemIsInternal || relationIsInternal) {
						if (!jp.skip_value())
							return false;
					} else {
						if (pItem == nullptr) {
							warn(
									ser::JsonDiagReason::UnknownComponent, compName,
									"Component is not registered in the component cache.");
							if (!jp.skip_value())
								return false;
						} else if (pItem->comp.size() == 0) {
							// Ignore tag-only components in semantic mode for now.
							warn(
									ser::JsonDiagReason::TagComponentUnsupported, compName,
									"Tag-only component semantic JSON loading is currently unsupported.");
							if (!jp.skip_value())
								return false;
						} else {
							if (!created) {
								entity = add();
								created = true;
								if (!entityName.empty()) {
									const auto existing = get(entityName.data(), (uint32_t)entityName.size());
									if (existing == EntityBad)
										name(entity, entityName.data(), (uint32_t)entityName.size());
									else
										warn(
												ser::JsonDiagReason::DuplicateEntityName, "entity.name",
												"Entity name already exists; keeping existing mapping.");
								}
							}

							if (!parse_and_apply_component_value(entity, componentEntity, *pItem, compName))
								return false;
						}
					}

					jp.ws();
					if (jp.consume(','))
						continue;
					if (jp.consume('}'))
						break;
					return false;
				}

				return true;
			};

			auto parse_entity_entry = [&]() -> bool {
				if (!jp.expect('{')) {
					error(ser::JsonDiagReason::InvalidJson, "$", "Root JSON value must be an object.");
					return false;
				}

				bool isPair = false;
				ser::json_str entityName;
				Entity entity = EntityBad;
				bool created = false;

				jp.ws();
				if (jp.consume('}'))
					return true;

				while (true) {
					ser::json_str_view key;
					if (!jp.parse_string_view(key))
						return false;
					if (!jp.expect(':'))
						return false;

					if (key == "entity") {
						if (!parse_entity_meta(isPair, entityName))
							return false;
					} else if (key == "components") {
						if (!parse_components_for_entity(entity, created, isPair, entityName))
							return false;
					} else {
						if (!jp.skip_value())
							return false;
					}

					jp.ws();
					if (jp.consume(','))
						continue;
					if (jp.consume('}'))
						break;
					return false;
				}

				return true;
			};

			auto parse_archetypes = [&]() -> bool {
				if (!jp.expect('['))
					return false;

				jp.ws();
				if (jp.consume(']'))
					return true;

				while (true) {
					if (!jp.expect('{'))
						return false;

					jp.ws();
					if (!jp.consume('}')) {
						while (true) {
							ser::json_str_view key;
							if (!jp.parse_string_view(key))
								return false;
							if (!jp.expect(':'))
								return false;

							if (key == "entities") {
								if (!jp.expect('['))
									return false;

								jp.ws();
								if (!jp.consume(']')) {
									while (true) {
										if (!parse_entity_entry())
											return false;

										jp.ws();
										if (jp.consume(','))
											continue;
										if (jp.consume(']'))
											break;
										return false;
									}
								}
							} else {
								if (!jp.skip_value())
									return false;
							}

							jp.ws();
							if (jp.consume(','))
								continue;
							if (jp.consume('}'))
								break;
							return false;
						}
					}

					jp.ws();
					if (jp.consume(','))
						continue;
					if (jp.consume(']'))
						break;
					return false;
				}

				return true;
			};

			if (!jp.expect('{'))
				return false;

			bool hasArchetypes = false;
			jp.ws();
			if (!jp.consume('}')) {
				while (true) {
					ser::json_str_view key;
					if (!jp.parse_string_view(key))
						return false;
					if (!jp.expect(':'))
						return false;

					if (key == "archetypes") {
						hasArchetypes = true;
						if (!parse_archetypes())
							return false;
					} else {
						if (!jp.skip_value())
							return false;
					}

					jp.ws();
					if (jp.consume(','))
						continue;
					if (jp.consume('}'))
						break;
					return false;
				}
			}

			jp.ws();
			if (!jp.eof())
				return false;
			if (!hasArchetypes) {
				error(ser::JsonDiagReason::MissingArchetypesSection, "$.archetypes", "Missing required 'archetypes' section.");
				return false;
			}

			return true;
		}

		inline bool World::load_json(const char* json, uint32_t len) {
			ser::JsonDiagnostics diagnostics;
			const bool parsed = load_json(json, len, diagnostics);
			return parsed && !diagnostics.has_issues();
		}

		inline bool
		World::load_json(ser::json_str_view json, ser::JsonDiagnostics& diagnostics, const ser::RuntimeJsonPolicy& policy) {
			return load_json(json.data(), json.size(), diagnostics, policy);
		}

		inline bool World::load_json(ser::json_str_view json) {
			ser::JsonDiagnostics diagnostics;
			const bool parsed = load_json(json.data(), json.size(), diagnostics);
			return parsed && !diagnostics.has_issues();
		}
	} // namespace ecs
} // namespace gaia

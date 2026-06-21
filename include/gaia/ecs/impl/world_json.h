#pragma once
#include "gaia/config/config.h"

#include <cstdio>
#include <cstring>

#include "gaia/ser/ser_json.h"

namespace gaia {
	namespace ecs {
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
					out.type = pFieldType->array_element_type();
					out.elemCount = pFieldType->array_element_count();
					out.pType = find_runtime_json_type(pCache, out.type);
					if (out.pType != nullptr && out.pType->typeKind == RuntimeTypeKind::Array)
						return false;
				}

				return out.elemCount != 0 && runtime_json_type_size(out.pType, out.type, out.elemSize);
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

			inline bool write_runtime_json_value(
					const ComponentCache* pCache, const ComponentCacheItem* pType, Entity typeEntity, const uint8_t* pData,
					uint32_t valueSize, ser::ser_json& writer, uint32_t depth);

			inline bool write_runtime_json_field(
					const ComponentCache* pCache, const ComponentCacheItem& owner, const RuntimeField& field,
					const uint8_t* pBase, ser::ser_json& writer, uint32_t depth) {
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
							pCache, layout.pType, layout.type, pFieldData, (uint32_t)fieldSize64, writer, depth + 1);

				bool ok = true;
				writer.begin_array();
				GAIA_FOR(layout.elemCount) {
					const auto* pElemData = pFieldData + (uintptr_t)layout.elemSize * i;
					ok = write_runtime_json_value(
									 pCache, layout.pType, layout.type, pElemData, layout.elemSize, writer, depth + 1) &&
							 ok;
				}
				writer.end_array();
				return ok;
			}

			inline bool write_runtime_json_struct(
					const ComponentCache* pCache, const ComponentCacheItem& item, const uint8_t* pData, ser::ser_json& writer,
					uint32_t depth) {
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
					ok = write_runtime_json_field(pCache, item, field, pData, writer, depth + 1) && ok;
				}
				writer.end_object();
				return ok;
			}

			inline bool write_runtime_json_value(
					const ComponentCache* pCache, const ComponentCacheItem* pType, Entity typeEntity, const uint8_t* pData,
					uint32_t valueSize, ser::ser_json& writer, uint32_t depth) {
				if (depth >= RuntimeJsonMaxDepth) {
					writer.value_null();
					return false;
				}

				if (pType != nullptr && pType->typeKind == RuntimeTypeKind::Struct)
					return write_runtime_json_struct(pCache, *pType, pData, writer, depth + 1);

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

				return ser::detail::write_runtime_field_json(writer, pData, type, valueSize);
			}

			inline bool read_runtime_json_value(
					const ComponentCache* pCache, const ComponentCacheItem* pType, Entity typeEntity, uint8_t* pData,
					uint32_t valueSize, ser::ser_json& reader, ser::JsonDiagnostics& diagnostics, ser::json_str_view path,
					uint32_t depth, bool& ok);

			inline void warn_runtime_json(
					ser::JsonDiagnostics& diagnostics, ser::JsonDiagReason reason, ser::json_str_view path, const char* message) {
				diagnostics.add(ser::JsonDiagSeverity::Warning, reason, path, message);
			}

			inline bool read_runtime_json_field(
					const ComponentCache* pCache, const ComponentCacheItem& owner, const RuntimeField& field, uint8_t* pBase,
					ser::ser_json& reader, ser::JsonDiagnostics& diagnostics, ser::json_str_view path, uint32_t depth, bool& ok) {
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
							pCache, layout.pType, layout.type, pFieldData, (uint32_t)fieldSize64, reader, diagnostics, path,
							depth + 1, ok);

				if (!reader.expect('['))
					return false;

				GAIA_FOR(layout.elemCount) {
					if (i > 0 && !reader.expect(','))
						return false;
					const auto elemPath = make_runtime_json_element_path(path, i);
					auto* pElemData = pFieldData + (uintptr_t)layout.elemSize * i;
					if (!read_runtime_json_value(
									pCache, layout.pType, layout.type, pElemData, layout.elemSize, reader, diagnostics, elemPath,
									depth + 1, ok))
						return false;
				}
				return reader.expect(']');
			}

			inline bool read_runtime_json_struct(
					const ComponentCache* pCache, const ComponentCacheItem& item, uint8_t* pData, ser::ser_json& reader,
					ser::JsonDiagnostics& diagnostics, ser::json_str_view path, uint32_t depth, bool& ok) {
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
												 pCache, item, *pField, pData, reader, diagnostics, fieldPath, depth + 1, ok))
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
					uint32_t depth, bool& ok) {
				if (depth >= RuntimeJsonMaxDepth) {
					ok = false;
					warn_runtime_json(
							diagnostics, ser::JsonDiagReason::FieldOutOfBounds, path, "Runtime JSON nesting is too deep.");
					return reader.skip_value();
				}

				if (pType != nullptr && pType->typeKind == RuntimeTypeKind::Struct)
					return read_runtime_json_struct(pCache, *pType, pData, reader, diagnostics, path, depth + 1, ok);

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

		//! Serializes a single component instance into key/value JSON using runtime field metadata in @a item.
		//! Returns false when some field types are unsupported or out of bounds (those are emitted as null).
		inline bool component_to_json(const ComponentCacheItem& item, const void* pComponentData, ser::ser_json& writer) {
			GAIA_ASSERT(pComponentData != nullptr);
			if (pComponentData == nullptr)
				return false;

			return detail::write_runtime_json_struct(
					item.owner_cache(), item, reinterpret_cast<const uint8_t*>(pComponentData), writer, 0);
		}

		//! Convenience overload returning JSON as a string.
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
		//! \param componentPath Logical component path used in diagnostics.
		//! \return false only when JSON is malformed for the expected component payload shape.
		inline bool json_to_component(
				const ComponentCacheItem& item, void* pComponentData, ser::ser_json& reader, ser::JsonDiagnostics& diagnostics,
				ser::json_str_view componentPath = {}) {
			GAIA_ASSERT(pComponentData != nullptr);
			if (pComponentData == nullptr)
				return false;

			if (reader.parse_null()) {
				detail::warn_runtime_json(
						diagnostics, ser::JsonDiagReason::NullComponentPayload, componentPath, "Component payload is null.");
				return true;
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
										item.owner_cache(), item, *pField, pBase, reader, diagnostics, fieldPath, 0, ok))
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

		//! Compatibility overload preserving the previous bool-based best-effort status.
		inline bool
		json_to_component(const ComponentCacheItem& item, void* pComponentData, ser::ser_json& reader, bool& ok) {
			ser::JsonDiagnostics diagnostics;
			const bool parsed = json_to_component(item, pComponentData, reader, diagnostics);
			ok = !diagnostics.has_issues();
			return parsed;
		}

		//! Serializes world state into a JSON document.
		//! Components with runtime fields are emitted as structured JSON objects.
		//! Components with no runtime fields fallback to raw serialized bytes.
		//! Returns false when some runtime field types are unsupported (those fields are emitted as null).
		inline bool World::save_json(ser::ser_json& writer, ser::JsonSaveFlags flags) const {
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
										const auto name = symbol(item.entity);
										writer.key(name.data(), name.size());

										// Tags have no associated payload.
										if (rec.comp.size() == 0) {
											writer.value_bool(true);
											continue;
										}

										const auto row = item.entity.kind() == EntityKind::EK_Uni ? 0U : i;

										if (item.field_count() != 0 && rec.comp.soa() == 0) {
											const auto* pCompData = pChunk->comp_ptr(j, row);
											ok = ecs::component_to_json(item, pCompData, writer) && ok;
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
		inline bool World::load_json(const char* json, uint32_t len, ser::JsonDiagnostics& diagnostics) {
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
				uint32_t cap = 0;
				bool valid = false;
			};

			auto locate_component_data = [&](Entity entity, const ComponentCacheItem& item) -> CompDataLoc {
				CompDataLoc loc{};
				if (!has(entity))
					return loc;

				auto& ec = fetch(entity);
				const auto compIdx = core::get_index(ec.pChunk->ids_view(), item.entity);
				if (compIdx == BadIndex)
					return loc;

				loc.pBase = ec.pChunk->comp_ptr_mut(compIdx, 0);
				loc.row = item.entity.kind() == EntityKind::EK_Uni ? 0U : ec.row;
				loc.cap = ec.pChunk->capacity();
				loc.valid = true;
				return loc;
			};

			auto has_direct_component = [&](Entity entity, Entity componentEntity) -> bool {
				if (!has(entity))
					return false;
				const auto& ec = fetch(entity);
				return core::get_index(ec.pChunk->ids_view(), componentEntity) != BadIndex;
			};

			auto parse_and_apply_component_value = [&](Entity entity, const ComponentCacheItem& item,
																								 ser::json_str_view compPath) -> bool {
				jp.ws();
				if (jp.eof())
					return false;

				// Tag values are currently ignored by semantic loader.
				const char next = jp.peek();
				if (next == 't' || next == 'f') {
					bool tagValue = false;
					if (!jp.parse_bool(tagValue))
						return false;
					warn(
							ser::JsonDiagReason::TagValueIgnored, compPath,
							"Tag-like boolean component payload is ignored in semantic mode.");
					return true;
				}

				if (jp.parse_null()) {
					warn(
							ser::JsonDiagReason::NullComponentPayload, compPath,
							"Null component payload is ignored in semantic mode.");
					return true;
				}

				if (!has_direct_component(entity, item.entity))
					add(entity, item.entity);

				auto loc = locate_component_data(entity, item);
				if (!loc.valid) {
					warn(
							ser::JsonDiagReason::MissingComponentStorage, compPath,
							"Component storage is unavailable on the target entity.");
					return jp.skip_value();
				}

				auto* pRowData = loc.pBase + (uintptr_t)item.comp.size() * loc.row;
				if (!ecs::json_to_component(item, pRowData, jp, diagnostics, compPath))
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

					const bool isInternalComp = compName.size() >= 10 && memcmp(compName.data(), "gaia::ecs::", 10) == 0;
					if (isPair || isInternalComp) {
						if (!jp.skip_value())
							return false;
					} else {
						const auto componentEntity = get(compName.data(), (uint32_t)compName.size());
						const auto* pItem = componentEntity != EntityBad ? comp_cache().find(componentEntity) : nullptr;
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

							if (!parse_and_apply_component_value(entity, *pItem, compName))
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

		inline bool World::load_json(ser::json_str_view json, ser::JsonDiagnostics& diagnostics) {
			return load_json(json.data(), json.size(), diagnostics);
		}

		inline bool World::load_json(ser::json_str_view json) {
			ser::JsonDiagnostics diagnostics;
			const bool parsed = load_json(json.data(), json.size(), diagnostics);
			return parsed && !diagnostics.has_issues();
		}
	} // namespace ecs
} // namespace gaia

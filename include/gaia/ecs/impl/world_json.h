#pragma once
#include "gaia/config/config.h"

#include <cstring>

#include "gaia/ser/ser_json.h"

namespace gaia {
	namespace ecs {
		//! Serializes a single component instance into JSON using runtime schema data in @a item.
		//! Field names are emitted as-is from ComponentCacheItem::schema (flat key/value JSON object).
		//! Returns false when some field types are unsupported (those are emitted as null).
		inline bool component_to_json(const ComponentCacheItem& item, const void* pComponentData, ser::ser_json& writer) {
			GAIA_ASSERT(pComponentData != nullptr);
			if (pComponentData == nullptr)
				return false;

			bool ok = true;
			const auto* pBase = reinterpret_cast<const uint8_t*>(pComponentData);

			writer.begin_object();
			for (const auto& field: item.schema) {
				writer.key(field.name);
				if (field.offset + field.size > item.comp.size()) {
					writer.value_null();
					ok = false;
					continue;
				}
				const auto* pFieldData = pBase + field.offset;
				ok = ser::detail::write_schema_field_json(writer, pFieldData, field.type, field.size) && ok;
			}
			writer.end_object();

			return ok;
		}

		//! Convenience overload returning JSON as a string.
		inline ser::json_str component_to_json(const ComponentCacheItem& item, const void* pComponentData, bool& ok) {
			ser::ser_json writer;
			ok = component_to_json(item, pComponentData, writer);
			return writer.str();
		}

		//! Deserializes a single component instance from a JSON object previously emitted by component_to_json()
		//! or from a raw payload object in the form {"$raw":[...]}.
		//! \param item Component metadata and optional runtime schema.
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

			auto make_field_path = [&](ser::json_str_view fieldName) {
				if (componentPath.empty())
					return ser::json_str(fieldName);
				if (fieldName.empty())
					return ser::json_str(componentPath);

				ser::json_str path;
				path.reserve(componentPath.size() + 1 + fieldName.size());
				path.append(componentPath.data(), componentPath.size());
				path.append('.');
				path.append(fieldName.data(), fieldName.size());
				return path;
			};
			auto warn = [&](ser::JsonDiagReason reason, const ser::json_str& path, const char* message) {
				diagnostics.add(ser::JsonDiagSeverity::Warning, reason, path, message);
			};

			if (reader.parse_null()) {
				warn(ser::JsonDiagReason::NullComponentPayload, make_field_path(""), "Component payload is null.");
				return true;
			}

			if (!reader.expect('{'))
				return false;

			bool rawFound = false;
			bool schemaFound = false;
			ser::ser_buffer_binary rawPayload;
			auto* pBase = reinterpret_cast<uint8_t*>(pComponentData);

			reader.ws();
			if (reader.consume('}')) {
				warn(
						ser::JsonDiagReason::MissingSchemaOrRawPayload, make_field_path(""),
						"Component object is empty and contains no schema fields or $raw payload.");
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

				if (key == "$raw") {
					rawFound = true;
					if (!ser::detail::parse_json_byte_array(reader, rawPayload))
						return false;
				} else if (item.has_fields() && item.comp.soa() == 0) {
					const ComponentCacheItem::SchemaField* pField = nullptr;
					for (const auto& field: item.schema) {
						const auto fieldNameLen = (size_t)GAIA_STRLEN(field.name, ComponentCacheItem::MaxNameLength);
						if (key.size() == fieldNameLen && memcmp(key.data(), field.name, key.size()) == 0) {
							pField = &field;
							break;
						}
					}

					if (pField == nullptr) {
						warn(ser::JsonDiagReason::UnknownField, make_field_path(key), "Unknown schema field.");
						if (!reader.skip_value())
							return false;
					} else if (pField->offset + pField->size > item.comp.size()) {
						warn(
								ser::JsonDiagReason::FieldOutOfBounds, make_field_path(key),
								"Schema field points outside component size.");
						if (!reader.skip_value())
							return false;
					} else {
						schemaFound = true;
						auto* pFieldData = pBase + pField->offset;
						bool fieldOk = true;
						if (!ser::detail::read_schema_field_json(reader, pFieldData, pField->type, pField->size, fieldOk))
							return false;
						if (!fieldOk) {
							warn(
									ser::JsonDiagReason::FieldValueAdjusted, make_field_path(key),
									"Field value was lossy, truncated, or unsupported for the target schema type.");
						}
					}
				} else {
					warn(
							ser::JsonDiagReason::MissingSchemaOrRawPayload, make_field_path(key),
							"Component schema is unavailable for keyed field payloads.");
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
					warn(
							ser::JsonDiagReason::SoaRawUnsupported, make_field_path("$raw"),
							"$raw payload is not supported for SoA components.");
					return true;
				}

				auto s = ser::make_serializer(rawPayload);
				s.seek(0);
				item.load(s, pBase, 0, 1, 1);
			}

			if (!rawFound && !schemaFound)
				warn(
						ser::JsonDiagReason::MissingSchemaOrRawPayload, make_field_path(""),
						"Component payload contains neither recognized schema fields nor $raw data.");

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
		//! Components with runtime schema are emitted as structured JSON objects.
		//! Components with no schema fallback to raw serialized bytes.
		//! Returns false when some schema field types are unsupported (those fields are emitted as null).
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

										if (item.has_fields() && rec.comp.soa() == 0) {
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

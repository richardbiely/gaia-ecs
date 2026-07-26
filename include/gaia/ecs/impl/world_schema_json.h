#pragma once
#include "gaia/config/config.h"

#if GAIA_JSON_ENABLED

	#include <cstdio>
	#include <cstring>

namespace gaia {
	namespace ecs {
		//! \cond INTERNAL
		namespace detail {
			//! Returns the stable manifest name for a runtime type kind.
			//! \param kind Runtime type kind.
			//! \return Static manifest name.
			inline const char* runtime_schema_kind_name(RuntimeTypeKind kind) noexcept {
				switch (kind) {
					case RuntimeTypeKind::None:
						return "none";
					case RuntimeTypeKind::Primitive:
						return "primitive";
					case RuntimeTypeKind::Struct:
						return "struct";
					case RuntimeTypeKind::Enum:
						return "enum";
					case RuntimeTypeKind::Bitmask:
						return "bitmask";
					case RuntimeTypeKind::Array:
						return "array";
					case RuntimeTypeKind::Vector:
						return "vector";
					case RuntimeTypeKind::Opaque:
						return "opaque";
				}
				return "none";
			}

			//! Returns the stable manifest name for a runtime JSON encoding.
			//! \param encoding Runtime JSON encoding.
			//! \return Static manifest name.
			inline const char* runtime_schema_json_encoding_name(RuntimeJsonEncoding encoding) noexcept {
				return encoding == RuntimeJsonEncoding::Utf8String ? "utf8-string" : "default";
			}

			//! Returns the stable manifest name for a component storage type.
			//! \param storage Component storage type.
			//! \return Static manifest name.
			inline const char* runtime_schema_storage_name(DataStorageType storage) noexcept {
				return storage == DataStorageType::Sparse ? "sparse" : "table";
			}

			//! Checks whether a registered type exposes at least one editable reflected leaf.
			//! \param cache Component metadata cache used to resolve referenced types.
			//! \param item Type metadata to inspect.
			//! \param depth Current recursive type depth.
			//! \return True when the type can be edited through the runtime JSON patch API.
			inline bool
			runtime_schema_type_editable(const ComponentCache& cache, const ComponentCacheItem& item, uint32_t depth = 0) {
				if (depth >= RuntimeJsonMaxDepth)
					return false;

				auto referenced_type_editable = [&](Entity type) {
					const auto* pType = cache.find(type);
					return pType != nullptr && runtime_schema_type_editable(cache, *pType, depth + 1);
				};

				switch (item.typeKind) {
					case RuntimeTypeKind::Primitive:
					case RuntimeTypeKind::Enum:
					case RuntimeTypeKind::Bitmask:
						return runtime_json_leaf_editable(item);
					case RuntimeTypeKind::Struct:
						GAIA_FOR(item.field_count()) {
							const auto& field = *item.field(i);
							if ((field.flags & (RuntimeFieldFlag_ReadOnly | RuntimeFieldFlag_Hidden)) == 0 &&
									referenced_type_editable(field.type))
								return true;
						}
						return false;
					case RuntimeTypeKind::Array:
						return item.elementCount > 0 && referenced_type_editable(item.elementType);
					case RuntimeTypeKind::Vector:
						return item.sequenceAdapter != nullptr && item.sequenceAdapter->count != nullptr &&
									 item.sequenceAdapter->element != nullptr && referenced_type_editable(item.elementType);
					case RuntimeTypeKind::Opaque: {
						if (item.opaqueAdapter == nullptr || item.opaqueAdapter->project == nullptr)
							return false;
						const auto* pProjected = cache.find(item.opaqueAsType);
						return pProjected != nullptr &&
									 (pProjected->typeKind == RuntimeTypeKind::Struct || pProjected->typeKind == RuntimeTypeKind::Array ||
										pProjected->typeKind == RuntimeTypeKind::Vector) &&
									 runtime_schema_type_editable(cache, *pProjected, depth + 1);
					}
					case RuntimeTypeKind::None:
						return false;
				}
				return false;
			}

			//! Writes a stable manifest reference to a registered type.
			//! \param writer JSON writer receiving the reference.
			//! \param cache Component metadata cache used to resolve \a type.
			//! \param type Reflected type entity.
			inline void runtime_schema_write_type_ref(ser::ser_json& writer, const ComponentCache& cache, Entity type) {
				const auto* pType = cache.find(type);
				if (pType == nullptr) {
					writer.value_null();
					return;
				}
				writer.begin_object();
				writer.key("symbol");
				const auto typeSymbol = pType->symbol_name();
				writer.value_string(typeSymbol.data(), typeSymbol.size());
				char stableId[17]{};
				(void)snprintf(stableId, sizeof(stableId), "%016llx", (unsigned long long)pType->hashLookup.hash);
				writer.key("stableId");
				writer.value_string(stableId);
				writer.end_object();
			}

			//! Builds the stable named path and hash for a semantic entity.
			//! \param world World containing the semantic entity hierarchy.
			//! \param semantic Named semantic entity.
			//! \param out Receives the dot-separated path.
			//! \param stableHash Receives the path-derived stable hash.
			//! \return True when the entity and every parent have valid names.
			GAIA_NODISCARD inline bool
			runtime_schema_semantic_path(const World& world, Entity semantic, util::str& out, uint64_t& stableHash) {
				out.clear();
				stableHash = 0;
				if (semantic == EntityBad || semantic.pair())
					return false;

				cnt::darray_ext<util::str_view, 16> segments;
				auto current = semantic;
				while (current != EntityBad) {
					if (!world.valid(current) || current.pair())
						return false;
					const auto name = world.name(current);
					if (name.empty())
						return false;
					segments.push_back(name);
					current = world.target(current, ChildOf);
				}
				if (segments.empty())
					return false;

				uint32_t size = (uint32_t)segments.size() - 1;
				for (const auto segment: segments)
					size += segment.size();
				out.reserve(size);
				stableHash = core::calculate_hash64("gaia.ecs.semantic.path.v1");
				for (uint32_t i = (uint32_t)segments.size(); i > 0; --i) {
					const auto segment = segments[i - 1];
					if (!out.empty())
						out.append('.');
					out.append(segment);
					stableHash = core::hash_combine(
							stableHash, (uint64_t)segment.size(), core::calculate_hash64(segment.data(), segment.size()));
				}
				return true;
			}

			//! Writes a stable manifest reference to a named semantic entity.
			//! \param writer JSON writer receiving the reference.
			//! \param world World containing the semantic entity hierarchy.
			//! \param semantic Semantic entity to reference.
			inline void runtime_schema_write_semantic_ref(ser::ser_json& writer, const World& world, Entity semantic) {
				util::str path;
				uint64_t stableHash = 0;
				if (!runtime_schema_semantic_path(world, semantic, path, stableHash)) {
					writer.value_null();
					return;
				}
				const auto name = world.name(semantic);
				writer.begin_object();
				writer.key("name");
				writer.value_string(name.data(), name.size());
				writer.key("path");
				writer.value_string(path.data(), path.size());
				char stableId[17]{};
				(void)snprintf(stableId, sizeof(stableId), "%016llx", (unsigned long long)stableHash);
				writer.key("stableId");
				writer.value_string(stableId);
				writer.end_object();
			}

			//! Writes one registered runtime field to the schema manifest.
			//! \param writer JSON writer receiving the field object.
			//! \param world World used to resolve semantic paths.
			//! \param cache Component metadata cache used to resolve field types.
			//! \param item Registered type that owns \a field.
			//! \param field Registered field metadata.
			inline void runtime_schema_write_field(
					ser::ser_json& writer, const World& world, const ComponentCache& cache, const ComponentCacheItem& item,
					const RuntimeFieldDesc& field) {
				writer.begin_object();
				writer.key("name");
				const auto fieldName = item.field_name(field);
				writer.value_string(fieldName.data(), fieldName.size());
				writer.key("type");
				runtime_schema_write_type_ref(writer, cache, field.type);
				const auto* pFieldType = cache.find(field.type);
				writer.key("editable");
				writer.value_bool(
						(field.flags & (RuntimeFieldFlag_ReadOnly | RuntimeFieldFlag_Hidden)) == 0 && pFieldType != nullptr &&
						runtime_schema_type_editable(cache, *pFieldType));
				writer.key("offset");
				writer.value_int(field.offset);
				writer.key("count");
				writer.value_int(ComponentCacheItem::field_element_count(field));
				writer.key("semantic");
				runtime_schema_write_semantic_ref(writer, world, field.semantic);
				writer.key("jsonEncoding");
				writer.value_string(runtime_schema_json_encoding_name(field.jsonEncoding));
				writer.key("readOnly");
				writer.value_bool((field.flags & RuntimeFieldFlag_ReadOnly) != 0);
				writer.key("hidden");
				writer.value_bool((field.flags & RuntimeFieldFlag_Hidden) != 0);
				writer.key("multiline");
				writer.value_bool((field.flags & RuntimeFieldFlag_Multiline) != 0);
				const auto unit = item.field_unit(field);
				if (!unit.empty()) {
					writer.key("unit");
					writer.value_string(unit.data(), unit.size());
				}
				if ((field.flags & RuntimeFieldFlag_HasMinimum) != 0) {
					writer.key("minimum");
					writer.value_float(field.minimum);
				}
				if ((field.flags & RuntimeFieldFlag_HasMaximum) != 0) {
					writer.key("maximum");
					writer.value_float(field.maximum);
				}
				if ((field.flags & RuntimeFieldFlag_HasStep) != 0) {
					writer.key("step");
					writer.value_float(field.step);
				}
				RuntimeJsonFieldLayout layout{};
				const bool derivedUtf8 = resolve_runtime_json_field_layout(&cache, field, layout) && layout.elemCount > 1 &&
																 runtime_json_is_char8_type(layout.pType, layout.type);
				if (field.jsonEncoding == RuntimeJsonEncoding::Utf8String || derivedUtf8) {
					writer.key("encoding");
					writer.value_string("utf-8");
					writer.key("capacity");
					writer.value_int(layout.elemCount);
				}
				writer.end_object();
			}

			//! Writes one registered component or reflected type to the schema manifest.
			//! \param writer JSON writer receiving the type object.
			//! \param world World used to resolve semantic paths.
			//! \param cache Component metadata cache used to resolve referenced types.
			//! \param item Registered component or type metadata.
			//! \param includeRuntimeEntity Whether to include the world-local entity id and generation.
			inline void runtime_schema_write_item(
					ser::ser_json& writer, const World& world, const ComponentCache& cache, const ComponentCacheItem& item,
					bool includeRuntimeEntity) {
				writer.begin_object();
				writer.key("symbol");
				const auto itemSymbol = item.symbol_name();
				writer.value_string(itemSymbol.data(), itemSymbol.size());
				writer.key("path");
				writer.value_string(item.path.empty() ? "" : item.path.data(), item.path.size());
				if (includeRuntimeEntity) {
					writer.key("entity");
					writer.begin_object();
					writer.key("id");
					writer.value_int(item.entity.id());
					writer.key("generation");
					writer.value_int(item.entity.gen());
					writer.end_object();
				}
				char stableId[17]{};
				(void)snprintf(stableId, sizeof(stableId), "%016llx", (unsigned long long)item.hashLookup.hash);
				writer.key("stableId");
				writer.value_string(stableId);
				writer.key("kind");
				writer.value_string(runtime_schema_kind_name(item.typeKind));
				writer.key("semantic");
				runtime_schema_write_semantic_ref(writer, world, item.semantic);
				writer.key("jsonEncoding");
				writer.value_string(runtime_schema_json_encoding_name(item.jsonEncoding));
				writer.key("size");
				writer.value_int(item.comp.size());
				writer.key("alignment");
				writer.value_int(item.comp.alig());
				writer.key("storage");
				writer.value_string(runtime_schema_storage_name(item.comp.storage_type()));
				writer.key("layout");
				writer.value_string(item.comp.soa() == 0 ? "aos" : "soa");
				writer.key("editable");
				writer.value_bool(runtime_schema_type_editable(cache, item));
				if (item.jsonEncoding == RuntimeJsonEncoding::Utf8String) {
					writer.key("encoding");
					writer.value_string("utf-8");
				}
				if (item.underlyingType != EntityBad) {
					writer.key("underlyingType");
					runtime_schema_write_type_ref(writer, cache, item.underlyingType);
				}
				if (item.elementType != EntityBad) {
					writer.key("elementType");
					runtime_schema_write_type_ref(writer, cache, item.elementType);
					writer.key("elementCount");
					writer.value_int(item.elementCount);
				}
				if (item.opaqueAsType != EntityBad) {
					writer.key("opaqueAsType");
					runtime_schema_write_type_ref(writer, cache, item.opaqueAsType);
				}
				if (item.typeKind == RuntimeTypeKind::Vector) {
					writer.key("readable");
					writer.value_bool(
							item.sequenceAdapter != nullptr && item.sequenceAdapter->count != nullptr &&
							item.sequenceAdapter->element != nullptr);
					writer.key("resizable");
					writer.value_bool(item.sequenceAdapter != nullptr && item.sequenceAdapter->resize != nullptr);
				}
				if (item.typeKind == RuntimeTypeKind::Opaque) {
					writer.key("readable");
					writer.value_bool(item.opaqueAdapter != nullptr && item.opaqueAdapter->project != nullptr);
					writer.key("committable");
					writer.value_bool(item.opaqueAdapter != nullptr && item.opaqueAdapter->commit != nullptr);
				}
				if (item.comp.soa() != 0) {
					writer.key("soaElementSizes");
					writer.begin_array();
					GAIA_FOR(item.comp.soa()) writer.value_int(item.soaSizes[i]);
					writer.end_array();
				}
				writer.key("fields");
				writer.begin_array();
				GAIA_FOR(item.field_count()) runtime_schema_write_field(writer, world, cache, item, *item.field(i));
				writer.end_array();
				if (item.constant_count() != 0) {
					writer.key("constants");
					writer.begin_array();
					GAIA_FOR(item.constant_count()) {
						const auto& constant = *item.constant(i);
						writer.begin_object();
						writer.key("name");
						const auto constantName = item.constant_name(constant);
						writer.value_string(constantName.data(), constantName.size());
						writer.key("value");
						writer.value_int(constant.value);
						writer.end_object();
					}
					writer.end_array();
				}
				writer.end_object();
			}

		} // namespace detail
		//! \endcond

		inline bool
		World::write_runtime_schema_json(ser::ser_json& writer, const char* schemaHash, bool includeRuntimeEntities) const {
			cnt::darray<const ComponentCacheItem*> items;
			items.reserve(m_compCache.m_compByEntityId.size());
			for (const auto& [entityId, pItem]: m_compCache.m_compByEntityId) {
				(void)entityId;
				items.push_back(pItem);
			}
			core::sort(items.begin(), items.end(), [](const ComponentCacheItem* a, const ComponentCacheItem* b) {
				const auto nameOrder = strcmp(a->symbol_name().data(), b->symbol_name().data());
				if (nameOrder != 0)
					return nameOrder < 0;
				const auto pathSize = a->path.size() < b->path.size() ? a->path.size() : b->path.size();
				const auto pathOrder = pathSize != 0 ? memcmp(a->path.data(), b->path.data(), pathSize) : 0;
				return pathOrder != 0 ? pathOrder < 0 : a->path.size() < b->path.size();
			});
			writer.clear();
			writer.begin_object();
			writer.key("format");
			writer.value_string("gaia.ecs.schema");
			writer.key("version");
			writer.value_int(2);
			if (schemaHash != nullptr) {
				writer.key("hash");
				writer.value_string(schemaHash);
			}
			writer.key("types");
			writer.begin_array();
			for (const auto* pItem: items)
				detail::runtime_schema_write_item(writer, *this, m_compCache, *pItem, includeRuntimeEntities);
			writer.end_array();
			writer.end_object();
			return true;
		}

		inline uint64_t World::runtime_schema_hash() const {
			ser::ser_json writer;
			(void)write_runtime_schema_json(writer, nullptr, false);
			const auto& canonical = writer.str();
			return core::calculate_hash64(canonical.data(), canonical.size());
		}

		inline bool World::save_runtime_schema_json(ser::ser_json& writer) const {
			char hash[17]{};
			(void)snprintf(hash, sizeof(hash), "%016llx", (unsigned long long)runtime_schema_hash());
			return write_runtime_schema_json(writer, hash, true);
		}

		inline ser::json_str World::save_runtime_schema_json() const {
			ser::ser_json writer;
			(void)save_runtime_schema_json(writer);
			return writer.str();
		}

	} // namespace ecs
} // namespace gaia

#endif

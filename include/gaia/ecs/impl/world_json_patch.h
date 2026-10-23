#pragma once
#include "gaia/config/config.h"

#if GAIA_JSON_ENABLED

	#include <cstdint>
	#include <cstring>

namespace gaia {
	namespace ecs {
		//! \cond INTERNAL
		namespace detail {
			//! Decodes one RFC 6901 JSON Pointer token.
			//! \param encoded Encoded token text without the leading slash.
			//! \param token Receives the decoded token.
			//! \return True when every escape sequence is valid.
			inline bool runtime_patch_decode_token(ser::json_str_view encoded, util::str& token) {
				token.clear();

				GAIA_FOR(encoded.size()) {
					const auto ch = encoded.data()[i];
					if (ch != '~') {
						token.append(ch);
						continue;
					}

					if (i + 1 >= encoded.size())
						return false;

					const auto escaped = encoded.data()[++i];
					if (escaped == '0')
						token.append('~');
					else if (escaped == '1')
						token.append('/');
					else
						return false;
				}

				return true;
			}

			//! Parses a sequence index from one JSON Pointer token.
			//! \param token Decimal index text.
			//! \param index Receives the parsed index.
			//! \return True when the token is a non-empty unsigned 32-bit integer.
			inline bool runtime_patch_parse_index(util::str_view token, uint32_t& index) {
				if (token.empty())
					return false;

				uint64_t value = 0;
				GAIA_FOR(token.size()) {
					const auto ch = token.data()[i];
					if (ch < '0' || ch > '9')
						return false;
					value = value * 10 + (uint32_t)(ch - '0');
					if (value > UINT32_MAX)
						return false;
				}

				index = (uint32_t)value;
				return true;
			}

			//! The value a JSON Pointer selects inside a component value.
			struct RuntimeJsonPointerTarget final {
				//! Registered type of the selected value.
				const ComponentCacheItem* pType = nullptr;
				//! The last field the pointer names, or nullptr when it names none.
				const RuntimeFieldDesc* pField = nullptr;
				//! True when the last token names \a pField itself rather than an element inside it.
				bool endsAtField = false;
				//! Why the pointer selects nothing.
				ser::JsonDiagReason reason = ser::JsonDiagReason::None;
				//! Human-readable explanation of \a reason.
				const char* message = nullptr;
			};

			//! Moves a cursor along an RFC 6901 JSON Pointer through reflected fields and sequence elements.
			//! \param cache Component metadata cache.
			//! \param root Registered type of the value at the cursor.
			//! \param cursor Cursor at the root value. Left at the selected value.
			//! \param pointer JSON Pointer relative to the root value. Empty selects the root.
			//! \param forWrite True to refuse read-only and hidden fields on the way.
			//! \param target Receives the selected value, or the reason the pointer selects nothing.
			//! \return True when the pointer selects a value.
			inline bool walk_runtime_json_pointer(
					const ComponentCache& cache, const ComponentCacheItem& root, ComponentCursor& cursor,
					ser::json_str_view pointer, bool forWrite, RuntimeJsonPointerTarget& target) {
				auto fail = [&](ser::JsonDiagReason reason, const char* message) {
					target.reason = reason;
					target.message = message;
					return false;
				};

				if (!pointer.empty() && pointer.data()[0] != '/')
					return fail(ser::JsonDiagReason::InvalidPatchPath, "Component path must be an RFC 6901 JSON Pointer.");
				if (!pointer.empty() && (pointer.size() == 1 || pointer.data()[pointer.size() - 1] == '/'))
					return fail(ser::JsonDiagReason::InvalidPatchPath, "Component path contains an empty token.");

				target.pType = &root;
				uint32_t pos = pointer.empty() ? pointer.size() : 1;
				while (pos < pointer.size()) {
					uint32_t end = pos;
					while (end < pointer.size() && pointer.data()[end] != '/')
						++end;

					util::str token;
					if (!runtime_patch_decode_token(ser::json_str_view(pointer.data() + pos, end - pos), token) || token.empty())
						return fail(ser::JsonDiagReason::InvalidPatchPath, "Component path contains an invalid token.");

					const ComponentCacheItem* pFieldOwner = target.pType;
					if (pFieldOwner != nullptr && pFieldOwner->typeKind == RuntimeTypeKind::Opaque)
						pFieldOwner = cache.find(pFieldOwner->opaque_as_type());

					if (pFieldOwner != nullptr && pFieldOwner->typeKind == RuntimeTypeKind::Struct) {
						const auto* pField = pFieldOwner->field(util::str_view(token.data(), token.size()));
						if (pField == nullptr)
							return fail(ser::JsonDiagReason::InvalidPatchPath, "Component path field does not exist.");
						if (forWrite && (pField->flags & RuntimeFieldFlag_ReadOnly) != 0)
							return fail(ser::JsonDiagReason::ReadOnlyField, "Component path field is read-only.");
						if (forWrite && (pField->flags & RuntimeFieldFlag_Hidden) != 0)
							return fail(ser::JsonDiagReason::HiddenField, "Component path field is hidden.");
						if (!cursor.field(util::str_view(token.data(), token.size())))
							return fail(ser::JsonDiagReason::InvalidPatchPath, "Component path field cannot be traversed.");

						target.pField = pField;
						target.endsAtField = true;
						target.pType = cache.find(pField->type);
					} else {
						uint32_t index = 0;
						if (!runtime_patch_parse_index(util::str_view(token.data(), token.size()), index) || !cursor.elem(index))
							return fail(ser::JsonDiagReason::InvalidPatchPath, "Component path sequence index is invalid.");

						target.endsAtField = false;
						target.pType = cache.find(cursor.type());
					}

					pos = end + 1;
				}

				return true;
			}

		} // namespace detail
		//! \endcond

		//! Serializes the value an RFC 6901 JSON Pointer selects inside one component value: a field, a nested
		//! field, or one element of an array or vector. The value is written as component_to_json() writes it
		//! within the whole component. Read-only and hidden fields can be read.
		//! \param item Component metadata and runtime fields.
		//! \param pComponentData Pointer to one component value.
		//! \param pointer JSON Pointer relative to the component, for example `/nested/value` or `/values/1`.
		//!                An empty pointer selects the whole component.
		//! \param writer JSON writer receiving the selected value. Untouched when the pointer selects nothing.
		//! \param diagnostics Receives the reason when the pointer selects nothing or the value cannot be written.
		//! \param policy Runtime enum and bitmask presentation policy.
		//! \return True when the selected value was written completely.
		inline bool component_to_json_at(
				const ComponentCacheItem& item, const void* pComponentData, ser::json_str_view pointer, ser::ser_json& writer,
				ser::JsonDiagnostics& diagnostics, const ser::RuntimeJsonPolicy& policy = {}) {
			auto error = [&](ser::JsonDiagReason reason, const char* message) {
				diagnostics.add(ser::JsonDiagSeverity::Error, reason, pointer, message);
				return false;
			};

			const auto* pCache = item.owner_cache();
			if (pComponentData == nullptr || pCache == nullptr || item.comp.size() == 0)
				return error(ser::JsonDiagReason::MissingComponentStorage, "Component value is unavailable.");

			auto cursor = ComponentCursor::from_raw(
					*pCache, item.entity, ComponentRawView{pComponentData, item.comp.size(), ComponentRawViewFlag_Valid});
			detail::RuntimeJsonPointerTarget target;
			if (!detail::walk_runtime_json_pointer(*pCache, item, cursor, pointer, false, target))
				return error(target.reason, target.message);

			// The cursor resolves sequence elements and opaque values as a patch sees them.
			const auto* pData = (const uint8_t*)cursor.ptr();
			const auto size = cursor.size();
			if (pData == nullptr || size == 0)
				return error(ser::JsonDiagReason::UnsupportedPatchValue, "Component path endpoint cannot be read.");

			bool ok = false;
			if (target.endsAtField) {
				// A field keeps its own layout: element count and string encoding.
				detail::RuntimeJsonFieldLayout layout{};
				if (!detail::resolve_runtime_json_field_layout(pCache, *target.pField, layout) ||
						(uint64_t)layout.elemSize * layout.elemCount != size)
					return error(ser::JsonDiagReason::UnsupportedPatchValue, "Component path field layout is invalid.");
				ok = detail::write_runtime_json_field_data(pCache, *target.pField, layout, pData, writer, policy, 0);
			} else {
				ok = detail::write_runtime_json_value(pCache, target.pType, cursor.type(), pData, size, writer, policy, 0);
			}
			if (!ok)
				return error(ser::JsonDiagReason::UnsupportedPatchValue, "Component path endpoint cannot be written as JSON.");
			return true;
		}

		inline bool World::patch_comp_json(
				Entity entity, Entity component, ser::json_str_view pointer, ser::json_str_view value,
				ser::JsonDiagnostics& diagnostics, const ser::RuntimeJsonPolicy& policy, uint64_t expectedRuntimeSchemaHash) {
			auto error = [&](ser::JsonDiagReason reason, const char* message) {
				diagnostics.add(ser::JsonDiagSeverity::Error, reason, pointer, message);
				return false;
			};

			if (expectedRuntimeSchemaHash != 0 && expectedRuntimeSchemaHash != runtime_schema_hash())
				return error(ser::JsonDiagReason::StaleSchema, "Runtime schema hash does not match the current manifest.");

			const auto* pRoot = component.pair() ? m_compCache.find_pair_payload(component) : m_compCache.find(component);
			if (pRoot == nullptr)
				return error(ser::JsonDiagReason::UnknownComponent, "Component patch target is not registered.");

			auto cursor = cursor_mut(entity, component);
			if (!cursor.valid() || cursor.size() == 0)
				return error(ser::JsonDiagReason::MissingComponentStorage, "Component patch payload is unavailable.");

			detail::RuntimeJsonPointerTarget target;
			if (!detail::walk_runtime_json_pointer(m_compCache, *pRoot, cursor, pointer, true, target))
				return error(target.reason, target.message);

			const ComponentCacheItem* pCurrent = target.pType;
			const RuntimeFieldDesc* pSelectedField = target.pField;

			const auto count = cursor.count();
			const bool charBuffer = pSelectedField != nullptr && cursor.type() == Char8 && count.ok() && count.value > 1;

			if (pCurrent == nullptr || !detail::runtime_json_leaf_editable(*pCurrent) ||
					(count.ok() && count.value > 1 && !charBuffer))
				return error(
						ser::JsonDiagReason::UnsupportedPatchValue, "Component patch endpoint must be a supported reflected leaf.");

			// Work on a copy so a failed patch does not change the component.
			cnt::darray<uint8_t> original;
			original.resize(cursor.size());
			if (!cursor.get_raw(original.data(), (uint32_t)original.size()))
				return error(ser::JsonDiagReason::UnsupportedPatchValue, "Component patch endpoint cannot be read.");

			cnt::darray<uint8_t> bytes;
			bytes.resize(original.size());
			memcpy(bytes.data(), original.data(), original.size());

			ser::ser_json reader(value.data(), value.size());
			bool valueOk = true;
			detail::RuntimeJsonReadContext ctx{&m_compCache, reader, diagnostics, policy, valueOk};

			if (cursor.type() == Char8 && cursor.size() == sizeof(char)) {
				// One Char8 value is written as a one-character JSON string.
				ser::json_str_view text;
				if (!reader.parse_string_view(text) || text.size() != 1)
					return error(ser::JsonDiagReason::InvalidJson, "Component patch character value must contain one character.");

				bytes[0] = (uint8_t)text.data()[0];
			} else if (!detail::read_runtime_json_value(
										 ctx, pCurrent, cursor.type(), bytes.data(), (uint32_t)bytes.size(), pointer, 0)) {
				return error(
						ser::JsonDiagReason::InvalidJson, "Component patch value is not valid JSON for the selected field.");
			}

			reader.ws();
			if (!reader.eof())
				return error(ser::JsonDiagReason::InvalidJson, "Component patch value contains trailing JSON.");

			if (!valueOk)
				return error(
						ser::JsonDiagReason::UnsupportedPatchValue,
						"Component patch value is incompatible with the selected field.");

			if (pSelectedField != nullptr &&
					(pSelectedField->flags & (RuntimeFieldFlag_HasMinimum | RuntimeFieldFlag_HasMaximum)) != 0) {
				double number = 0.0;
				if (!detail::runtime_patch_numeric_value(pCurrent, cursor.type(), bytes.data(), cursor.size(), number))
					return error(
							ser::JsonDiagReason::UnsupportedPatchValue, "Component patch range applies to a non-numeric field.");

				if (((pSelectedField->flags & RuntimeFieldFlag_HasMinimum) != 0 && number < pSelectedField->minimum) ||
						((pSelectedField->flags & RuntimeFieldFlag_HasMaximum) != 0 && number > pSelectedField->maximum))
					return error(
							ser::JsonDiagReason::RangeViolation, "Component patch value is outside the authored field range.");
			}

			if (!cursor.set_raw(bytes.data(), (uint32_t)bytes.size())) {
				// The write may be incomplete, so put the original value back.
				(void)cursor.set_raw(original.data(), (uint32_t)original.size(), false);
				return error(
						ser::JsonDiagReason::UnsupportedPatchValue, "Component patch could not commit the selected field.");
			}

			return true;
		}

	} // namespace ecs
} // namespace gaia

#endif

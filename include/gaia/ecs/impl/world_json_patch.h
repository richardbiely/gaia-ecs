#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <cstring>

namespace gaia {
	namespace ecs {
		//! \cond INTERNAL
		namespace detail {
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

			template <typename T>
			inline bool
			runtime_patch_numeric_value_as(Entity type, Entity expectedType, const void* data, uint32_t size, double& value) {
				if (type != expectedType || data == nullptr || size != sizeof(T))
					return false;
				T result{};
				memcpy(&result, data, sizeof(result));
				value = (double)result;
				return true;
			}

			inline bool runtime_patch_numeric_value(
					const ComponentCacheItem* pType, Entity type, const void* data, uint32_t size, double& value) {
				if (pType != nullptr &&
						(pType->typeKind == RuntimeTypeKind::Enum || pType->typeKind == RuntimeTypeKind::Bitmask))
					type = pType->underlyingType;
				return runtime_patch_numeric_value_as<int8_t>(type, S8, data, size, value) ||
							 runtime_patch_numeric_value_as<uint8_t>(type, U8, data, size, value) ||
							 runtime_patch_numeric_value_as<int16_t>(type, S16, data, size, value) ||
							 runtime_patch_numeric_value_as<uint16_t>(type, U16, data, size, value) ||
							 runtime_patch_numeric_value_as<int32_t>(type, S32, data, size, value) ||
							 runtime_patch_numeric_value_as<uint32_t>(type, U32, data, size, value) ||
							 runtime_patch_numeric_value_as<int64_t>(type, S64, data, size, value) ||
							 runtime_patch_numeric_value_as<uint64_t>(type, U64, data, size, value) ||
							 runtime_patch_numeric_value_as<float>(type, F32, data, size, value) ||
							 runtime_patch_numeric_value_as<double>(type, F64, data, size, value);
			}
		} // namespace detail
		//! \endcond

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
			if (!pointer.empty() && pointer.data()[0] != '/')
				return error(ser::JsonDiagReason::InvalidPatchPath, "Component patch path must be an RFC 6901 JSON Pointer.");
			if (!pointer.empty() && (pointer.size() == 1 || pointer.data()[pointer.size() - 1] == '/'))
				return error(ser::JsonDiagReason::InvalidPatchPath, "Component patch path contains an empty token.");

			const ComponentCacheItem* pCurrent = pRoot;
			const RuntimeFieldDesc* pSelectedField = nullptr;
			uint32_t pos = pointer.empty() ? pointer.size() : 1;
			while (pos < pointer.size()) {
				uint32_t end = pos;
				while (end < pointer.size() && pointer.data()[end] != '/')
					++end;
				util::str token;
				if (!detail::runtime_patch_decode_token(ser::json_str_view(pointer.data() + pos, end - pos), token) ||
						token.empty())
					return error(ser::JsonDiagReason::InvalidPatchPath, "Component patch path contains an invalid token.");

				const ComponentCacheItem* pFieldOwner = pCurrent;
				if (pFieldOwner != nullptr && pFieldOwner->typeKind == RuntimeTypeKind::Opaque)
					pFieldOwner = m_compCache.find(pFieldOwner->opaque_as_type());
				if (pFieldOwner != nullptr && pFieldOwner->typeKind == RuntimeTypeKind::Struct) {
					const auto* pField = pFieldOwner->field(util::str_view(token.data(), token.size()));
					if (pField == nullptr)
						return error(ser::JsonDiagReason::InvalidPatchPath, "Component patch field does not exist.");
					if ((pField->flags & RuntimeFieldFlag_ReadOnly) != 0)
						return error(ser::JsonDiagReason::ReadOnlyField, "Component patch field is read-only.");
					if ((pField->flags & RuntimeFieldFlag_Hidden) != 0)
						return error(ser::JsonDiagReason::HiddenField, "Component patch field is hidden.");
					if (!cursor.field(util::str_view(token.data(), token.size())))
						return error(ser::JsonDiagReason::InvalidPatchPath, "Component patch field cannot be traversed.");
					pSelectedField = pField;
					pCurrent = m_compCache.find(pField->type);
				} else {
					uint32_t index = 0;
					if (!detail::runtime_patch_parse_index(util::str_view(token.data(), token.size()), index) ||
							!cursor.elem(index))
						return error(ser::JsonDiagReason::InvalidPatchPath, "Component patch sequence index is invalid.");
					pCurrent = m_compCache.find(cursor.type());
				}
				pos = end + 1;
			}

			const auto count = cursor.count();
			const bool charBuffer = pSelectedField != nullptr && cursor.type() == Char8 && count.ok() && count.value > 1;
			if (pCurrent == nullptr || !detail::runtime_json_leaf_editable(*pCurrent) ||
					(count.ok() && count.value > 1 && !charBuffer))
				return error(
						ser::JsonDiagReason::UnsupportedPatchValue, "Component patch endpoint must be a supported reflected leaf.");

			cnt::darray<uint8_t> original;
			original.resize(cursor.size());
			if (!cursor.get_raw(original.data(), (uint32_t)original.size()))
				return error(ser::JsonDiagReason::UnsupportedPatchValue, "Component patch endpoint cannot be read.");
			cnt::darray<uint8_t> bytes;
			bytes.resize(original.size());
			memcpy(bytes.data(), original.data(), original.size());
			ser::ser_json reader(value.data(), value.size());
			bool valueOk = true;
			if (cursor.type() == Char8 && cursor.size() == sizeof(char)) {
				ser::json_str_view text;
				if (!reader.parse_string_view(text) || text.size() != 1)
					return error(ser::JsonDiagReason::InvalidJson, "Component patch character value must contain one character.");
				bytes[0] = (uint8_t)text.data()[0];
			} else if (!detail::read_runtime_json_value(
										 &m_compCache, pCurrent, cursor.type(), bytes.data(), (uint32_t)bytes.size(), reader, diagnostics,
										 pointer, policy, 0, valueOk)) {
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
				(void)cursor.set_raw(original.data(), (uint32_t)original.size(), false);
				return error(
						ser::JsonDiagReason::UnsupportedPatchValue, "Component patch could not commit the selected field.");
			}
			return true;
		}

	} // namespace ecs
} // namespace gaia

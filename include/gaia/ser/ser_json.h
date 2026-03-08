#pragma once
#include "gaia/config/config.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>

#include "gaia/cnt/darray.h"
#include "gaia/core/utility.h"
#include "gaia/ser/ser_buffer_binary.h"
#include "gaia/util/str.h"

namespace gaia {
	namespace ser {
		using json_str_view = util::str_view;
		using json_str = util::str;

		enum JsonSaveFlags : uint32_t {
			None = 0,
			BinarySnapshot = 1u << 0, // Include binary snapshot
			RawFallback = 1u << 1, // Allow raw data fallback
			Default = BinarySnapshot | RawFallback
		};

		enum class JsonDiagSeverity : uint8_t { Info, Warning, Error };
		enum class JsonDiagReason : uint8_t {
			None,
			UnknownField,
			FieldOutOfBounds,
			FieldValueAdjusted,
			TagValueIgnored,
			NullComponentPayload,
			MissingSchemaOrRawPayload,
			SoaRawUnsupported,
			UnknownComponent,
			TagComponentUnsupported,
			DuplicateEntityName,
			MissingComponentStorage,
			MissingArchetypesSection,
			MissingFormatField,
			UnsupportedFormatVersion,
			InvalidJson
		};

		struct JsonDiagnostic {
			JsonDiagSeverity severity = JsonDiagSeverity::Warning;
			JsonDiagReason reason = JsonDiagReason::None;
			json_str path;
			json_str message;
		};

		struct JsonDiagnostics {
			static constexpr uint32_t MaxDiagPathLength = 1024;
			static constexpr uint32_t MaxDiagMessageLength = 2048;

			cnt::darray<JsonDiagnostic> items;
			bool hasWarnings = false;
			bool hasErrors = false;

			void add(JsonDiagSeverity severity, JsonDiagReason reason, json_str_view path, json_str_view message) {
				JsonDiagnostic diag;
				diag.severity = severity;
				diag.reason = reason;
				diag.path.assign(path);
				diag.message.assign(message);
				items.push_back(diag);

				if (severity == JsonDiagSeverity::Warning)
					hasWarnings = true;
				else if (severity == JsonDiagSeverity::Error)
					hasErrors = true;
			}
			void add(JsonDiagSeverity severity, JsonDiagReason reason, json_str_view path, const char* message) {
				add(severity, reason, path, json_str_view(message, (uint32_t)GAIA_STRLEN(message, MaxDiagMessageLength)));
			}
			void add(JsonDiagSeverity severity, JsonDiagReason reason, const char* path, const char* message) {
				add(severity, reason, json_str_view(path, (uint32_t)GAIA_STRLEN(path, MaxDiagPathLength)),
						json_str_view(message, (uint32_t)GAIA_STRLEN(message, MaxDiagMessageLength)));
			}

			GAIA_NODISCARD bool has_issues() const {
				return hasWarnings || hasErrors;
			}

			void clear() {
				items.clear();
				hasWarnings = false;
				hasErrors = false;
			}
		};

		//! Lightweight JSON serializer/deserializer.
		//! - write mode: emits JSON text into an internal string
		//! - read mode: parses JSON text from a provided input buffer
		//! It intentionally stays low-level and allocation-light (no DOM tree).
		class ser_json {
			static constexpr uint32_t MaxImplicitKeyLength = 384;
			static constexpr uint32_t MaxImplicitStringLength = 16u * 1024u * 1024u;
			static constexpr uint32_t MaxLiteralLength = 256u;

			enum class CtxType : uint8_t { Object, Array };

			struct Ctx {
				CtxType type = CtxType::Object;
				bool first = true;
				bool needsValue = false;
			};

			json_str m_out;
			json_str m_parseScratch;
			cnt::darray<Ctx> m_ctx;
			const char* m_it = nullptr;
			const char* m_end = nullptr;

			static void add_escaped(json_str& out, const char* str, uint32_t len) {
				GAIA_FOR(len) {
					const char ch = str[i];
					switch (ch) {
						case '"':
							out.append("\\\"");
							break;
						case '\\':
							out.append("\\\\");
							break;
						case '\n':
							out.append("\\n");
							break;
						case '\r':
							out.append("\\r");
							break;
						case '\t':
							out.append("\\t");
							break;
						default:
							out.append(ch);
							break;
					}
				}
			}

			void before_value() {
				if (m_ctx.empty())
					return;

				auto& ctx = m_ctx.back();
				if (ctx.type == CtxType::Array) {
					if (!ctx.first)
						m_out.append(",");
					ctx.first = false;
				} else {
					GAIA_ASSERT(ctx.needsValue);
					ctx.needsValue = false;
				}
			}

		public:
			ser_json() = default;
			ser_json(const char* json, uint32_t len) {
				reset_input(json, len);
			}
			template <size_t N>
			explicit ser_json(const char (&json)[N]) {
				static_assert(N > 0);
				reset_input(json, (uint32_t)(N - 1));
			}

			//! Sets an input JSON buffer for parsing.
			void reset_input(const char* json, uint32_t len) {
				if (json == nullptr) {
					m_it = nullptr;
					m_end = nullptr;
					return;
				}

				m_it = json;
				m_end = json + len;
			}
			template <size_t N>
			void reset_input(const char (&json)[N]) {
				static_assert(N > 0);
				reset_input(json, (uint32_t)(N - 1));
			}

			//! Clears output JSON text and writer context.
			void clear() {
				m_out.clear();
				m_parseScratch.clear();
				m_ctx.clear();
			}

			//! Returns currently emitted output text.
			GAIA_NODISCARD const json_str& str() const {
				return m_out;
			}

			//! Returns true if parser has reached end of input.
			GAIA_NODISCARD bool eof() const {
				return m_it == nullptr || m_end == nullptr || m_it >= m_end;
			}

			//! Returns next non-consumed character.
			GAIA_NODISCARD char peek() const {
				GAIA_ASSERT(m_it != nullptr && m_it < m_end);
				return *m_it;
			}

			//! Skips whitespace in parser input.
			void ws() {
				if (m_it == nullptr || m_end == nullptr)
					return;
				while (m_it < m_end && std::isspace((unsigned char)*m_it))
					++m_it;
			}

			GAIA_NODISCARD const char* pos() const {
				return m_it;
			}

			GAIA_NODISCARD const char* end() const {
				return m_end;
			}

			void begin_object() {
				before_value();
				m_out.append("{");
				m_ctx.push_back({CtxType::Object, true, false});
			}

			void end_object() {
				GAIA_ASSERT(!m_ctx.empty() && m_ctx.back().type == CtxType::Object);
				m_ctx.pop_back();
				m_out.append("}");
			}

			void begin_array() {
				before_value();
				m_out.append("[");
				m_ctx.push_back({CtxType::Array, true, false});
			}

			void end_array() {
				GAIA_ASSERT(!m_ctx.empty() && m_ctx.back().type == CtxType::Array);
				m_ctx.pop_back();
				m_out.append("]");
			}

			void key(const char* name, uint32_t len = 0) {
				GAIA_ASSERT(name != nullptr);
				GAIA_ASSERT(!m_ctx.empty() && m_ctx.back().type == CtxType::Object);
				auto& ctx = m_ctx.back();
				GAIA_ASSERT(!ctx.needsValue);

				if (!ctx.first)
					m_out.append(",");
				ctx.first = false;
				ctx.needsValue = true;

				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(name, MaxImplicitKeyLength) : len;
				m_out.append("\"");
				add_escaped(m_out, name, l);
				m_out.append("\":");
			}

			void value_null() {
				before_value();
				m_out.append("null");
			}

			void value_bool(bool v) {
				before_value();
				if (v)
					m_out.append("true");
				else
					m_out.append("false");
			}

			template <typename TInt, typename = std::enable_if_t<std::is_integral_v<TInt> && !std::is_same_v<TInt, bool>>>
			void value_int(TInt v) {
				before_value();

				char buff[64];
				if constexpr (std::is_signed_v<TInt>) {
					(void)GAIA_STRFMT(buff, sizeof(buff), "%lld", (long long)v);
				} else {
					(void)GAIA_STRFMT(buff, sizeof(buff), "%llu", (unsigned long long)v);
				}
				m_out.append(buff, (uint32_t)GAIA_STRLEN(buff, (size_t)sizeof(buff)));
			}

			void value_float(float v) {
				before_value();
				char buff[64];
				(void)GAIA_STRFMT(buff, sizeof(buff), "%.9g", (double)v);
				m_out.append(buff, (uint32_t)GAIA_STRLEN(buff, (size_t)sizeof(buff)));
			}

			void value_float(double v) {
				before_value();
				char buff[64];
				(void)GAIA_STRFMT(buff, sizeof(buff), "%.17g", v);
				m_out.append(buff, (uint32_t)GAIA_STRLEN(buff, (size_t)sizeof(buff)));
			}

			void value_string(const char* str, uint32_t len = 0) {
				GAIA_ASSERT(str != nullptr);
				before_value();
				const auto l = len == 0 ? (uint32_t)GAIA_STRLEN(str, MaxImplicitStringLength) : len;
				m_out.append("\"");
				add_escaped(m_out, str, l);
				m_out.append("\"");
			}

			bool consume(char ch) {
				ws();
				if (m_it == nullptr || m_end == nullptr || m_it >= m_end || *m_it != ch)
					return false;
				++m_it;
				return true;
			}

			bool expect(char ch) {
				return consume(ch);
			}

			bool parse_literal(const char* lit) {
				ws();
				if (m_it == nullptr || m_end == nullptr || lit == nullptr)
					return false;

				const auto litLen = (uint32_t)GAIA_STRLEN(lit, MaxLiteralLength);
				if (litLen >= MaxLiteralLength)
					return false;
				if ((uint32_t)(m_end - m_it) < litLen)
					return false;
				if (memcmp(m_it, lit, litLen) != 0)
					return false;
				m_it += litLen;
				return true;
			}

			bool parse_string_view(json_str_view& out, bool* fromScratch = nullptr) {
				ws();
				if (m_it == nullptr || m_end == nullptr || m_it >= m_end || *m_it != '"')
					return false;

				++m_it;
				const char* begin = m_it;
				bool escaped = false;
				m_parseScratch.clear();
				while (m_it < m_end) {
					const char ch = *m_it++;
					if (ch == '"')
						break;

					if (ch == '\\') {
						if (!escaped) {
							escaped = true;
							const auto prefixLen = (size_t)((m_it - 1) - begin);
							if (prefixLen > 0)
								m_parseScratch.append(begin, (uint32_t)prefixLen);
						}

						if (m_it >= m_end)
							return false;
						const char esc = *m_it++;
						switch (esc) {
							case '"':
							case '\\':
							case '/':
								m_parseScratch.append(esc);
								break;
							case 'b':
								m_parseScratch.append('\b');
								break;
							case 'f':
								m_parseScratch.append('\f');
								break;
							case 'n':
								m_parseScratch.append('\n');
								break;
							case 'r':
								m_parseScratch.append('\r');
								break;
							case 't':
								m_parseScratch.append('\t');
								break;
							case 'u': {
								if ((uint32_t)(m_end - m_it) < 4)
									return false;
								GAIA_FOR(4) {
									const char h = m_it[i];
									const bool isHex = (h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') || (h >= 'A' && h <= 'F');
									if (!isHex)
										return false;
								}
								m_it += 4;
								m_parseScratch.append('?');
								break;
							}
							default:
								return false;
						}
					} else if (escaped)
						m_parseScratch.append(ch);
				}

				if (m_it <= begin || m_it > m_end || *(m_it - 1) != '"')
					return false;

				if (escaped) {
					if (fromScratch != nullptr)
						*fromScratch = true;
					out = m_parseScratch.view();
				} else {
					if (fromScratch != nullptr)
						*fromScratch = false;
					out = json_str_view(begin, (uint32_t)((m_it - 1) - begin));
				}
				return true;
			}

			bool parse_string(json_str& out) {
				json_str_view view;
				if (!parse_string_view(view))
					return false;
				out.assign(view);
				return true;
			}

			bool parse_string_eq(const char* literal) {
				json_str_view view;
				if (!parse_string_view(view))
					return false;
				const auto literalLen = (size_t)GAIA_STRLEN(literal, MaxLiteralLength);
				if (literalLen >= MaxLiteralLength)
					return false;
				return view.size() == literalLen && memcmp(view.data(), literal, literalLen) == 0;
			}

			bool parse_number(double& value) {
				ws();
				if (m_it == nullptr || m_end == nullptr || m_it >= m_end)
					return false;

				char* pEnd = nullptr;
				value = std::strtod(m_it, &pEnd);
				if (pEnd == m_it)
					return false;
				m_it = pEnd;
				return true;
			}

			bool parse_bool(bool& value) {
				if (parse_literal("true")) {
					value = true;
					return true;
				}
				if (parse_literal("false")) {
					value = false;
					return true;
				}
				return false;
			}

			bool parse_null() {
				return parse_literal("null");
			}

			bool skip_value() {
				ws();
				if (m_it == nullptr || m_end == nullptr || m_it >= m_end)
					return false;

				if (*m_it == '{') {
					++m_it;
					ws();
					if (consume('}'))
						return true;

					while (true) {
						json_str_view key;
						if (!parse_string_view(key))
							return false;
						if (!expect(':'))
							return false;
						if (!skip_value())
							return false;

						ws();
						if (consume(','))
							continue;
						if (consume('}'))
							return true;
						return false;
					}
				}

				if (*m_it == '[') {
					++m_it;
					ws();
					if (consume(']'))
						return true;

					while (true) {
						if (!skip_value())
							return false;
						ws();
						if (consume(','))
							continue;
						if (consume(']'))
							return true;
						return false;
					}
				}

				if (*m_it == '"') {
					json_str_view tmp;
					return parse_string_view(tmp);
				}

				if (*m_it == 't' || *m_it == 'f') {
					bool v = false;
					return parse_bool(v);
				}

				if (*m_it == 'n')
					return parse_null();

				double v = 0.0;
				return parse_number(v);
			}
		};

		namespace detail {
			template <typename T>
			inline void copy_field_bytes(uint8_t* pFieldData, uint32_t size, const T& v) {
				memcpy(pFieldData, &v, size < sizeof(v) ? size : (uint32_t)sizeof(v));
			}

			template <typename TInt>
			inline bool read_schema_field_json_int(ser_json& reader, uint8_t* pFieldData, uint32_t size, bool& ok) {
				double d = 0.0;
				if (!reader.parse_number(d))
					return false;

				if (!std::isfinite(d)) {
					ok = false;
					const TInt v = 0;
					copy_field_bytes(pFieldData, size, v);
					return true;
				}

				double clamped = std::trunc(d);
				if (clamped != d)
					ok = false;

				constexpr auto minVal = (double)std::numeric_limits<TInt>::lowest();
				constexpr auto maxVal = (double)std::numeric_limits<TInt>::max();
				if (clamped < minVal) {
					clamped = minVal;
					ok = false;
				} else if (clamped > maxVal) {
					clamped = maxVal;
					ok = false;
				}

				const TInt v = (TInt)clamped;
				copy_field_bytes(pFieldData, size, v);
				return true;
			}

			template <typename TFloat>
			inline bool read_schema_field_json_float(ser_json& reader, uint8_t* pFieldData, uint32_t size, bool& ok) {
				double d = 0.0;
				if (!reader.parse_number(d))
					return false;

				if (!std::isfinite(d)) {
					ok = false;
					const TFloat v = (TFloat)0;
					copy_field_bytes(pFieldData, size, v);
					return true;
				}

				const TFloat v = (TFloat)d;
				copy_field_bytes(pFieldData, size, v);
				return true;
			}

			inline bool
			write_schema_field_json(ser_json& writer, const uint8_t* pFieldData, serialization_type_id type, uint32_t size) {
				switch (type) {
					case serialization_type_id::s8: {
						int8_t v = 0;
						memcpy(&v, pFieldData, sizeof(v));
						writer.value_int(v);
						return true;
					}
					case serialization_type_id::u8: {
						uint8_t v = 0;
						memcpy(&v, pFieldData, sizeof(v));
						writer.value_int(v);
						return true;
					}
					case serialization_type_id::s16: {
						int16_t v = 0;
						memcpy(&v, pFieldData, sizeof(v));
						writer.value_int(v);
						return true;
					}
					case serialization_type_id::u16: {
						uint16_t v = 0;
						memcpy(&v, pFieldData, sizeof(v));
						writer.value_int(v);
						return true;
					}
					case serialization_type_id::s32: {
						int32_t v = 0;
						memcpy(&v, pFieldData, sizeof(v));
						writer.value_int(v);
						return true;
					}
					case serialization_type_id::u32: {
						uint32_t v = 0;
						memcpy(&v, pFieldData, sizeof(v));
						writer.value_int(v);
						return true;
					}
					case serialization_type_id::s64: {
						int64_t v = 0;
						memcpy(&v, pFieldData, sizeof(v));
						writer.value_int(v);
						return true;
					}
					case serialization_type_id::u64: {
						uint64_t v = 0;
						memcpy(&v, pFieldData, sizeof(v));
						writer.value_int(v);
						return true;
					}
					case serialization_type_id::b: {
						bool v = false;
						memcpy(&v, pFieldData, sizeof(v));
						writer.value_bool(v);
						return true;
					}
					case serialization_type_id::f32: {
						float v = 0.0f;
						memcpy(&v, pFieldData, sizeof(v));
						writer.value_float(v);
						return true;
					}
					case serialization_type_id::f64: {
						double v = 0.0;
						memcpy(&v, pFieldData, sizeof(v));
						writer.value_float(v);
						return true;
					}
					case serialization_type_id::c8: {
						const auto len = (uint32_t)GAIA_STRLEN((const char*)pFieldData, size);
						writer.value_string((const char*)pFieldData, len);
						return true;
					}
					default:
						writer.value_null();
						return false;
				}
			}

			inline bool read_schema_field_json(
					ser_json& reader, uint8_t* pFieldData, serialization_type_id type, uint32_t size, bool& ok) {
				if (reader.parse_null()) {
					ok = false;
					return true;
				}

				switch (type) {
					case serialization_type_id::s8: {
						return read_schema_field_json_int<int8_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::u8: {
						return read_schema_field_json_int<uint8_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::s16: {
						return read_schema_field_json_int<int16_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::u16: {
						return read_schema_field_json_int<uint16_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::s32: {
						return read_schema_field_json_int<int32_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::u32: {
						return read_schema_field_json_int<uint32_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::s64: {
						return read_schema_field_json_int<int64_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::u64: {
						return read_schema_field_json_int<uint64_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::f32: {
						return read_schema_field_json_float<float>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::f64: {
						return read_schema_field_json_float<double>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::b: {
						bool v = false;
						if (!reader.parse_bool(v))
							return false;
						copy_field_bytes(pFieldData, size, v);
						return true;
					}
					case serialization_type_id::c8: {
						json_str_view str;
						if (!reader.parse_string_view(str))
							return false;
						if (size == 0) {
							ok = false;
							return true;
						}

						memset(pFieldData, 0, size);
						const auto maxLen = size > 0 ? size - 1 : 0;
						const auto strLen = (uint32_t)str.size();
						const auto copyLen = strLen < maxLen ? strLen : maxLen;
						if (strLen > maxLen)
							ok = false;
						if (copyLen > 0)
							memcpy(pFieldData, str.data(), copyLen);
						return true;
					}
					default:
						ok = false;
						return reader.skip_value();
				}
			}

			template <typename TByteSink>
			inline bool parse_json_byte_array(ser_json& reader, TByteSink& out) {
				if (!reader.expect('['))
					return false;

				reader.ws();
				if (reader.consume(']'))
					return true;

				while (true) {
					double d = 0.0;
					if (!reader.parse_number(d))
						return false;
					if (d < 0.0 || d > 255.0)
						return false;

					const auto v = (uint32_t)d;
					if ((double)v != d)
						return false;

					const uint8_t byte = (uint8_t)v;
					out.save_raw(&byte, 1, serialization_type_id::u8);

					reader.ws();
					if (reader.consume(','))
						continue;
					if (reader.consume(']'))
						return true;
					return false;
				}
			}
		} // namespace detail
	} // namespace ser
} // namespace gaia

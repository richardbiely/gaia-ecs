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
		//! Non-owning string view used by JSON APIs.
		using json_str_view = util::str_view;
		//! Owning string used by JSON APIs.
		using json_str = util::str;

		//! Options controlling semantic JSON output and fallback payloads.
		enum JsonSaveFlags : uint32_t {
			//! Disables optional snapshot and fallback payloads.
			None = 0,
			//! Includes a binary snapshot.
			BinarySnapshot = 1u << 0,
			//! Allows raw component data as a fallback.
			RawFallback = 1u << 1,
			//! Enables the standard snapshot and raw fallback behavior.
			Default = BinarySnapshot | RawFallback
		};

		//! Explicit presentation and import policy for runtime values in semantic JSON.
		//! Numeric enum and bitmask payloads are the default. Symbolic behavior must be requested.
		struct RuntimeJsonPolicy final {
			//! Emits enum constants by name and accepts named enum values during import.
			bool symbolicEnums = false;
			//! Emits bitmask values as arrays of flag names and accepts those arrays during import.
			bool symbolicBitmasks = false;
		};

		//! Severity assigned to a JSON diagnostic.
		enum class JsonDiagSeverity : uint8_t {
			//! Informational message that does not indicate data loss.
			Info,
			//! Recoverable issue or adjusted input.
			Warning,
			//! Error that prevents faithful processing.
			Error
		};
		//! Machine-readable reason for a JSON diagnostic.
		enum class JsonDiagReason : uint8_t {
			//! No specific reason.
			None,
			//! Input contains an unrecognized field.
			UnknownField,
			//! Field location exceeds component storage.
			FieldOutOfBounds,
			//! Field value was adjusted while loading.
			FieldValueAdjusted,
			//! Payload supplied for a tag component was ignored.
			TagValueIgnored,
			//! Component payload is null.
			NullComponentPayload,
			//! Runtime fields and a raw fallback are both absent.
			MissingRuntimeFieldsOrRawPayload,
			//! Raw fallback cannot represent structure-of-arrays storage.
			SoaRawUnsupported,
			//! Component identifier is unknown.
			UnknownComponent,
			//! Operation does not support a tag component.
			TagComponentUnsupported,
			//! An entity name appears more than once.
			DuplicateEntityName,
			//! Component storage required by the payload is absent.
			MissingComponentStorage,
			//! Document omits the required archetypes section.
			MissingArchetypesSection,
			//! Document omits its format identifier.
			MissingFormatField,
			//! Document format version is not supported.
			UnsupportedFormatVersion,
			//! Input is not valid JSON.
			InvalidJson,
			//! Symbolic runtime constant is unknown.
			UnknownRuntimeConstant,
			//! Runtime constant is invalid in the current context.
			InvalidRuntimeConstant
		};

		//! One structured issue produced while reading or writing JSON.
		struct JsonDiagnostic {
			//! Diagnostic severity.
			JsonDiagSeverity severity = JsonDiagSeverity::Warning;
			//! Machine-readable reason.
			JsonDiagReason reason = JsonDiagReason::None;
			//! Path to the affected JSON value.
			json_str path;
			//! Human-readable explanation.
			json_str message;
		};

		//! Collection of JSON diagnostics with cached severity flags.
		struct JsonDiagnostics {
			//! Maximum path length read from a null-terminated string.
			static constexpr uint32_t MaxDiagPathLength = 1024;
			//! Maximum message length read from a null-terminated string.
			static constexpr uint32_t MaxDiagMessageLength = 2048;

			//! Recorded diagnostics in insertion order.
			cnt::darray<JsonDiagnostic> items;
			//! True when at least one warning has been added.
			bool hasWarnings = false;
			//! True when at least one error has been added.
			bool hasErrors = false;

			//! Appends a diagnostic.
			//! \param severity Diagnostic severity.
			//! \param reason Machine-readable reason.
			//! \param path Path to the affected value.
			//! \param message Human-readable explanation.
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
			//! Appends a diagnostic with a null-terminated message.
			//! \param severity Diagnostic severity.
			//! \param reason Machine-readable reason.
			//! \param path Path to the affected value.
			//! \param message Null-terminated explanation.
			void add(JsonDiagSeverity severity, JsonDiagReason reason, json_str_view path, const char* message) {
				add(severity, reason, path, json_str_view(message, (uint32_t)GAIA_STRLEN(message, MaxDiagMessageLength)));
			}
			//! Appends a diagnostic from null-terminated strings.
			//! \param severity Diagnostic severity.
			//! \param reason Machine-readable reason.
			//! \param path Null-terminated path.
			//! \param message Null-terminated explanation.
			void add(JsonDiagSeverity severity, JsonDiagReason reason, const char* path, const char* message) {
				add(severity, reason, json_str_view(path, (uint32_t)GAIA_STRLEN(path, MaxDiagPathLength)),
						json_str_view(message, (uint32_t)GAIA_STRLEN(message, MaxDiagMessageLength)));
			}

			//! Checks whether warnings or errors were recorded.
			//! \return True when the collection contains an issue.
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

			//! \param json Input buffer. It must outlive parsing.
			//! \param len Input length in bytes.
			ser_json(const char* json, uint32_t len) {
				reset_input(json, len);
			}
			//! Creates a reader over a null-terminated character array.
			//! \tparam N Array extent including the terminator.
			//! \param json Input character array. It must outlive parsing.
			template <size_t N>
			explicit ser_json(const char (&json)[N]) {
				static_assert(N > 0);
				reset_input(json, (uint32_t)(N - 1));
			}

			//! Sets an input JSON buffer for parsing.
			//! \param json Input buffer. It must outlive parsing, or null to clear input.
			//! \param len Input length in bytes.
			void reset_input(const char* json, uint32_t len) {
				if (json == nullptr) {
					m_it = nullptr;
					m_end = nullptr;
					return;
				}

				m_it = json;
				m_end = json + len;
			}
			//! Sets input from a null-terminated character array.
			//! \tparam N Array extent including the terminator.
			//! \param json Input character array. It must outlive parsing.
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
			//! \return Reference to the owned output string.
			GAIA_NODISCARD const json_str& str() const {
				return m_out;
			}

			//! Returns true if parser has reached end of input.
			//! \return True when no unread input remains.
			GAIA_NODISCARD bool eof() const {
				return m_it == nullptr || m_end == nullptr || m_it >= m_end;
			}

			//! Returns next non-consumed character.
			//! \return Next input character.
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

			//! Scans an integer JSON token without converting through floating point.
			//! Integral exponents are folded into the magnitude. Decimal tokens are reported through \a integerToken as
			//! non-integers and are not consumed.
			//! \param negative Receives whether the token has a leading minus sign.
			//! \param magnitude Receives the unsigned magnitude, saturated to UINT64_MAX on overflow.
			//! \param overflow Receives whether the magnitude exceeds UINT64_MAX.
			//! \param adjusted Receives whether a negative exponent discarded non-zero fractional digits.
			//! \param integerToken Receives whether the token represents an integer without a decimal point.
			//! \return False when the input does not begin with a numeric token.
			bool
			parse_integer_token(bool& negative, uint64_t& magnitude, bool& overflow, bool& adjusted, bool& integerToken) {
				ws();
				negative = false;
				magnitude = 0;
				overflow = false;
				adjusted = false;
				integerToken = false;
				if (m_it == nullptr || m_end == nullptr || m_it >= m_end)
					return false;

				const char* p = m_it;
				if (*p == '-') {
					negative = true;
					++p;
				}
				if (p >= m_end || *p < '0' || *p > '9')
					return false;
				const char* digitsBegin = p;
				if (*p == '0' && p + 1 < m_end && p[1] >= '0' && p[1] <= '9')
					return false;

				constexpr uint64_t MaxValue = (std::numeric_limits<uint64_t>::max)();
				while (p < m_end && *p >= '0' && *p <= '9') {
					const auto digit = (uint64_t)(*p - '0');
					if (magnitude > (MaxValue - digit) / 10)
						overflow = true;
					else if (!overflow)
						magnitude = magnitude * 10 + digit;
					++p;
				}
				const char* digitsEnd = p;

				if (p < m_end && *p == '.') {
					const char* decimalIt = p + 1;
					if (decimalIt >= m_end || *decimalIt < '0' || *decimalIt > '9')
						return false;
					while (decimalIt < m_end && *decimalIt >= '0' && *decimalIt <= '9')
						++decimalIt;
					if (decimalIt < m_end && (*decimalIt == 'e' || *decimalIt == 'E')) {
						++decimalIt;
						if (decimalIt < m_end && (*decimalIt == '+' || *decimalIt == '-'))
							++decimalIt;
						if (decimalIt >= m_end || *decimalIt < '0' || *decimalIt > '9')
							return false;
					}
					return true;
				}
				if (p < m_end && (*p == 'e' || *p == 'E')) {
					const char* exponentIt = p + 1;
					bool negativeExponent = false;
					if (exponentIt < m_end && (*exponentIt == '+' || *exponentIt == '-')) {
						negativeExponent = *exponentIt == '-';
						++exponentIt;
					}
					if (exponentIt >= m_end || *exponentIt < '0' || *exponentIt > '9')
						return false;

					uint32_t exponent = 0;
					bool exponentOverflow = false;
					while (exponentIt < m_end && *exponentIt >= '0' && *exponentIt <= '9') {
						const auto digit = (uint32_t)(*exponentIt - '0');
						if (exponent > (UINT32_MAX - digit) / 10)
							exponentOverflow = true;
						else if (!exponentOverflow)
							exponent = exponent * 10 + digit;
						++exponentIt;
					}

					if (negativeExponent && (exponentOverflow || exponent != 0)) {
						const auto digitCount = (uint32_t)(digitsEnd - digitsBegin);
						const auto keptCount = exponentOverflow || exponent >= digitCount ? 0U : digitCount - exponent;
						magnitude = 0;
						overflow = false;
						for (uint32_t i = 0; i < keptCount; ++i) {
							const auto digit = (uint64_t)(digitsBegin[i] - '0');
							if (magnitude > (MaxValue - digit) / 10)
								overflow = true;
							else if (!overflow)
								magnitude = magnitude * 10 + digit;
						}
						for (uint32_t i = keptCount; i < digitCount; ++i)
							adjusted = adjusted || digitsBegin[i] != '0';
					} else if (exponentOverflow || exponent > 19) {
						overflow = magnitude != 0;
					} else {
						while (exponent-- > 0) {
							if (magnitude > MaxValue / 10) {
								overflow = true;
								break;
							}
							magnitude *= 10;
						}
					}
					p = exponentIt;
				}

				if (overflow)
					magnitude = MaxValue;

				integerToken = true;
				m_it = p;
				return true;
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
			//! \cond INTERNAL
			template <typename T>
			inline void copy_field_bytes(uint8_t* pFieldData, uint32_t size, const T& v) {
				memcpy(pFieldData, &v, size < sizeof(v) ? size : (uint32_t)sizeof(v));
			}

			template <typename TInt>
			inline bool read_runtime_field_json_int(ser_json& reader, uint8_t* pFieldData, uint32_t size, bool& ok) {
				bool negative = false;
				uint64_t magnitude = 0;
				bool overflow = false;
				bool adjusted = false;
				bool integerToken = false;
				if (!reader.parse_integer_token(negative, magnitude, overflow, adjusted, integerToken))
					return false;

				if (integerToken) {
					if (adjusted)
						ok = false;
					TInt value = 0;
					if constexpr (std::is_signed_v<TInt>) {
						constexpr auto MaxPositive = (uint64_t)(std::numeric_limits<TInt>::max)();
						constexpr auto MaxNegative = MaxPositive + 1;
						if (negative) {
							if (overflow || magnitude > MaxNegative) {
								value = (std::numeric_limits<TInt>::lowest)();
								ok = false;
							} else if (magnitude == MaxNegative) {
								value = (std::numeric_limits<TInt>::lowest)();
							} else {
								value = (TInt) - (int64_t)magnitude;
							}
						} else if (overflow || magnitude > MaxPositive) {
							value = (std::numeric_limits<TInt>::max)();
							ok = false;
						} else {
							value = (TInt)magnitude;
						}
					} else {
						constexpr auto MaxValue = (uint64_t)(std::numeric_limits<TInt>::max)();
						if (negative) {
							if (overflow || magnitude != 0)
								ok = false;
						} else if (overflow || magnitude > MaxValue) {
							value = (std::numeric_limits<TInt>::max)();
							ok = false;
						} else {
							value = (TInt)magnitude;
						}
					}

					copy_field_bytes(pFieldData, size, value);
					return true;
				}

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

				constexpr auto minVal = (double)(std::numeric_limits<TInt>::lowest)();
				constexpr auto maxVal = (double)(std::numeric_limits<TInt>::max)();
				if (clamped < minVal) {
					clamped = minVal;
					ok = false;
				} else if constexpr (sizeof(TInt) == sizeof(uint64_t)) {
					if (clamped >= maxVal) {
						const TInt v = (std::numeric_limits<TInt>::max)();
						copy_field_bytes(pFieldData, size, v);
						ok = false;
						return true;
					}
				} else if (clamped > maxVal) {
					clamped = maxVal;
					ok = false;
				}

				const TInt v = (TInt)clamped;
				copy_field_bytes(pFieldData, size, v);
				return true;
			}

			template <typename TFloat>
			inline bool read_runtime_field_json_float(ser_json& reader, uint8_t* pFieldData, uint32_t size, bool& ok) {
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
			write_runtime_field_json(ser_json& writer, const uint8_t* pFieldData, serialization_type_id type, uint32_t size) {
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

			inline bool read_runtime_field_json(
					ser_json& reader, uint8_t* pFieldData, serialization_type_id type, uint32_t size, bool& ok) {
				if (reader.parse_null()) {
					ok = false;
					return true;
				}

				switch (type) {
					case serialization_type_id::s8: {
						return read_runtime_field_json_int<int8_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::u8: {
						return read_runtime_field_json_int<uint8_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::s16: {
						return read_runtime_field_json_int<int16_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::u16: {
						return read_runtime_field_json_int<uint16_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::s32: {
						return read_runtime_field_json_int<int32_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::u32: {
						return read_runtime_field_json_int<uint32_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::s64: {
						return read_runtime_field_json_int<int64_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::u64: {
						return read_runtime_field_json_int<uint64_t>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::f32: {
						return read_runtime_field_json_float<float>(reader, pFieldData, size, ok);
					}
					case serialization_type_id::f64: {
						return read_runtime_field_json_float<double>(reader, pFieldData, size, ok);
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
			//! \endcond
		} // namespace detail
	} // namespace ser
} // namespace gaia

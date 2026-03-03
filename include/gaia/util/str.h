#pragma once
#include "gaia/config/config.h"

#include <cstdint>
#include <cstring>

#include "gaia/cnt/darray.h"
#include "gaia/core/span.h"

namespace gaia {
	namespace util {
		//! Lightweight non-owning string view over a character sequence.
		struct str_view {
			const char* m_data = nullptr;
			uint32_t m_size = 0;

			//! Constructs an empty string view.
			str_view() = default;

			//! Constructs a string view from a pointer and an explicit length.
			//! \param data Pointer to the first character. Can be nullptr if @a size is 0.
			//! \param size Number of characters in the view.
			constexpr str_view(const char* data, uint32_t size): m_data(data), m_size(size) {}

			//! Constructs a string view from a literal, excluding its trailing null terminator.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit String literal.
			template <size_t N>
			constexpr str_view(const char (&lit)[N]): m_data(lit), m_size((uint32_t)(N - 1)) {
				static_assert(N > 0);
			}

			//! Returns the underlying character pointer.
			//! \return Pointer to the first character or nullptr for default-constructed empty views.
			GAIA_NODISCARD constexpr const char* data() const {
				return m_data;
			}

			//! Returns the number of characters in the view.
			//! \return Character count.
			GAIA_NODISCARD constexpr uint32_t size() const {
				return m_size;
			}

			//! Checks whether the view contains no characters.
			//! \return True if size() == 0.
			GAIA_NODISCARD constexpr bool empty() const {
				return m_size == 0;
			}

			//! Finds the first occurrence of substring @a value starting at index @a pos.
			//! \param value Needle string view.
			//! \param pos Start position in this view.
			//! \return Index of first match or BadIndex.
			GAIA_NODISCARD uint32_t find(str_view value, uint32_t pos = 0) const {
				return find(value.data(), value.size(), pos);
			}

			//! Finds the first occurrence of a character sequence starting at index @a pos.
			//! \param value Needle pointer.
			//! \param len Number of characters in @a value.
			//! \param pos Start position in this view.
			//! \return Index of first match or BadIndex.
			GAIA_NODISCARD uint32_t find(const char* value, uint32_t len, uint32_t pos) const {
				if (pos > m_size)
					return BadIndex;
				if (len == 0)
					return pos;
				if (len > m_size - pos)
					return BadIndex;

				for (uint32_t i = pos; i + len <= m_size; ++i) {
					if (memcmp(m_data + i, value, len) == 0)
						return i;
				}
				return BadIndex;
			}

			//! Finds the first occurrence of literal @a lit starting at index @a pos.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Needle literal.
			//! \param pos Start position in this view.
			//! \return Index of first match or BadIndex.
			template <size_t N>
			GAIA_NODISCARD uint32_t find(const char (&lit)[N], uint32_t pos = 0) const {
				static_assert(N > 0);
				return find(str_view(lit), pos);
			}

			//! Finds the first occurrence of character @a ch starting at index @a pos.
			//! \param ch Needle character.
			//! \param pos Start position in this view.
			//! \return Index of first match or BadIndex.
			GAIA_NODISCARD uint32_t find(char ch, uint32_t pos = 0) const {
				if (pos >= m_size)
					return BadIndex;
				for (uint32_t i = pos; i < m_size; ++i) {
					if (m_data[i] == ch)
						return i;
				}
				return BadIndex;
			}

			//! Finds the first character that is present in set @a chars.
			//! \param chars Set of accepted characters.
			//! \param pos Start position in this view.
			//! \return Index of first matching character or BadIndex.
			GAIA_NODISCARD uint32_t find_first_of(str_view chars, uint32_t pos = 0) const {
				if (pos >= m_size || chars.empty())
					return BadIndex;
				for (uint32_t i = pos; i < m_size; ++i) {
					if (contains(chars, m_data[i]))
						return i;
				}
				return BadIndex;
			}

			//! Finds the first occurrence of character @a ch.
			//! \param ch Needle character.
			//! \param pos Start position in this view.
			//! \return Index of first match or BadIndex.
			GAIA_NODISCARD uint32_t find_first_of(char ch, uint32_t pos = 0) const {
				return find(ch, pos);
			}

			//! Finds the first character that is present in literal set @a lit.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Set literal.
			//! \param pos Start position in this view.
			//! \return Index of first matching character or BadIndex.
			template <size_t N>
			GAIA_NODISCARD uint32_t find_first_of(const char (&lit)[N], uint32_t pos = 0) const {
				return find_first_of(str_view(lit), pos);
			}

			//! Finds the last character that is present in set @a chars.
			//! \param chars Set of accepted characters.
			//! \param pos Maximum position to consider, BadIndex means end of view.
			//! \return Index of last matching character or BadIndex.
			GAIA_NODISCARD uint32_t find_last_of(str_view chars, uint32_t pos = BadIndex) const {
				if (m_size == 0 || chars.empty())
					return BadIndex;

				uint32_t i = pos;
				if (i == BadIndex || i >= m_size)
					i = m_size - 1;

				for (;;) {
					if (contains(chars, m_data[i]))
						return i;
					if (i == 0)
						break;
					--i;
				}
				return BadIndex;
			}

			//! Finds the last occurrence of character @a ch.
			//! \param ch Needle character.
			//! \param pos Maximum position to consider, BadIndex means end of view.
			//! \return Index of last match or BadIndex.
			GAIA_NODISCARD uint32_t find_last_of(char ch, uint32_t pos = BadIndex) const {
				if (m_size == 0)
					return BadIndex;

				uint32_t i = pos;
				if (i == BadIndex || i >= m_size)
					i = m_size - 1;

				for (;;) {
					if (m_data[i] == ch)
						return i;
					if (i == 0)
						break;
					--i;
				}
				return BadIndex;
			}

			//! Finds the last character that is present in literal set @a lit.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Set literal.
			//! \param pos Maximum position to consider, BadIndex means end of view.
			//! \return Index of last matching character or BadIndex.
			template <size_t N>
			GAIA_NODISCARD uint32_t find_last_of(const char (&lit)[N], uint32_t pos = BadIndex) const {
				return find_last_of(str_view(lit), pos);
			}

			//! Finds the first character that is NOT present in set @a chars.
			//! \param chars Set of excluded characters.
			//! \param pos Start position in this view.
			//! \return Index of first non-matching character or BadIndex.
			GAIA_NODISCARD uint32_t find_first_not_of(str_view chars, uint32_t pos = 0) const {
				if (pos >= m_size)
					return BadIndex;
				if (chars.empty())
					return pos;

				for (uint32_t i = pos; i < m_size; ++i) {
					if (!contains(chars, m_data[i]))
						return i;
				}
				return BadIndex;
			}

			//! Finds the first character that is different from @a ch.
			//! \param ch Excluded character.
			//! \param pos Start position in this view.
			//! \return Index of first non-matching character or BadIndex.
			GAIA_NODISCARD uint32_t find_first_not_of(char ch, uint32_t pos = 0) const {
				if (pos >= m_size)
					return BadIndex;
				for (uint32_t i = pos; i < m_size; ++i) {
					if (m_data[i] != ch)
						return i;
				}
				return BadIndex;
			}

			//! Finds the first character that is NOT present in literal set @a lit.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Set literal.
			//! \param pos Start position in this view.
			//! \return Index of first non-matching character or BadIndex.
			template <size_t N>
			GAIA_NODISCARD uint32_t find_first_not_of(const char (&lit)[N], uint32_t pos = 0) const {
				return find_first_not_of(str_view(lit), pos);
			}

			//! Finds the last character that is NOT present in set @a chars.
			//! \param chars Set of excluded characters.
			//! \param pos Maximum position to consider, BadIndex means end of view.
			//! \return Index of last non-matching character or BadIndex.
			GAIA_NODISCARD uint32_t find_last_not_of(str_view chars, uint32_t pos = BadIndex) const {
				if (m_size == 0)
					return BadIndex;

				uint32_t i = pos;
				if (i == BadIndex || i >= m_size)
					i = m_size - 1;

				if (chars.empty())
					return i;

				for (;;) {
					if (!contains(chars, m_data[i]))
						return i;
					if (i == 0)
						break;
					--i;
				}
				return BadIndex;
			}

			//! Finds the last character that is different from @a ch.
			//! \param ch Excluded character.
			//! \param pos Maximum position to consider, BadIndex means end of view.
			//! \return Index of last non-matching character or BadIndex.
			GAIA_NODISCARD uint32_t find_last_not_of(char ch, uint32_t pos = BadIndex) const {
				if (m_size == 0)
					return BadIndex;

				uint32_t i = pos;
				if (i == BadIndex || i >= m_size)
					i = m_size - 1;

				for (;;) {
					if (m_data[i] != ch)
						return i;
					if (i == 0)
						break;
					--i;
				}
				return BadIndex;
			}

			//! Finds the last character that is NOT present in literal set @a lit.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Set literal.
			//! \param pos Maximum position to consider, BadIndex means end of view.
			//! \return Index of last non-matching character or BadIndex.
			template <size_t N>
			GAIA_NODISCARD uint32_t find_last_not_of(const char (&lit)[N], uint32_t pos = BadIndex) const {
				return find_last_not_of(str_view(lit), pos);
			}

			//! Compares this view with literal @a lit for exact byte equality.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Literal to compare with.
			//! \return True when lengths and contents are equal.
			template <size_t N>
			GAIA_NODISCARD bool operator==(const char (&lit)[N]) const {
				static_assert(N > 0);
				return m_size == (uint32_t)(N - 1) && (m_size == 0 || memcmp(m_data, lit, m_size) == 0);
			}

		private:
			GAIA_NODISCARD static bool contains(str_view set, char value) {
				for (uint32_t i = 0; i < set.m_size; ++i) {
					if (set.m_data[i] == value)
						return true;
				}
				return false;
			}
		};

		//! Lightweight owning string container with explicit length semantics (no implicit null terminator).
		struct str {
			cnt::darray<char> m_data;

			//! Constructs an empty string.
			str() = default;

			//! Constructs a string by copying view contents.
			//! \param view Source view.
			explicit str(str_view view) {
				assign(view);
			}

			//! Constructs a string from literal @a lit, excluding trailing null terminator.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Source literal.
			template <size_t N>
			explicit str(const char (&lit)[N]) {
				assign(lit);
			}

			//! Removes all characters from the string.
			void clear() {
				m_data.clear();
			}

			//! Reserves capacity for at least @a len characters.
			//! \param len Target character capacity.
			void reserve(uint32_t len) {
				m_data.reserve(len);
			}

			//! Replaces contents with @a size characters from @a data.
			//! \param data Source pointer.
			//! \param size Number of characters to copy.
			void assign(const char* data, uint32_t size) {
				m_data.resize(size);
				if (size > 0)
					memcpy(m_data.data(), data, size);
			}

			//! Replaces contents with @a view contents.
			//! \param view Source view.
			void assign(str_view view) {
				assign(view.data(), view.size());
			}

			//! Replaces contents with literal @a lit.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Source literal.
			template <size_t N>
			void assign(const char (&lit)[N]) {
				static_assert(N > 0);
				assign(lit, (uint32_t)(N - 1));
			}

			//! Appends @a size characters from @a data.
			//! \param data Source pointer.
			//! \param size Number of characters to append.
			void append(const char* data, uint32_t size) {
				const auto oldSize = this->size();
				m_data.resize(oldSize + size);
				if (size > 0)
					memcpy(m_data.data() + oldSize, data, size);
			}

			//! Appends @a view contents.
			//! \param view Source view.
			void append(str_view view) {
				append(view.data(), view.size());
			}

			//! Appends literal @a lit.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Source literal.
			template <size_t N>
			void append(const char (&lit)[N]) {
				static_assert(N > 0);
				append(lit, (uint32_t)(N - 1));
			}

			//! Appends a single character.
			//! \param ch Character to append.
			void append(char ch) {
				m_data.push_back(ch);
			}

			//! Returns read-only pointer to internal data.
			//! \return Pointer to first character or nullptr when empty.
			GAIA_NODISCARD const char* data() const {
				return m_data.data();
			}

			//! Returns mutable pointer to internal data.
			//! \return Pointer to first character or nullptr when empty.
			GAIA_NODISCARD char* data() {
				return m_data.data();
			}

			//! Returns number of characters stored in the string.
			//! \return Character count.
			GAIA_NODISCARD uint32_t size() const {
				return (uint32_t)m_data.size();
			}

			//! Checks whether the string contains no characters.
			//! \return True if size() == 0.
			GAIA_NODISCARD bool empty() const {
				return m_data.empty();
			}

			//! Returns a non-owning view over the current contents.
			//! \return View of this string.
			GAIA_NODISCARD str_view view() const {
				return str_view(data(), size());
			}

			//! Implicit conversion to non-owning view.
			//! \return View of this string.
			GAIA_NODISCARD operator str_view() const {
				return view();
			}

			//! Compares this string with literal @a lit for exact byte equality.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Literal to compare with.
			//! \return True when lengths and contents are equal.
			template <size_t N>
			GAIA_NODISCARD bool operator==(const char (&lit)[N]) const {
				static_assert(N > 0);
				const auto len = (uint32_t)(N - 1);
				return size() == len && (len == 0 || memcmp(data(), lit, len) == 0);
			}

			//! Compares this string with view @a other for exact byte equality.
			//! \param other View to compare with.
			//! \return True when lengths and contents are equal.
			GAIA_NODISCARD bool operator==(str_view other) const {
				return size() == other.size() && (size() == 0 || memcmp(data(), other.data(), size()) == 0);
			}

			//! Compares this string with string @a other for exact byte equality.
			//! \param other String to compare with.
			//! \return True when lengths and contents are equal.
			GAIA_NODISCARD bool operator==(const str& other) const {
				return operator==(other.view());
			}

			//! Finds the first occurrence of substring @a value starting at index @a pos.
			//! \param value Needle view.
			//! \param pos Start position in this string.
			//! \return Index of first match or BadIndex.
			GAIA_NODISCARD uint32_t find(str_view value, uint32_t pos = 0) const {
				return view().find(value, pos);
			}

			//! Finds the first occurrence of a character sequence starting at index @a pos.
			//! \param value Needle pointer.
			//! \param len Number of needle characters.
			//! \param pos Start position in this string.
			//! \return Index of first match or BadIndex.
			GAIA_NODISCARD uint32_t find(const char* value, uint32_t len, uint32_t pos) const {
				return view().find(value, len, pos);
			}

			//! Finds the first occurrence of literal @a lit starting at index @a pos.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Needle literal.
			//! \param pos Start position in this string.
			//! \return Index of first match or BadIndex.
			template <size_t N>
			GAIA_NODISCARD uint32_t find(const char (&lit)[N], uint32_t pos = 0) const {
				static_assert(N > 0);
				return find(str_view(lit), pos);
			}

			//! Finds the first occurrence of character @a ch starting at index @a pos.
			//! \param ch Needle character.
			//! \param pos Start position in this string.
			//! \return Index of first match or BadIndex.
			GAIA_NODISCARD uint32_t find(char ch, uint32_t pos = 0) const {
				return view().find(ch, pos);
			}

			//! Finds the first character that is present in set @a chars.
			//! \param chars Set of accepted characters.
			//! \param pos Start position in this string.
			//! \return Index of first matching character or BadIndex.
			GAIA_NODISCARD uint32_t find_first_of(str_view chars, uint32_t pos = 0) const {
				return view().find_first_of(chars, pos);
			}

			//! Finds the first occurrence of character @a ch.
			//! \param ch Needle character.
			//! \param pos Start position in this string.
			//! \return Index of first match or BadIndex.
			GAIA_NODISCARD uint32_t find_first_of(char ch, uint32_t pos = 0) const {
				return view().find_first_of(ch, pos);
			}

			//! Finds the first character that is present in literal set @a lit.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Set literal.
			//! \param pos Start position in this string.
			//! \return Index of first matching character or BadIndex.
			template <size_t N>
			GAIA_NODISCARD uint32_t find_first_of(const char (&lit)[N], uint32_t pos = 0) const {
				return view().find_first_of(lit, pos);
			}

			//! Finds the last character that is present in set @a chars.
			//! \param chars Set of accepted characters.
			//! \param pos Maximum position to consider, BadIndex means end of string.
			//! \return Index of last matching character or BadIndex.
			GAIA_NODISCARD uint32_t find_last_of(str_view chars, uint32_t pos = BadIndex) const {
				return view().find_last_of(chars, pos);
			}

			//! Finds the last occurrence of character @a ch.
			//! \param ch Needle character.
			//! \param pos Maximum position to consider, BadIndex means end of string.
			//! \return Index of last match or BadIndex.
			GAIA_NODISCARD uint32_t find_last_of(char ch, uint32_t pos = BadIndex) const {
				return view().find_last_of(ch, pos);
			}

			//! Finds the last character that is present in literal set @a lit.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Set literal.
			//! \param pos Maximum position to consider, BadIndex means end of string.
			//! \return Index of last matching character or BadIndex.
			template <size_t N>
			GAIA_NODISCARD uint32_t find_last_of(const char (&lit)[N], uint32_t pos = BadIndex) const {
				return view().find_last_of(lit, pos);
			}

			//! Finds the first character that is NOT present in set @a chars.
			//! \param chars Set of excluded characters.
			//! \param pos Start position in this string.
			//! \return Index of first non-matching character or BadIndex.
			GAIA_NODISCARD uint32_t find_first_not_of(str_view chars, uint32_t pos = 0) const {
				return view().find_first_not_of(chars, pos);
			}

			//! Finds the first character that is different from @a ch.
			//! \param ch Excluded character.
			//! \param pos Start position in this string.
			//! \return Index of first non-matching character or BadIndex.
			GAIA_NODISCARD uint32_t find_first_not_of(char ch, uint32_t pos = 0) const {
				return view().find_first_not_of(ch, pos);
			}

			//! Finds the first character that is NOT present in literal set @a lit.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Set literal.
			//! \param pos Start position in this string.
			//! \return Index of first non-matching character or BadIndex.
			template <size_t N>
			GAIA_NODISCARD uint32_t find_first_not_of(const char (&lit)[N], uint32_t pos = 0) const {
				return view().find_first_not_of(lit, pos);
			}

			//! Finds the last character that is NOT present in set @a chars.
			//! \param chars Set of excluded characters.
			//! \param pos Maximum position to consider, BadIndex means end of string.
			//! \return Index of last non-matching character or BadIndex.
			GAIA_NODISCARD uint32_t find_last_not_of(str_view chars, uint32_t pos = BadIndex) const {
				return view().find_last_not_of(chars, pos);
			}

			//! Finds the last character that is different from @a ch.
			//! \param ch Excluded character.
			//! \param pos Maximum position to consider, BadIndex means end of string.
			//! \return Index of last non-matching character or BadIndex.
			GAIA_NODISCARD uint32_t find_last_not_of(char ch, uint32_t pos = BadIndex) const {
				return view().find_last_not_of(ch, pos);
			}

			//! Finds the last character that is NOT present in literal set @a lit.
			//! \tparam N Number of characters in the literal including the trailing null terminator.
			//! \param lit Set literal.
			//! \param pos Maximum position to consider, BadIndex means end of string.
			//! \return Index of last non-matching character or BadIndex.
			template <size_t N>
			GAIA_NODISCARD uint32_t find_last_not_of(const char (&lit)[N], uint32_t pos = BadIndex) const {
				return view().find_last_not_of(lit, pos);
			}
		};

		//! Returns true when @a c is an ASCII whitespace character.
		//! \param c Character to test.
		//! \return True for ' ' and characters in range ['\t', '\r'].
		GAIA_NODISCARD constexpr bool is_whitespace(char c) {
			return c == ' ' || (c >= '\t' && c <= '\r');
		}

		//! Trims ASCII whitespace from both ends of @a expr.
		//! \param expr Input string view.
		//! \return Trimmed sub-view into @a expr.
		GAIA_NODISCARD constexpr str_view trim(str_view expr) {
			const auto len = expr.size();
			if (len == 0)
				return {};

			uint32_t beg = 0;
			while (beg < len && is_whitespace(expr.data()[beg]))
				++beg;
			if (beg == len)
				return {};

			uint32_t end = len - 1;
			while (end > beg && is_whitespace(expr.data()[end]))
				--end;
			return str_view(expr.data() + beg, end - beg + 1);
		}

		//! Trims ASCII whitespace from both ends of @a expr.
		//! \param expr Input character span.
		//! \return Trimmed subspan view into @a expr.
		GAIA_NODISCARD constexpr std::span<const char> trim(std::span<const char> expr) {
			const auto trimmed = trim(str_view(expr.data(), (uint32_t)expr.size()));
			return std::span<const char>(trimmed.data(), trimmed.size());
		}
	} // namespace util
} // namespace gaia

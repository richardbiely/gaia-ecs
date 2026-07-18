#pragma once
#include "gaia/config/config.h"

#include "gaia/core/hashing_policy.h"
#include "gaia/core/span.h"

namespace gaia {
	namespace meta {

		//----------------------------------------------------------------------
		// Type meta data
		//----------------------------------------------------------------------

		//! Provides compile-time compiler-derived names and hashes for C++ types.
		struct type_info final {
		private:
			constexpr static size_t find_first_of(const char* data, size_t len, char toFind, size_t startPos = 0) {
				for (size_t i = startPos; i < len; ++i) {
					if (data[i] == toFind)
						return i;
				}
				return size_t(-1);
			}

			constexpr static size_t find_last_of(const char* data, size_t len, char c, size_t startPos = size_t(-1)) {
				const auto minValue = startPos <= len - 1 ? startPos : len - 1;
				for (int64_t i = (int64_t)minValue; i >= 0; --i) {
					if (data[i] == c)
						return (size_t)i;
				}
				return size_t(-1);
			}

		public:
			//! Returns the complete compiler function signature containing T.
			//! \tparam T Type to describe.
			//! \return Null-terminated compiler signature string.
			template <typename T>
			GAIA_NODISCARD static constexpr const char* full_name() noexcept {
				return GAIA_PRETTY_FUNCTION;
			}

			//! Extracts the unqualified compiler spelling of T from the function signature.
			//! \tparam T Type to describe.
			//! \return Span covering the extracted type name.
			template <typename T>
			GAIA_NODISCARD static constexpr auto name() noexcept {
				// MSVC:
				//		const char* __cdecl ecs::type_info::name<struct ecs::EnfEntity>(void)
				//   -> ecs::EnfEntity
				// Clang/Clang-cl/GCC:
				//		const ecs::type_info::name() [T = ecs::EnfEntity]
				//   -> ecs::EnfEntity

				// Note:
				//		We don't want to use std::string_view here because it would only make it harder on compile-times.
				//		In fact, even if we did, we need to be afraid of compiler issues.
				// 		Clang 8 and older wouldn't compile because their string_view::find_last_of doesn't work
				//		in constexpr context. Tested with and without LIBCPP
				//		https://stackoverflow.com/questions/56484834/constexpr-stdstring-viewfind-last-of-doesnt-work-on-clang-8-with-libstdc
				//		As a workaround find_first_of and find_last_of were implemented

				size_t strLen = 0;
				while (GAIA_PRETTY_FUNCTION[strLen] != '\0')
					++strLen;

				std::span<const char> name{GAIA_PRETTY_FUNCTION, strLen};
				const auto prefixPos = find_first_of(name.data(), name.size(), GAIA_PRETTY_FUNCTION_PREFIX);
				const auto start = find_first_of(name.data(), name.size(), ' ', prefixPos + 1);
				const auto end = find_last_of(name.data(), name.size(), GAIA_PRETTY_FUNCTION_SUFFIX);
				return name.subspan(start + 1, end - start - 1);
			}

			//! Calculates the stable Gaia-ECS hash of the compiler spelling of T.
			//! \tparam T Type to hash.
			//! \return 64-bit hash of name<T>().
			template <typename T>
			GAIA_NODISCARD static constexpr auto hash() noexcept {
#if GAIA_COMPILER_MSVC && _MSC_VER <= 1916
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4307)
#endif

				auto n = name<T>();
				return core::calculate_hash64(n.data(), n.size());

#if GAIA_COMPILER_MSVC && _MSC_VER <= 1916
				GAIA_MSVC_WARNING_PUSH()
#endif
			}
		};

	} // namespace meta
} // namespace gaia

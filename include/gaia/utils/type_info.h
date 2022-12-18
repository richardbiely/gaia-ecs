#pragma once
#include "../config/config.h"
#include "../utils/span.h"
#include "hashing_policy.h"

namespace gaia {
	namespace utils {

		//! Provides statically generated unique identifier.
		struct GAIA_API type_seq final {
			GAIA_NODISCARD static uint32_t next() noexcept {
				static uint32_t value{};
				return value++;
			}
		};

		//! Provides statically generated unique identifier for a given group of types.
		template <typename...>
		class type_group {
			inline static uint32_t identifier{};

		public:
			template <typename... Type>
			inline static const uint32_t id = identifier++;
		};

		template <>
		class type_group<void>;

		//----------------------------------------------------------------------
		// Type meta data
		//----------------------------------------------------------------------

		struct type_info final {
		private:
			constexpr static size_t GetMin(size_t a, size_t b) {
				return b < a ? b : a;
			}

			constexpr static size_t FindFirstOf(const char* data, size_t len, char toFind, size_t startPos = 0) {
				for (size_t i = startPos; i < len; ++i) {
					if (data[i] == toFind)
						return i;
				}
				return size_t(-1);
			}

			constexpr static size_t FindLastOf(const char* data, size_t len, char c, size_t startPos = size_t(-1)) {
				for (int64_t i = (int64_t)GetMin(len - 1, startPos); i >= 0; --i) {
					if (data[i] == c)
						return i;
				}
				return size_t(-1);
			}

		public:
			template <typename T>
			static uint32_t index() noexcept {
				return type_group<type_info>::id<T>;
			}

			template <typename T>
			GAIA_NODISCARD static constexpr const char* full_name() noexcept {
				return GAIA_PRETTY_FUNCTION;
			}

			template <typename T>
			GAIA_NODISCARD static constexpr auto name() noexcept {
				// MSVC:
				//		const char* __cdecl ecs::ComponentInfo::name<struct ecs::EnfEntity>(void)
				//   -> ecs::EnfEntity
				// Clang/GCC:
				//		const ecs::ComponentInfo::name() [T = ecs::EnfEntity]
				//   -> ecs::EnfEntity

				// Note:
				//		We don't want to use std::string_view here because it would only make it harder on compile-times.
				//		In fact, even if we did, we need to be afraid of compiler issues.
				// 		Clang 8 and older wouldn't compile because their string_view::find_last_of doesn't work
				//		in constexpr context. Tested with and without LIBCPP
				//		https://stackoverflow.com/questions/56484834/constexpr-stdstring-viewfind-last-of-doesnt-work-on-clang-8-with-libstdc
				//		As a workaround FindFirstOf and FindLastOf were implemented

				size_t strLen = 0;
				while (GAIA_PRETTY_FUNCTION[strLen] != '\0')
					++strLen;

				std::span<const char> name{GAIA_PRETTY_FUNCTION, strLen};
				const auto prefixPos = FindFirstOf(name.data(), name.size(), GAIA_PRETTY_FUNCTION_PREFIX);
				const auto start = FindFirstOf(name.data(), name.size(), ' ', prefixPos + 1);
				const auto end = FindLastOf(name.data(), name.size(), GAIA_PRETTY_FUNCTION_SUFFIX);
				return name.subspan(start + 1, end - start - 1);
			}

			template <typename T>
			GAIA_NODISCARD static constexpr auto hash() noexcept {
#if GAIA_COMPILER_MSVC && _MSV_VER <= 1916
				GAIA_MSVC_WARNING_PUSH()
				GAIA_MSVC_WARNING_DISABLE(4307)
#endif

				auto n = name<T>();
				return calculate_hash64(n.data(), n.size());

#if GAIA_COMPILER_MSVC && _MSV_VER <= 1916
				GAIA_MSVC_WARNING_PUSH()
#endif
			}
		};

	} // namespace utils
} // namespace gaia

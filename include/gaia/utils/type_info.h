#pragma once

#include "../config/config.h"
#include "hashing_policy.h"
#include <string_view>

namespace gaia {
	namespace utils {

		//! Provides statically generated unique identifier.
		struct GAIA_API type_seq final {
			[[nodiscard]] static uint32_t next() noexcept {
				static uint32_t value{};
				return value++;
			}
		};

		//! Provides statically generated unique identifier for a given group of
		//! types.
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
				return (b < a) ? b : a;
			}

			constexpr static size_t FindFirstOf(const char* data, size_t len, char toFind, size_t startPos = 0) {
				for (size_t i = startPos; i < len; ++i) {
					if (data[i] == toFind)
						return i;
				}
				return std::string::npos;
			}

			constexpr static size_t FindLastOf(const char* data, size_t len, char c, size_t startPos = std::string::npos) {
				for (int64_t i = (int64_t)GetMin(len - 1, startPos); i >= 0; --i) {
					if (data[i] == c)
						return i;
				}
				return std::string::npos;
			}

		public:
			template <typename T>
			static uint32_t index() noexcept {
				return type_group<type_info>::id<T>;
			}

			template <typename T>
			[[nodiscard]] static constexpr const char* full_name() noexcept {
				return GAIA_PRETTY_FUNCTION;
			}

			template <typename T>
			[[nodiscard]] static constexpr auto name() noexcept {
				// MSVC  : const char* __cdecl
				// ecs::ComponentMetaData::GetMetaName<struct ecs::EnfEntity>(void)
				//   res : ecs::EnfEntity
				// Clang : const ecs::ComponentMetaData::GetMetaName() [T =
				// ecs::EnfEntity]
				//   res : ecs::EnfEntity

				// Note:
				// 		Clang 8 and older wouldn't compile because their string_view::find_last_of doesn't work
				//		in constexpr context. Tested with and without LIBCPP
				//		https://stackoverflow.com/questions/56484834/constexpr-stdstring-viewfind-last-of-doesnt-work-on-clang-8-with-libstdc
				//		As a workaround FindFirstOf and FindLastOf were implemented

				std::string_view name{GAIA_PRETTY_FUNCTION};
				const auto prefixPos = FindFirstOf(name.data(), name.length(), GAIA_PRETTY_FUNCTION_PREFIX);
				const auto start = FindFirstOf(name.data(), name.length(), ' ', prefixPos + 1);
				const auto end = FindLastOf(name.data(), name.length(), GAIA_PRETTY_FUNCTION_SUFFIX);
				return name.substr(start + 1, end - start - 1);
			}

			template <typename T>
			[[nodiscard]] static constexpr auto hash() noexcept {
				return hash_fnv1a_64(name<T>().data(), name<T>().length());
			}
		};

	} // namespace utils
} // namespace gaia
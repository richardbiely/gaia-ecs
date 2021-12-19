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

				// Warning:
				// 		Clang 8 + LIBCPP doesn't compile because its string_view::find_last_of doesn't work in constexpr context
        //		https://stackoverflow.com/questions/56484834/constexpr-stdstring-viewfind-last-of-doesnt-work-on-clang-8-with-libstdc

				std::string_view name{GAIA_PRETTY_FUNCTION};
				const auto prefixPos = name.find_first_of(GAIA_PRETTY_FUNCTION_PREFIX);
				const auto start = name.find_first_of(' ', prefixPos + 1);
				const auto end = name.find_last_of(GAIA_PRETTY_FUNCTION_SUFFIX);
				return name.substr(start + 1, end - start - 1);
			}

			template <typename T>
			[[nodiscard]] static constexpr auto hash() noexcept {
				return hash_fnv1a_64(name<T>().data(), name<T>().length());
			}
		};

	} // namespace utils
} // namespace gaia
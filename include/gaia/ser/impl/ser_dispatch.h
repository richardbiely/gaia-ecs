#pragma once
#include "gaia/config/config.h"

#include <type_traits>
#include <utility>

#include "gaia/core/utility.h"
#include "gaia/meta/reflection.h"
#include "gaia/ser/ser_common.h"

namespace gaia {
	namespace ser {
		namespace detail {
			template <typename Serializer, typename T, typename SaveTrivial>
			void save_dispatch(Serializer& s, const T& arg, SaveTrivial&& saveTrivial) {
				using U = core::raw_t<T>;

				// Custom save() has precedence
				if constexpr (has_func_save<U, Serializer&>::value) {
					arg.save(s);
				} else if constexpr (has_tag_save<Serializer, U>::value) {
					tag_invoke(save_v, s, static_cast<const U&>(arg));
				}
				// Trivially serializable types
				else if constexpr (is_trivially_serializable<U>::value) {
					saveTrivial(s, arg);
				}
				// Types which have size(), begin() and end() member functions
				else if constexpr (core::has_size_begin_end<U>::value) {
					const auto size = arg.size();
					saveTrivial(s, size);

					for (const auto& e: std::as_const(arg))
						save_dispatch(s, e, saveTrivial);
				}
				// Classes
				else if constexpr (std::is_class_v<U>) {
					meta::each_member(GAIA_FWD(arg), [&s, &saveTrivial](auto&&... items) {
						// TODO: Handle contiguous blocks of trivially copyable types
						(save_dispatch(s, items, saveTrivial), ...);
					});
				} else
					static_assert(!sizeof(U), "Type is not supported for serialization, yet");
			}

			template <typename Serializer, typename T, typename LoadTrivial>
			void load_dispatch(Serializer& s, T& arg, LoadTrivial&& loadTrivial) {
				using U = core::raw_t<T>;

				// Custom load() has precedence
				if constexpr (has_func_load<U, Serializer&>::value) {
					arg.load(s);
				} else if constexpr (has_tag_load<Serializer, U>::value) {
					tag_invoke(load_v, s, static_cast<U&>(arg));
				}
				// Trivially serializable types
				else if constexpr (is_trivially_serializable<U>::value) {
					loadTrivial(s, arg);
				}
				// Types which have size(), begin() and end() member functions
				else if constexpr (core::has_size_begin_end<U>::value) {
					auto size = arg.size();
					loadTrivial(s, size);

					if constexpr (has_func_resize<U, size_t>::value) {
						// If resize is present, use it
						arg.resize(size);

						// NOTE: We can't do it this way. If there are containers with the overloaded
						//       operator=, the result might not be what one would expect.
						//       E.g., in our case, SoA containers need specific handling.
						// for (auto&& e: arg)
						// 	load_dispatch(s, e, loadTrivial);

						uint32_t i = 0;
						for (auto e: arg) {
							load_dispatch(s, e, loadTrivial);
							arg[i] = GAIA_MOV(e);
							++i;
						}
					} else {
						// With no resize present, write directly into memory
						GAIA_FOR(size) {
							using arg_type = typename std::remove_pointer<decltype(arg.data())>::type;
							arg_type val;
							load_dispatch(s, val, loadTrivial);
							arg[i] = val;
						}
					}
				}
				// Classes
				else if constexpr (std::is_class_v<U>) {
					meta::each_member(GAIA_FWD(arg), [&s, &loadTrivial](auto&&... items) {
						// TODO: Handle contiguous blocks of trivially copyable types
						(load_dispatch(s, items, loadTrivial), ...);
					});
				} else
					static_assert(!sizeof(U), "Type is not supported for serialization, yet");
			}
		} // namespace detail
	} // namespace ser
} // namespace gaia

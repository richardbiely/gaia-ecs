#pragma once
#include "gaia/config/config.h"

#include <type_traits>
#include <utility>

#include "gaia/core/utility.h"
#include "gaia/meta/reflection.h"
#include "ser_common.h"

namespace gaia {
	namespace ser {
		struct ISerializer {
			ISerializer() = default;
			virtual ~ISerializer() = default;
			ISerializer(const ISerializer&) = default;
			ISerializer(ISerializer&&) = default;
			ISerializer& operator=(const ISerializer&) = default;
			ISerializer& operator=(ISerializer&&) = default;

			template <typename T>
			void save(const T& arg) {
				using U = core::raw_t<T>;

				// Custom save() has precedence
				if constexpr (has_func_save<U, ISerializer&>::value) {
					arg.save(*this);
				} else if constexpr (has_tag_save<ISerializer, U>::value) {
					tag_invoke(save_v, *this, static_cast<const U&>(arg));
				}
				// Trivially serializable types
				else if constexpr (is_trivially_serializable<U>::value) {
					this->save_raw(arg);
				}
				// Types which have size(), begin() and end() member functions
				else if constexpr (core::has_size_begin_end<U>::value) {
					const auto size = arg.size();
					this->save_raw(size);

					for (const auto& e: std::as_const(arg))
						save(e);
				}
				// Classes
				else if constexpr (std::is_class_v<U>) {
					meta::each_member(GAIA_FWD(arg), [&](auto&&... items) {
						// TODO: Handle contiguous blocks of trivially copyable types
						(save(items), ...);
					});
				} else
					static_assert(!sizeof(U), "Type is not supported for serialization, yet");
			}

			template <typename T>
			void load(T& arg) {
				using U = core::raw_t<T>;

				// Custom load() has precedence
				if constexpr (has_func_load<U, ISerializer&>::value) {
					arg.load(*this);
				} else if constexpr (has_tag_load<ISerializer, U>::value) {
					tag_invoke(load_v, *this, static_cast<U&>(arg));
				}
				// Trivially serializable types
				else if constexpr (is_trivially_serializable<U>::value) {
					this->load_raw(arg);
				}
				// Types which have size(), begin() and end() member functions
				else if constexpr (core::has_size_begin_end<U>::value) {
					auto size = arg.size();
					this->load_raw(size);

					if constexpr (has_func_resize<U, size_t>::value) {
						// If resize is present, use it
						arg.resize(size);
						for (auto&& e: arg)
							load(e);
					} else {
						// With no resize present, write directly into memory
						GAIA_FOR(size) {
							using arg_type = typename std::remove_pointer<decltype(arg.data())>::type;
							arg_type val;
							load(val);
							arg[i] = val;
						}
					}
				}
				// Classes
				else if constexpr (std::is_class_v<U>) {
					meta::each_member(GAIA_FWD(arg), [&](auto&&... items) {
						// TODO: Handle contiguous blocks of trivially copyable types
						(load(items), ...);
					});
				} else
					static_assert(!sizeof(U), "Type is not supported for serialization, yet");
			}

#if GAIA_ASSERT_ENABLED
			template <typename T>
			void check(const T& arg) {
				T tmp{};

				// Make sure that we write just as many bytes as we read.
				// If positions are the same there is a good chance that save and load match.
				const auto pos0 = tell();
				save(arg);
				const auto pos1 = tell();
				seek(pos0);
				load(tmp);
				const auto pos2 = tell();
				GAIA_ASSERT(pos2 == pos1);

				// Return back to the original position in the buffer.
				seek(pos0);
			}
#endif

			template <typename T>
			void save_raw(const T& value) {
				save_raw(&value, sizeof(value), ser::type_id<T>());
			}

			template <typename T>
			void load_raw(T& value) {
				load_raw(&value, sizeof(value), ser::type_id<T>());
			}

			virtual void save_raw(const void* src, uint32_t size, serialization_type_id id) {
				(void)id;
				(void)src;
				(void)size;
			}

			virtual void load_raw(void* src, uint32_t size, serialization_type_id id) {
				(void)id;
				(void)src;
				(void)size;
			}

			virtual const char* data() const {
				return nullptr;
			}

			virtual void reset() {}

			virtual uint32_t tell() const {
				return 0;
			}

			virtual uint32_t bytes() const {
				return 0;
			}

			virtual void seek(uint32_t pos) {
				(void)pos;
			}
		};
	} // namespace ser
} // namespace gaia

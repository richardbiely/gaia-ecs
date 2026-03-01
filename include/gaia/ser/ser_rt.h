#pragma once
#include "gaia/config/config.h"

#include <type_traits>
#include <utility>

#include "gaia/core/utility.h"
#include "gaia/ser/impl/ser_dispatch.h"
#include "gaia/ser/ser_common.h"

namespace gaia {
	namespace ser {
		namespace detail {
			template <typename T, typename = void>
			struct has_save_raw_ptr: std::false_type {};
			template <typename T>
			struct has_save_raw_ptr<
					T, std::void_t<decltype(std::declval<T&>().save_raw(
								 (const void*)nullptr, uint32_t{}, ser::serialization_type_id{}))>>: std::true_type {};

			template <typename T, typename = void>
			struct has_load_raw_ptr: std::false_type {};
			template <typename T>
			struct has_load_raw_ptr<
					T,
					std::void_t<decltype(std::declval<T&>().load_raw((void*)nullptr, uint32_t{}, ser::serialization_type_id{}))>>:
					std::true_type {};

			template <typename T, typename = void>
			struct has_data_ptr: std::false_type {};
			template <typename T>
			struct has_data_ptr<T, std::void_t<decltype(std::declval<const T&>().data())>>: std::true_type {};

			template <typename T, typename = void>
			struct has_reset_fn: std::false_type {};
			template <typename T>
			struct has_reset_fn<T, std::void_t<decltype(std::declval<T&>().reset())>>: std::true_type {};

			template <typename T, typename = void>
			struct has_tell_fn: std::false_type {};
			template <typename T>
			struct has_tell_fn<T, std::void_t<decltype(std::declval<const T&>().tell())>>: std::true_type {};

			template <typename T, typename = void>
			struct has_bytes_fn: std::false_type {};
			template <typename T>
			struct has_bytes_fn<T, std::void_t<decltype(std::declval<const T&>().bytes())>>: std::true_type {};

			template <typename T, typename = void>
			struct has_seek_fn: std::false_type {};
			template <typename T>
			struct has_seek_fn<T, std::void_t<decltype(std::declval<T&>().seek(uint32_t{}))>>: std::true_type {};
		} // namespace detail

		//! Runtime serializer type-erased handle.
		//! Traversal logic is shared with compile-time serialization, while raw I/O is delegated
		//! through function pointers bound to a concrete serializer instance.
		//! This is a binary traversal API; JSON document I/O uses ser::ser_json.
		struct serializer {
			using SaveRawFn = void (*)(void*, const void*, uint32_t, serialization_type_id);
			using LoadRawFn = void (*)(void*, void*, uint32_t, serialization_type_id);
			using DataFn = const char* (*)(const void*);
			using ResetFn = void (*)(void*);
			using TellFn = uint32_t (*)(const void*);
			using BytesFn = uint32_t (*)(const void*);
			using SeekFn = void (*)(void*, uint32_t);

			void* ctx = nullptr;
			SaveRawFn func_save_raw = nullptr;
			LoadRawFn func_load_raw = nullptr;
			DataFn func_data = nullptr;
			ResetFn func_reset = nullptr;
			TellFn func_tell = nullptr;
			BytesFn func_bytes = nullptr;
			SeekFn func_seek = nullptr;

			serializer() = default;

			serializer(
					void* ctx, SaveRawFn saveRaw, LoadRawFn loadRaw, DataFn data, ResetFn reset, TellFn tell, BytesFn bytes,
					SeekFn seek):
					ctx(ctx), func_save_raw(saveRaw), func_load_raw(loadRaw), func_data(data), func_reset(reset), func_tell(tell),
					func_bytes(bytes), func_seek(seek) {}

			GAIA_NODISCARD bool valid() const {
				return ctx != nullptr && func_save_raw != nullptr && func_load_raw != nullptr && func_tell != nullptr &&
							 func_seek != nullptr;
			}

			template <typename T>
			void save(const T& arg) {
				auto saveTrivial = [](auto& serializer, const auto& value) {
					serializer.save_raw(value);
				};
				detail::save_dispatch(*this, arg, saveTrivial);
			}

			template <typename T>
			void load(T& arg) {
				auto loadTrivial = [](auto& serializer, auto& value) {
					serializer.load_raw(value);
				};
				detail::load_dispatch(*this, arg, loadTrivial);
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

			void save_raw(const void* src, uint32_t size, serialization_type_id id) {
				GAIA_ASSERT(valid());
				func_save_raw(ctx, src, size, id);
			}

			void load_raw(void* src, uint32_t size, serialization_type_id id) {
				GAIA_ASSERT(valid());
				func_load_raw(ctx, src, size, id);
			}

			GAIA_NODISCARD const char* data() const {
				if (func_data == nullptr)
					return nullptr;
				return func_data(ctx);
			}

			void reset() {
				if (func_reset == nullptr)
					return;
				func_reset(ctx);
			}

			GAIA_NODISCARD uint32_t tell() const {
				GAIA_ASSERT(valid());
				return func_tell(ctx);
			}

			GAIA_NODISCARD uint32_t bytes() const {
				if (func_bytes == nullptr)
					return 0;
				return func_bytes(ctx);
			}

			void seek(uint32_t pos) {
				GAIA_ASSERT(valid());
				func_seek(ctx, pos);
			}

			template <typename TSerializer>
			static serializer bind(TSerializer& obj) {
				static_assert(detail::has_save_raw_ptr<TSerializer>::value, "Serializer must expose save_raw(ptr,size,id)");
				static_assert(detail::has_load_raw_ptr<TSerializer>::value, "Serializer must expose load_raw(ptr,size,id)");
				static_assert(detail::has_tell_fn<TSerializer>::value, "Serializer must expose tell()");
				static_assert(detail::has_seek_fn<TSerializer>::value, "Serializer must expose seek(pos)");

				serializer ref;
				ref.ctx = &obj;
				ref.func_save_raw = [](void* ctx, const void* src, uint32_t size, serialization_type_id id) {
					auto& s = *reinterpret_cast<TSerializer*>(ctx);
					s.save_raw(src, size, id);
				};
				ref.func_load_raw = [](void* ctx, void* src, uint32_t size, serialization_type_id id) {
					auto& s = *reinterpret_cast<TSerializer*>(ctx);
					s.load_raw(src, size, id);
				};

				if constexpr (detail::has_data_ptr<TSerializer>::value) {
					ref.func_data = [](const void* ctx) {
						const auto& s = *reinterpret_cast<const TSerializer*>(ctx);
						return (const char*)s.data();
					};
				}

				if constexpr (detail::has_reset_fn<TSerializer>::value) {
					ref.func_reset = [](void* ctx) {
						auto& s = *reinterpret_cast<TSerializer*>(ctx);
						s.reset();
					};
				}

				ref.func_tell = [](const void* ctx) {
					const auto& s = *reinterpret_cast<const TSerializer*>(ctx);
					return (uint32_t)s.tell();
				};

				if constexpr (detail::has_bytes_fn<TSerializer>::value) {
					ref.func_bytes = [](const void* ctx) {
						const auto& s = *reinterpret_cast<const TSerializer*>(ctx);
						return (uint32_t)s.bytes();
					};
				}

				ref.func_seek = [](void* ctx, uint32_t pos) {
					auto& s = *reinterpret_cast<TSerializer*>(ctx);
					s.seek(pos);
				};

				return ref;
			}
		};

		//! Backward-compatible alias for older API name.
		using serializer_ref = serializer;

		//! Returns a runtime serializer handle as-is.
		inline serializer make_serializer(serializer s) {
			return s;
		}

		//! Binds an object exposing save_raw/load_raw/tell/seek into a runtime serializer handle.
		template <typename TSerializer>
		inline serializer make_serializer(TSerializer& s) {
			return serializer::bind(s);
		}

		//! Backward-compatible helper aliases.
		inline serializer make_serializer_ref(serializer s) {
			return make_serializer(s);
		}

		template <typename TSerializer>
		inline serializer make_serializer_ref(TSerializer& s) {
			return make_serializer(s);
		}
	} // namespace ser
} // namespace gaia

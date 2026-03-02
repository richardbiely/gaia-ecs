#pragma once
#include "gaia/config/config.h"

#include <type_traits>
#include <utility>

#include "gaia/core/utility.h"
#include "gaia/ser/impl/ser_dispatch.h"
#include "gaia/ser/ser_common.h"

namespace gaia {
	namespace ser {
		using save_raw_fn = void (*)(void*, const void*, uint32_t, serialization_type_id);
		using load_raw_fn = void (*)(void*, void*, uint32_t, serialization_type_id);
		using data_fn = const char* (*)(const void*);
		using reset_fn = void (*)(void*);
		using tell_fn = uint32_t (*)(const void*);
		using bytes_fn = uint32_t (*)(const void*);
		using seek_fn = void (*)(void*, uint32_t);

		//! Opaque runtime serializer context passed around as a single object.
		struct serializer_ctx {
			void* user = nullptr;
			save_raw_fn save_raw = nullptr;
			load_raw_fn load_raw = nullptr;
			data_fn data = nullptr;
			reset_fn reset = nullptr;
			tell_fn tell = nullptr;
			bytes_fn bytes = nullptr;
			seek_fn seek = nullptr;
		};

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
			serializer_ctx m_ctx{};

			serializer() = default;

			explicit serializer(const serializer_ctx& ctx): m_ctx(ctx) {}

			GAIA_NODISCARD bool valid() const {
				return m_ctx.user != nullptr && m_ctx.save_raw != nullptr && m_ctx.load_raw != nullptr &&
							 m_ctx.tell != nullptr && m_ctx.seek != nullptr;
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
				GAIA_ASSERT(m_ctx.save_raw != nullptr);
				m_ctx.save_raw(m_ctx.user, src, size, id);
			}

			void load_raw(void* src, uint32_t size, serialization_type_id id) {
				GAIA_ASSERT(m_ctx.load_raw != nullptr);
				m_ctx.load_raw(m_ctx.user, src, size, id);
			}

			GAIA_NODISCARD const char* data() const {
				if (m_ctx.data == nullptr)
					return nullptr;
				return m_ctx.data(m_ctx.user);
			}

			void reset() {
				if (m_ctx.reset == nullptr)
					return;
				m_ctx.reset(m_ctx.user);
			}

			GAIA_NODISCARD uint32_t tell() const {
				GAIA_ASSERT(m_ctx.tell != nullptr);
				return m_ctx.tell(m_ctx.user);
			}

			GAIA_NODISCARD uint32_t bytes() const {
				if (m_ctx.bytes == nullptr)
					return 0;
				return m_ctx.bytes(m_ctx.user);
			}

			void seek(uint32_t pos) {
				GAIA_ASSERT(m_ctx.seek != nullptr);
				m_ctx.seek(m_ctx.user, pos);
			}

			template <typename TSerializer>
			static serializer_ctx bind_ctx(TSerializer& obj) {
				static_assert(detail::has_save_raw_ptr<TSerializer>::value, "Serializer must expose save_raw(ptr,size,id)");
				static_assert(detail::has_load_raw_ptr<TSerializer>::value, "Serializer must expose load_raw(ptr,size,id)");
				static_assert(detail::has_tell_fn<TSerializer>::value, "Serializer must expose tell()");
				static_assert(detail::has_seek_fn<TSerializer>::value, "Serializer must expose seek(pos)");

				serializer_ctx ctx;
				ctx.user = &obj;
				ctx.save_raw = [](void* ctx, const void* src, uint32_t size, serialization_type_id id) {
					auto& s = *reinterpret_cast<TSerializer*>(ctx);
					s.save_raw(src, size, id);
				};
				ctx.load_raw = [](void* ctx, void* src, uint32_t size, serialization_type_id id) {
					auto& s = *reinterpret_cast<TSerializer*>(ctx);
					s.load_raw(src, size, id);
				};

				if constexpr (detail::has_data_ptr<TSerializer>::value) {
					ctx.data = [](const void* ctx) {
						const auto& s = *reinterpret_cast<const TSerializer*>(ctx);
						return (const char*)s.data();
					};
				}

				if constexpr (detail::has_reset_fn<TSerializer>::value) {
					ctx.reset = [](void* ctx) {
						auto& s = *reinterpret_cast<TSerializer*>(ctx);
						s.reset();
					};
				}

				ctx.tell = [](const void* ctx) {
					const auto& s = *reinterpret_cast<const TSerializer*>(ctx);
					return (uint32_t)s.tell();
				};

				if constexpr (detail::has_bytes_fn<TSerializer>::value) {
					ctx.bytes = [](const void* ctx) {
						const auto& s = *reinterpret_cast<const TSerializer*>(ctx);
						return (uint32_t)s.bytes();
					};
				}

				ctx.seek = [](void* ctx, uint32_t pos) {
					auto& s = *reinterpret_cast<TSerializer*>(ctx);
					s.seek(pos);
				};

				return ctx;
			}

			template <typename TSerializer>
			static serializer bind(TSerializer& obj) {
				return serializer(bind_ctx(obj));
			}
		};

		//! Returns a runtime serializer handle as-is.
		inline serializer make_serializer(serializer s) {
			return s;
		}

		//! Binds an object exposing save_raw/load_raw/tell/seek into a runtime serializer handle.
		template <typename TSerializer>
		inline serializer make_serializer(TSerializer& s) {
			return serializer::bind(s);
		}
	} // namespace ser
} // namespace gaia

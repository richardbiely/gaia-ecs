#pragma once
#include "gaia/config/config.h"

#include <cinttypes>
#include <cstdint>
#include <type_traits>

#include "gaia/ecs/api.h"
#include "gaia/ecs/archetype.h"
#include "gaia/ecs/chunk.h"
#include "gaia/ecs/command_buffer_fwd.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/component_cache_item.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/query_common.h"
#include "gaia/mem/data_layout_policy.h"

namespace gaia {
	namespace ecs {
		class World;

		//! QueryImpl constraints
		enum class Constraints : uint8_t { EnabledOnly, DisabledOnly, AcceptAll };

		namespace detail {
			struct BfsChunkRun {
				const Archetype* pArchetype = nullptr;
				Chunk* pChunk = nullptr;
				uint16_t from = 0;
				uint16_t to = 0;
				uint32_t offset = 0;
			};

			//! Read-only term view for chunk-backed AoS data.
			//! Used by direct iteration paths that can guarantee contiguous component storage.
			template <typename U>
			struct EntityTermViewGetPointer {
				const U* pData = nullptr;
				uint32_t cnt = 0;

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					return pData[idx];
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}

				GAIA_NODISCARD const U* data() const noexcept {
					return pData;
				}
			};

			//! Read-only term view for entity-backed AoS data resolved through the world store.
			//! Used when the queried payload lives out of line and must be fetched by entity id.
			template <typename U>
			struct EntityTermViewGetEntity {
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t cnt = 0;

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					return world_query_entity_arg_by_id<const U&>(*pWorld, pEntities[idx], id);
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}

				GAIA_NODISCARD const U* data() const noexcept {
					return nullptr;
				}
			};

			//! Mutable term view for chunk-backed AoS data.
			//! Exposes direct references to contiguous component storage without any world lookup.
			template <typename U>
			struct EntityTermViewSetPointer {
				U* pData = nullptr;
				uint32_t cnt = 0;

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) {
					GAIA_ASSERT(idx < cnt);
					return pData[idx];
				}

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					return static_cast<const U&>(pData[idx]);
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}

				GAIA_NODISCARD U* data() noexcept {
					return pData;
				}

				GAIA_NODISCARD const U* data() const noexcept {
					return pData;
				}
			};

			//! Mutable term view for entity-backed AoS data resolved through the world store.
			//! Used when writes target out-of-line payloads rather than chunk columns.
			template <typename U>
			struct EntityTermViewSetEntity {
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t cnt = 0;

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) {
					GAIA_ASSERT(idx < cnt);
					return world_query_entity_arg_by_id<U&>(*pWorld, pEntities[idx], id);
				}

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					return world_query_entity_arg_by_id<const U&>(*pWorld, pEntities[idx], id);
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}

				GAIA_NODISCARD U* data() noexcept {
					return nullptr;
				}

				GAIA_NODISCARD const U* data() const noexcept {
					return nullptr;
				}
			};

			//! Read-only AoS term view fallback for APIs where storage mode is known only at runtime.
			//! Direct iteration paths should use EntityTermViewGetPointer/Entity instead.
			template <typename U>
			struct EntityTermViewGet {
				const U* pData = nullptr;
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t cnt = 0;

				static EntityTermViewGet pointer(const U* pData, uint32_t cnt) {
					return {pData, nullptr, nullptr, EntityBad, cnt};
				}

				static EntityTermViewGet entity(const Entity* pEntities, World* pWorld, Entity id, uint32_t cnt) {
					return {nullptr, pEntities, pWorld, id, cnt};
				}

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					if (pData != nullptr)
						return pData[idx];

					return world_query_entity_arg_by_id<const U&>(*pWorld, pEntities[idx], id);
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}

				GAIA_NODISCARD const U* data() const noexcept {
					return pData;
				}
			};

			//! Mutable AoS term view fallback for APIs where storage mode is known only at runtime.
			//! Direct iteration paths should use EntityTermViewSetPointer/Entity instead.
			template <typename U>
			struct EntityTermViewSet {
				U* pData = nullptr;
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t cnt = 0;

				static EntityTermViewSet pointer(U* pData, uint32_t cnt) {
					return {pData, nullptr, nullptr, EntityBad, cnt};
				}

				static EntityTermViewSet entity(const Entity* pEntities, World* pWorld, Entity id, uint32_t cnt) {
					return {nullptr, pEntities, pWorld, id, cnt};
				}

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) {
					GAIA_ASSERT(idx < cnt);
					if (pData != nullptr)
						return pData[idx];

					return world_query_entity_arg_by_id<U&>(*pWorld, pEntities[idx], id);
				}

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					if (pData != nullptr)
						return static_cast<const U&>(pData[idx]);

					return world_query_entity_arg_by_id<const U&>(*pWorld, pEntities[idx], id);
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}

				GAIA_NODISCARD U* data() noexcept {
					return pData;
				}

				GAIA_NODISCARD const U* data() const noexcept {
					return pData;
				}
			};

			//! Writable row proxy for chunk-backed SoA data.
			//! Reads and writes the full structured value directly from SoA storage.
			template <typename U>
			struct SoATermRowWriteProxyPointer {
				using raw_view_policy = mem::data_view_policy_soa<U::gaia_Data_Layout, U>;

				uint8_t* pData = nullptr;
				uint32_t dataSize = 0;
				uint32_t idx = 0;

				GAIA_NODISCARD operator U() const {
					return raw_view_policy::get(std::span<const uint8_t>{pData, dataSize}, idx);
				}

				SoATermRowWriteProxyPointer& operator=(const U& value) {
					raw_view_policy::set(std::span<uint8_t>{pData, dataSize}, idx) = value;
					return *this;
				}
			};

			//! Writable row proxy for entity-backed SoA data.
			//! Reads and writes the full structured value through the world store.
			template <typename U>
			struct SoATermRowWriteProxyEntity {
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t idx = 0;

				GAIA_NODISCARD operator U() const {
					return world_query_entity_arg_by_id<const U&>(*pWorld, pEntities[idx], id);
				}

				SoATermRowWriteProxyEntity& operator=(const U& value) {
					world_query_entity_arg_by_id<U&>(*pWorld, pEntities[idx], id) = value;
					return *this;
				}
			};

			//! Read-only field proxy for a single SoA member in chunk-backed storage.
			//! Exposes a contiguous field span so the hot path can index it directly.
			template <typename U, size_t Item>
			struct SoATermFieldReadProxyPointer {
				using view_policy = mem::data_view_policy_soa_get<U::gaia_Data_Layout, U>;
				using value_type = typename view_policy::template data_view_policy_idx_info<Item>::const_value_type;

				const value_type* pData = nullptr;
				uint32_t cnt = 0;

				GAIA_NODISCARD value_type operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					return pData[idx];
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			//! Read-only field proxy for a single SoA member in entity-backed storage.
			//! Resolves the whole value via the world store and projects the requested field.
			template <typename U, size_t Item>
			struct SoATermFieldReadProxyEntity {
				using view_policy = mem::data_view_policy_soa_get<U::gaia_Data_Layout, U>;
				using value_type = typename view_policy::template data_view_policy_idx_info<Item>::const_value_type;

				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t cnt = 0;

				GAIA_NODISCARD value_type operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					const U value = world_query_entity_arg_by_id<const U&>(*pWorld, pEntities[idx], id);
					return std::get<Item>(meta::struct_to_tuple(value));
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			//! Mutable field proxy for a single SoA member in chunk-backed storage.
			//! Returns raw references so arithmetic updates compile down to plain field writes.
			template <typename U, size_t Item>
			struct SoATermFieldWriteProxyPointer {
				using view_policy = mem::data_view_policy_soa_set<U::gaia_Data_Layout, U>;
				using value_type = typename view_policy::template data_view_policy_idx_info<Item>::value_type;

				value_type* pData = nullptr;
				uint32_t cnt = 0;

				GAIA_NODISCARD value_type& operator[](size_t idx) {
					GAIA_ASSERT(idx < cnt);
					return pData[idx];
				}

				GAIA_NODISCARD const value_type& operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					return pData[idx];
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			//! Mutable field proxy for a single SoA member in entity-backed storage.
			//! Updates reconstruct the whole value, modify one field and store it back.
			template <typename U, size_t Item>
			struct SoATermFieldWriteProxyEntity {
				using view_policy = mem::data_view_policy_soa_set<U::gaia_Data_Layout, U>;
				using value_type = typename view_policy::template data_view_policy_idx_info<Item>::value_type;

				//! Proxy representing one writable field element in entity-backed storage.
				struct ElementProxy {
					const Entity* pEntities = nullptr;
					World* pWorld = nullptr;
					Entity id = EntityBad;
					uint32_t idx = 0;

					GAIA_NODISCARD operator value_type() const {
						const U value = world_query_entity_arg_by_id<const U&>(*pWorld, pEntities[idx], id);
						return std::get<Item>(meta::struct_to_tuple(value));
					}

					ElementProxy& operator=(const value_type& value) {
						auto data = world_query_entity_arg_by_id<const U&>(*pWorld, pEntities[idx], id);
						auto tuple = meta::struct_to_tuple(data);
						std::get<Item>(tuple) = value;
						world_query_entity_arg_by_id<U&>(*pWorld, pEntities[idx], id) = meta::tuple_to_struct<U>(GAIA_MOV(tuple));
						return *this;
					}

					template <typename V>
					ElementProxy& operator+=(V&& value) {
						const value_type current = operator value_type();
						return operator=(current + GAIA_FWD(value));
					}

					template <typename V>
					ElementProxy& operator-=(V&& value) {
						const value_type current = operator value_type();
						return operator=(current - GAIA_FWD(value));
					}

					template <typename V>
					ElementProxy& operator*=(V&& value) {
						const value_type current = operator value_type();
						return operator=(current * GAIA_FWD(value));
					}

					template <typename V>
					ElementProxy& operator/=(V&& value) {
						const value_type current = operator value_type();
						return operator=(current / GAIA_FWD(value));
					}
				};

				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t cnt = 0;

				GAIA_NODISCARD ElementProxy operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					return ElementProxy{pEntities, pWorld, id, (uint32_t)idx};
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			//! Read-only SoA term view for chunk-backed storage only.
			//! Used by direct iteration paths once the term has been resolved to a chunk column.
			template <typename U>
			struct SoATermViewGetPointer {
				using raw_view_policy = mem::data_view_policy_soa<U::gaia_Data_Layout, U>;
				using read_view_policy = mem::data_view_policy_soa_get<U::gaia_Data_Layout, U>;

				const uint8_t* pData = nullptr;
				uint32_t dataSize = 0;
				uint32_t idxBase = 0;
				uint32_t cnt = 0;

				GAIA_NODISCARD U operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					return raw_view_policy::get(std::span<const uint8_t>{pData, dataSize}, idxBase + idx);
				}

				template <size_t Item>
				GAIA_NODISCARD auto get() const {
					const auto field = read_view_policy{std::span<const uint8_t>{pData, dataSize}}.template get<Item>();
					return SoATermFieldReadProxyPointer<U, Item>{field.data() + idxBase, cnt};
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			//! Read-only SoA term view for entity-backed storage only.
			//! Used when the queried SoA payload lives out of line and is resolved by entity id.
			template <typename U>
			struct SoATermViewGetEntity {
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t cnt = 0;

				GAIA_NODISCARD U operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					return world_query_entity_arg_by_id<const U&>(*pWorld, pEntities[idx], id);
				}

				template <size_t Item>
				GAIA_NODISCARD auto get() const {
					return SoATermFieldReadProxyEntity<U, Item>{pEntities, pWorld, id, cnt};
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			//! Mutable SoA term view for chunk-backed storage only.
			//! Provides direct row and field access without carrying any entity fallback state.
			template <typename U>
			struct SoATermViewSetPointer {
				using raw_view_policy = mem::data_view_policy_soa<U::gaia_Data_Layout, U>;
				using read_view_policy = mem::data_view_policy_soa_get<U::gaia_Data_Layout, U>;
				using write_view_policy = mem::data_view_policy_soa_set<U::gaia_Data_Layout, U>;

				uint8_t* pData = nullptr;
				uint32_t dataSize = 0;
				uint32_t idxBase = 0;
				uint32_t cnt = 0;

				GAIA_NODISCARD auto operator[](size_t idx) {
					GAIA_ASSERT(idx < cnt);
					return SoATermRowWriteProxyPointer<U>{pData, dataSize, idxBase + (uint32_t)idx};
				}

				GAIA_NODISCARD U operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					return raw_view_policy::get(std::span<const uint8_t>{pData, dataSize}, idxBase + idx);
				}

				template <size_t Item>
				GAIA_NODISCARD auto get() const {
					const auto field = read_view_policy{std::span<const uint8_t>{pData, dataSize}}.template get<Item>();
					return SoATermFieldReadProxyPointer<U, Item>{field.data() + idxBase, cnt};
				}

				template <size_t Item>
				GAIA_NODISCARD auto set() {
					auto field = write_view_policy{std::span<uint8_t>{pData, dataSize}}.template set<Item>();
					return SoATermFieldWriteProxyPointer<U, Item>{field.data() + idxBase, cnt};
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			//! Mutable SoA term view for entity-backed storage only.
			//! Reads and writes rows and fields through the world store.
			template <typename U>
			struct SoATermViewSetEntity {
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t cnt = 0;

				GAIA_NODISCARD auto operator[](size_t idx) {
					GAIA_ASSERT(idx < cnt);
					return SoATermRowWriteProxyEntity<U>{pEntities, pWorld, id, (uint32_t)idx};
				}

				GAIA_NODISCARD U operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					return world_query_entity_arg_by_id<const U&>(*pWorld, pEntities[idx], id);
				}

				template <size_t Item>
				GAIA_NODISCARD auto get() const {
					return SoATermFieldReadProxyEntity<U, Item>{pEntities, pWorld, id, cnt};
				}

				template <size_t Item>
				GAIA_NODISCARD auto set() {
					return SoATermFieldWriteProxyEntity<U, Item>{pEntities, pWorld, id, cnt};
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			//! Writable SoA row proxy fallback for APIs where storage mode is known only at runtime.
			//! Direct chunk-backed paths should use SoATermRowWriteProxyPointer instead.
			template <typename U>
			struct SoATermRowWriteProxy {
				uint8_t* pData = nullptr;
				uint32_t dataSize = 0;
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t idx = 0;

				GAIA_NODISCARD operator U() const {
					if (pData != nullptr)
						return (U)SoATermRowWriteProxyPointer<U>{pData, dataSize, idx};

					return (U)SoATermRowWriteProxyEntity<U>{pEntities, pWorld, id, idx};
				}

				SoATermRowWriteProxy& operator=(const U& value) {
					if (pData != nullptr) {
						SoATermRowWriteProxyPointer<U>{pData, dataSize, idx} = value;
					} else
						SoATermRowWriteProxyEntity<U>{pEntities, pWorld, id, idx} = value;
					return *this;
				}
			};

			//! Read-only SoA field proxy fallback for APIs where storage mode is known only at runtime.
			//! Direct chunk-backed paths should use SoATermFieldReadProxyPointer instead.
			template <typename U, size_t Item>
			struct SoATermFieldReadProxy {
				using value_type = typename SoATermFieldReadProxyPointer<U, Item>::value_type;

				const value_type* pData = nullptr;
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t cnt = 0;

				GAIA_NODISCARD value_type operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					if (pData != nullptr)
						return SoATermFieldReadProxyPointer<U, Item>{pData, cnt}[idx];

					return SoATermFieldReadProxyEntity<U, Item>{pEntities, pWorld, id, cnt}[idx];
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			//! Mutable SoA field proxy fallback for APIs where storage mode is known only at runtime.
			//! Direct chunk-backed paths should use SoATermFieldWriteProxyPointer instead.
			template <typename U, size_t Item>
			struct SoATermFieldWriteProxy {
				using value_type = typename SoATermFieldWriteProxyPointer<U, Item>::value_type;

				value_type* pData = nullptr;
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t cnt = 0;

				//! Proxy representing a single writable SoA field element in the runtime fallback path only.
				//! Direct chunk-backed paths should use SoATermFieldWriteProxyPointer, which returns raw field references.
				struct ElementProxy {
					value_type* pData = nullptr;
					const Entity* pEntities = nullptr;
					World* pWorld = nullptr;
					Entity id = EntityBad;
					uint32_t idx = 0;

					GAIA_NODISCARD operator value_type() const {
						if (pData != nullptr)
							return SoATermFieldWriteProxyPointer<U, Item>{pData, idx + 1}[idx];

						return SoATermFieldWriteProxyEntity<U, Item>{pEntities, pWorld, id, idx + 1}[idx];
					}

					ElementProxy& operator=(const value_type& value) {
						if (pData != nullptr) {
							SoATermFieldWriteProxyPointer<U, Item>{pData, idx + 1}[idx] = value;
							return *this;
						}

						SoATermFieldWriteProxyEntity<U, Item>{pEntities, pWorld, id, idx + 1}[idx] = value;
						return *this;
					}

					template <typename V>
					ElementProxy& operator+=(V&& value) {
						const value_type current = operator value_type();
						return operator=(current + GAIA_FWD(value));
					}

					template <typename V>
					ElementProxy& operator-=(V&& value) {
						const value_type current = operator value_type();
						return operator=(current - GAIA_FWD(value));
					}

					template <typename V>
					ElementProxy& operator*=(V&& value) {
						const value_type current = operator value_type();
						return operator=(current * GAIA_FWD(value));
					}

					template <typename V>
					ElementProxy& operator/=(V&& value) {
						const value_type current = operator value_type();
						return operator=(current / GAIA_FWD(value));
					}
				};

				GAIA_NODISCARD ElementProxy operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					return ElementProxy{pData, pEntities, pWorld, id, (uint32_t)idx};
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			//! Read-only SoA term view fallback for APIs where storage mode is known only at runtime.
			//! Direct chunk-backed paths should use SoATermViewGetPointer instead.
			template <typename U>
			struct SoATermViewGet {
				const uint8_t* pData = nullptr;
				uint32_t dataSize = 0;
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t idxBase = 0;
				uint32_t cnt = 0;

				GAIA_NODISCARD U operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					if (pData != nullptr)
						return SoATermViewGetPointer<U>{pData, dataSize, idxBase, cnt}[idx];

					return SoATermViewGetEntity<U>{pEntities, pWorld, id, cnt}[idx];
				}

				template <size_t Item>
				GAIA_NODISCARD auto get() const {
					if (pData != nullptr) {
						const auto field = SoATermViewGetPointer<U>{pData, dataSize, idxBase, cnt}.template get<Item>();
						return SoATermFieldReadProxy<U, Item>{field.pData, nullptr, pWorld, id, cnt};
					}

					const auto field = SoATermViewGetEntity<U>{pEntities, pWorld, id, cnt}.template get<Item>();
					return SoATermFieldReadProxy<U, Item>{nullptr, field.pEntities, field.pWorld, field.id, field.cnt};
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			//! Mutable SoA term view fallback for APIs where storage mode is known only at runtime.
			//! Direct chunk-backed paths should use SoATermViewSetPointer instead.
			template <typename U>
			struct SoATermViewSet {
				uint8_t* pData = nullptr;
				uint32_t dataSize = 0;
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t idxBase = 0;
				uint32_t cnt = 0;

				GAIA_NODISCARD auto operator[](size_t idx) {
					GAIA_ASSERT(idx < cnt);
					return SoATermRowWriteProxy<U>{pData, dataSize, pEntities, pWorld, id, idxBase + (uint32_t)idx};
				}

				GAIA_NODISCARD U operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					if (pData != nullptr)
						return SoATermViewSetPointer<U>{pData, dataSize, idxBase, cnt}[idx];

					return SoATermViewSetEntity<U>{pEntities, pWorld, id, cnt}[idx];
				}

				template <size_t Item>
				GAIA_NODISCARD auto get() const {
					if (pData != nullptr) {
						const auto field = SoATermViewSetPointer<U>{pData, dataSize, idxBase, cnt}.template get<Item>();
						return SoATermFieldReadProxy<U, Item>{field.pData, nullptr, pWorld, id, cnt};
					}

					const auto field = SoATermViewSetEntity<U>{pEntities, pWorld, id, cnt}.template get<Item>();
					return SoATermFieldReadProxy<U, Item>{nullptr, field.pEntities, field.pWorld, field.id, field.cnt};
				}

				template <size_t Item>
				GAIA_NODISCARD auto set() {
					if (pData != nullptr) {
						auto field = SoATermViewSetPointer<U>{pData, dataSize, idxBase, cnt}.template set<Item>();
						return SoATermFieldWriteProxy<U, Item>{field.pData, nullptr, pWorld, id, cnt};
					}

					const auto field = SoATermViewSetEntity<U>{pEntities, pWorld, id, cnt}.template set<Item>();
					return SoATermFieldWriteProxy<U, Item>{nullptr, field.pEntities, field.pWorld, field.id, field.cnt};
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			template <Constraints IterConstraint>
			class ChunkIterImpl {
			protected:
				using CompIndicesBitView = core::bit_view<ChunkHeader::MAX_COMPONENTS_BITS>;

				//! World pointer
				const World* m_pWorld = nullptr;
				//! Currently iterated archetype
				const Archetype* m_pArchetype = nullptr;
				//! Chunk currently associated with the iterator
				Chunk* m_pChunk = nullptr;
				//! ChunkHeader::MAX_COMPONENTS values for component indices mapping for the parent archetype
				const uint8_t* m_pCompIdxMapping = nullptr;
				//! Optional per-term ids used by one-row direct iterators when a term resolves semantically.
				const Entity* m_pTermIdMapping = nullptr;
				//! Row of the first entity we iterate from
				uint16_t m_from;
				//! Row of the last entity we iterate to
				uint16_t m_to;
				//! GroupId. 0 if not set.
				GroupId m_groupId = 0;

			public:
				ChunkIterImpl() = default;
				~ChunkIterImpl() = default;
				ChunkIterImpl(ChunkIterImpl&&) noexcept = default;
				ChunkIterImpl& operator=(ChunkIterImpl&&) noexcept = default;
				ChunkIterImpl(const ChunkIterImpl&) = delete;
				ChunkIterImpl& operator=(const ChunkIterImpl&) = delete;

				void set_world(const World* pWorld) {
					GAIA_ASSERT(pWorld != nullptr);
					m_pWorld = pWorld;
				}

				GAIA_NODISCARD World* world() {
					GAIA_ASSERT(m_pWorld != nullptr);
					return const_cast<World*>(m_pWorld);
				}

				GAIA_NODISCARD const World* world() const {
					GAIA_ASSERT(m_pWorld != nullptr);
					return m_pWorld;
				}

				void set_archetype(const Archetype* pArchetype) {
					GAIA_ASSERT(pArchetype != nullptr);
					m_pArchetype = pArchetype;
				}

				// GAIA_NODISCARD Archetype* archetype() {
				// 	GAIA_ASSERT(m_pArchetype != nullptr);
				// 	return m_pArchetype;
				// }

				GAIA_NODISCARD const Archetype* archetype() const {
					GAIA_ASSERT(m_pArchetype != nullptr);
					return m_pArchetype;
				}

				void set_chunk(Chunk* pChunk) {
					GAIA_ASSERT(pChunk != nullptr);
					m_pChunk = pChunk;

					if constexpr (IterConstraint == Constraints::EnabledOnly)
						m_from = m_pChunk->size_disabled();
					else
						m_from = 0;

					if constexpr (IterConstraint == Constraints::DisabledOnly)
						m_to = m_pChunk->size_disabled();
					else
						m_to = m_pChunk->size();
				}

				void set_chunk(Chunk* pChunk, uint16_t from, uint16_t to) {
					if (from == 0 && to == 0) {
						set_chunk(pChunk);
						return;
					}

					GAIA_ASSERT(pChunk != nullptr);
					m_pChunk = pChunk;
					m_from = from;
					m_to = to;
				}

				GAIA_NODISCARD const Chunk* chunk() const {
					GAIA_ASSERT(m_pChunk != nullptr);
					return m_pChunk;
				}

				void set_remapping_indices(const uint8_t* pCompIndicesMapping) {
					m_pCompIdxMapping = pCompIndicesMapping;
				}

				void set_term_ids(const Entity* pTermIds) {
					m_pTermIdMapping = pTermIds;
				}

				void set_group_id(GroupId groupId) {
					m_groupId = groupId;
				}

				GAIA_NODISCARD GroupId group_id() const {
					return m_groupId;
				}

				GAIA_NODISCARD CommandBufferST& cmd_buffer_st() const {
					auto* pWorld = const_cast<World*>(m_pWorld);
					return cmd_buffer_st_get(*pWorld);
				}

				GAIA_NODISCARD CommandBufferMT& cmd_buffer_mt() const {
					auto* pWorld = const_cast<World*>(m_pWorld);
					return cmd_buffer_mt_get(*pWorld);
				}

				template <typename T>
				GAIA_NODISCARD bool uses_out_of_line_component() const {
					using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
					if constexpr (std::is_same_v<Arg, Entity>)
						return false;
					else {
						using FT = typename component_type_t<Arg>::TypeFull;
						if constexpr (is_pair<FT>::value || mem::is_soa_layout_v<Arg>)
							return false;
						else {
							const auto* pItem = comp_cache(*world()).template find<FT>();
							// Type-based iteration switches to the world/store path as soon as the
							// payload stops being chunk-backed, even if the id still fragments.
							return pItem != nullptr && world_is_out_of_line_component(*world(), pItem->entity);
						}
					}
				}

				//! Returns a read-only entity or component view that can resolve non-direct storage.
				//! This is the fallback accessor for terms that may come from outside the chunk column,
				//! such as inherited, sparse, out-of-line, or other entity-backed storage.
				//! \warning If @a T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity of component view with read-only access
				template <typename T>
				GAIA_NODISCARD auto view_any() const {
					using U = typename actual_type_t<T>::Type;
					if constexpr (std::is_same_v<U, Entity> || mem::is_soa_layout_v<U>)
						return m_pChunk->view<T>(from(), to());
					else {
						Entity id = EntityBad;
						if (uses_out_of_line_component<T>()) {
							using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
							using FT = typename component_type_t<Arg>::TypeFull;
							id = comp_cache(*world()).template get<FT>().entity;
						}

						if (id != EntityBad)
							return EntityTermViewGet<U>::entity(
									m_pChunk->entity_view().data() + from(), const_cast<World*>(world()), id, size());

						auto* pData = reinterpret_cast<const U*>(m_pChunk->template view<T>(from(), to()).data());
						return EntityTermViewGet<U>::pointer(pData, size());
					}
				}

				//! Returns a read-only entity or component view for a remapped term index that can resolve non-direct storage.
				//! Use this when the term may resolve to inherited, sparse, out-of-line, or other entity-backed storage
				//! instead of a dense chunk column.
				//! \warning It is expected the term index maps to a valid query term for @a T.
				//! \tparam T Component or Entity
				//! \param termIdx Query term index
				//! \return Entity or component view with read-only access
				template <typename T>
				GAIA_NODISCARD auto view_any(uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;

					if constexpr (mem::is_soa_layout_v<U>) {
						const auto compIdx = m_pCompIdxMapping[termIdx];
						if (compIdx == 0xFF) {
							GAIA_ASSERT(m_pTermIdMapping != nullptr);
							GAIA_ASSERT(size() == 1);

							const auto* pEntities = m_pChunk->entity_view().data() + from();
							const auto id = m_pTermIdMapping[termIdx];
							return SoATermViewGet<U>{nullptr, 0, pEntities, world(), id, 0, 1};
						}

						GAIA_ASSERT(compIdx < m_pChunk->ids_view().size());
						return SoATermViewGet<U>{
								m_pChunk->comp_ptr(compIdx), m_pChunk->capacity(), nullptr, world(), EntityBad, from(), size()};
					} else {
						const auto compIdx = m_pCompIdxMapping[termIdx];
						const auto id = m_pTermIdMapping != nullptr ? m_pTermIdMapping[termIdx] : EntityBad;
						if (id != EntityBad && world_is_out_of_line_component(*world(), id))
							return EntityTermViewGet<U>::entity(
									m_pChunk->entity_view().data() + from(), const_cast<World*>(world()), id, size());

						if (compIdx == 0xFF) {
							GAIA_ASSERT(m_pTermIdMapping != nullptr);
							GAIA_ASSERT(size() == 1);

							const auto entity = m_pChunk->entity_view()[from()];
							const auto id = m_pTermIdMapping[termIdx];
							const auto& data = world_query_entity_arg_by_id<const U&>(*world(), entity, id);
							return EntityTermViewGet<U>::pointer(&data, 1);
						}
						GAIA_ASSERT(compIdx < m_pChunk->ids_view().size());

						auto* pData = reinterpret_cast<const U*>(m_pChunk->comp_ptr_mut(compIdx, from()));
						return EntityTermViewGet<U>::pointer(pData, size());
					}
				}

				//! Returns a read-only entity or component view for the owned chunk-backed fast path.
				//! This assumes the resolved term is stored directly in the current chunk range and therefore
				//! skips all non-direct dispatch.
				//! Use view_any() when the term may resolve to inherited, sparse, out-of-line, or otherwise
				//! entity-backed storage.
				//! \warning If @a T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Direct read-only entity or component view
				template <typename T>
				GAIA_NODISCARD auto view() const {
					using U = typename actual_type_t<T>::Type;
					if constexpr (std::is_same_v<U, Entity>)
						return m_pChunk->view<T>(from(), to());
					else if constexpr (mem::is_soa_layout_v<U>) {
						const auto view = m_pChunk->view<T>(from(), to());
						return SoATermViewGetPointer<U>{(const uint8_t*)view.data(), (uint32_t)view.size(), from(), size()};
					} else {
						auto* pData = reinterpret_cast<const U*>(m_pChunk->template view<T>(from(), to()).data());
						return EntityTermViewGetPointer<U>{pData, size()};
					}
				}

				//! Returns a read-only entity or component view for a remapped owned chunk-backed term.
				//! The caller is responsible for passing a term index that maps to a dense chunk column.
				//! Use view_any(termIdx) when the term may resolve to inherited, sparse, out-of-line, or other
				//! non-direct storage.
				//! \warning Passing a non-mapped or entity-backed term index is invalid.
				//! \tparam T Component or Entity
				//! \param termIdx Query term index
				//! \return Direct read-only entity or component view
				template <typename T>
				GAIA_NODISCARD auto view(uint32_t termIdx) const {
					using U = typename actual_type_t<T>::Type;
					const auto compIdx = m_pCompIdxMapping[termIdx];
					GAIA_ASSERT(compIdx != 0xFF);

					if constexpr (mem::is_soa_layout_v<U>)
						return SoATermViewGetPointer<U>{m_pChunk->comp_ptr(compIdx), m_pChunk->capacity(), from(), size()};
					else {
						auto* pData = reinterpret_cast<const U*>(m_pChunk->comp_ptr(compIdx, from()));
						return EntityTermViewGetPointer<U>{pData, size()};
					}
				}

				//! Returns a mutable entity or component view that can resolve non-direct storage.
				//! This is the fallback accessor for inherited, sparse, out-of-line, or other entity-backed
				//! terms that are not guaranteed to be backed by the current chunk column.
				//! \warning If @a T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto view_mut_any() {
					using U = typename actual_type_t<T>::Type;
					static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");
					if constexpr (mem::is_soa_layout_v<U>)
						return m_pChunk->view_mut<T>(from(), to());
					else {
						Entity id = EntityBad;
						if (uses_out_of_line_component<T>()) {
							using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
							using FT = typename component_type_t<Arg>::TypeFull;
							id = comp_cache(*world()).template get<FT>().entity;
						}

						if (id != EntityBad)
							return EntityTermViewSet<U>::entity(m_pChunk->entity_view().data() + from(), world(), id, size());

						auto* pData = reinterpret_cast<U*>(m_pChunk->template view_mut<T>(from(), to()).data());
						return EntityTermViewSet<U>::pointer(pData, size());
					}
				}

				//! Returns a mutable entity or component view for the owned chunk-backed fast path.
				//! Updates world versioning through the underlying chunk view and skips all non-direct dispatch.
				//! Use view_mut_any() when the term may resolve to inherited, sparse, out-of-line, or other
				//! entity-backed storage.
				//! \tparam T Component or Entity
				//! \return Direct entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto view_mut() {
					using U = typename actual_type_t<T>::Type;
					static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");
					if constexpr (mem::is_soa_layout_v<U>) {
						auto view = m_pChunk->view_mut<T>(from(), to());
						return SoATermViewSetPointer<U>{(uint8_t*)view.data(), (uint32_t)view.size(), from(), size()};
					} else {
						auto* pData = reinterpret_cast<U*>(m_pChunk->template view_mut<T>(from(), to()).data());
						return EntityTermViewSetPointer<U>{pData, size()};
					}
				}

				//! Returns a mutable entity or component view for a remapped owned chunk-backed term.
				//! Updates world versioning for the selected chunk column before handing out mutable access.
				//! Use view_mut_any(termIdx) when the term may resolve to inherited, sparse, out-of-line, or
				//! other non-direct storage.
				//! \warning Passing a non-mapped or entity-backed term index is invalid.
				//! \tparam T Component or Entity
				//! \param termIdx Query term index
				//! \return Direct entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto view_mut(uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;
					static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via view_mut is forbidden");
					const auto compIdx = m_pCompIdxMapping[termIdx];
					GAIA_ASSERT(compIdx != 0xFF);

					if constexpr (mem::is_soa_layout_v<U>) {
						m_pChunk->update_world_version(compIdx);
						return SoATermViewSetPointer<U>{m_pChunk->comp_ptr_mut(compIdx), m_pChunk->capacity(), from(), size()};
					} else {
						m_pChunk->update_world_version(compIdx);
						auto* pData = reinterpret_cast<U*>(m_pChunk->comp_ptr_mut(compIdx, from()));
						return EntityTermViewSetPointer<U>{pData, size()};
					}
				}

				//! Returns a mutable entity or component view for a remapped term index that can resolve non-direct storage.
				//! Use this when the term may resolve to inherited, sparse, out-of-line, or other entity-backed
				//! storage instead of a dense chunk column.
				//! Updates world versioning for chunk-backed terms before handing out mutable access.
				//! \warning It is expected the term index maps to a valid query term for @a T.
				//! \tparam T Component or Entity
				//! \param termIdx Query term index
				//! \return Entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto view_mut_any(uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;

					if constexpr (mem::is_soa_layout_v<U>) {
						const auto compIdx = m_pCompIdxMapping[termIdx];
						if (compIdx == 0xFF) {
							GAIA_ASSERT(m_pTermIdMapping != nullptr);
							GAIA_ASSERT(size() == 1);

							const auto entity = m_pChunk->entity_view()[from()];
							const auto id = m_pTermIdMapping[termIdx];
							(void)world_query_entity_arg_by_id<U&>(*world(), entity, id);
							const auto& ec = ::gaia::ecs::fetch(*world(), entity);
							const auto* pEntities = ec.pChunk->entity_view().data() + ec.row;
							return SoATermViewSet<U>{nullptr, 0, pEntities, world(), id, 0, 1};
						}

						GAIA_ASSERT(compIdx < m_pChunk->comp_rec_view().size());
						m_pChunk->update_world_version(compIdx);
						return SoATermViewSet<U>{
								m_pChunk->comp_ptr_mut(compIdx), m_pChunk->capacity(), nullptr, world(), EntityBad, from(), size()};
					} else {
						const auto compIdx = m_pCompIdxMapping[termIdx];
						const auto id = m_pTermIdMapping != nullptr ? m_pTermIdMapping[termIdx] : EntityBad;
						if (id != EntityBad && world_is_out_of_line_component(*world(), id))
							return EntityTermViewSet<U>::entity(m_pChunk->entity_view().data() + from(), world(), id, size());

						if (compIdx == 0xFF) {
							GAIA_ASSERT(m_pTermIdMapping != nullptr);
							GAIA_ASSERT(size() == 1);

							const auto entity = m_pChunk->entity_view()[from()];
							const auto id = m_pTermIdMapping[termIdx];
							auto& data = world_query_entity_arg_by_id<U&>(*world(), entity, id);
							return EntityTermViewSet<U>::pointer(&data, 1);
						}
						GAIA_ASSERT(compIdx < m_pChunk->comp_rec_view().size());

						m_pChunk->update_world_version(compIdx);

						auto* pData = reinterpret_cast<U*>(m_pChunk->comp_ptr_mut(compIdx, from()));
						return EntityTermViewSet<U>::pointer(pData, size());
					}
				}

				//! Returns a mutable component view that can resolve non-direct storage.
				//! This is the fallback accessor for inherited, sparse, out-of-line, or other entity-backed
				//! terms that are not guaranteed to be backed by the current chunk column.
				//! Doesn't update the world version when the access is acquired.
				//! \warning It is expected the component @a T is present. Undefined behavior otherwise.
				//! \tparam T Component
				//! \return Component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto sview_mut_any() {
					using U = typename actual_type_t<T>::Type;
					static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via sview_mut is forbidden");
					if constexpr (mem::is_soa_layout_v<U>)
						return m_pChunk->sview_mut<T>(from(), to());
					else {
						Entity id = EntityBad;
						if (uses_out_of_line_component<T>()) {
							using Arg = std::remove_cv_t<std::remove_reference_t<T>>;
							using FT = typename component_type_t<Arg>::TypeFull;
							id = comp_cache(*world()).template get<FT>().entity;
						}

						if (id != EntityBad)
							return EntityTermViewSet<U>::entity(m_pChunk->entity_view().data() + from(), world(), id, size());

						auto* pData = reinterpret_cast<U*>(m_pChunk->template sview_mut<T>(from(), to()).data());
						return EntityTermViewSet<U>::pointer(pData, size());
					}
				}

				//! Returns a mutable component view for the owned chunk-backed fast path.
				//! Doesn't update the world version when the access is acquired and skips all non-direct dispatch.
				//! Use sview_mut_any() when the term may resolve to inherited, sparse, out-of-line, or other
				//! entity-backed storage.
				//! \tparam T Component
				//! \return Direct component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto sview_mut() {
					using U = typename actual_type_t<T>::Type;
					static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via sview_mut is forbidden");
					if constexpr (mem::is_soa_layout_v<U>) {
						auto view = m_pChunk->sview_mut<T>(from(), to());
						return SoATermViewSetPointer<U>{(uint8_t*)view.data(), (uint32_t)view.size(), from(), size()};
					} else {
						auto* pData = reinterpret_cast<U*>(m_pChunk->template sview_mut<T>(from(), to()).data());
						return EntityTermViewSetPointer<U>{pData, size()};
					}
				}

				//! Returns a mutable component view for a remapped owned chunk-backed term.
				//! Doesn't update the world version when the access is acquired.
				//! Use sview_mut_any(termIdx) when the term may resolve to inherited, sparse, out-of-line, or
				//! other non-direct storage.
				//! \warning Passing a non-mapped or entity-backed term index is invalid.
				//! \tparam T Component
				//! \param termIdx Query term index
				//! \return Direct component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto sview_mut(uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;
					static_assert(!std::is_same_v<U, Entity>, "Modifying chunk entities via sview_mut is forbidden");
					const auto compIdx = m_pCompIdxMapping[termIdx];
					GAIA_ASSERT(compIdx != 0xFF);

					if constexpr (mem::is_soa_layout_v<U>)
						return SoATermViewSetPointer<U>{m_pChunk->comp_ptr_mut(compIdx), m_pChunk->capacity(), from(), size()};
					else {
						auto* pData = reinterpret_cast<U*>(m_pChunk->comp_ptr_mut(compIdx, from()));
						return EntityTermViewSetPointer<U>{pData, size()};
					}
				}

				//! Returns a mutable component view for a remapped term index that can resolve non-direct storage.
				//! Use this when the term may resolve to inherited, sparse, out-of-line, or other entity-backed
				//! storage instead of a dense chunk column.
				//! Doesn't update the world version when the access is acquired.
				//! \warning It is expected the term index maps to a valid query term for @a T.
				//! \tparam T Component
				//! \param termIdx Query term index
				//! \return Component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto sview_mut_any(uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;

					if constexpr (mem::is_soa_layout_v<U>) {
						const auto compIdx = m_pCompIdxMapping[termIdx];
						if (compIdx == 0xFF) {
							GAIA_ASSERT(m_pTermIdMapping != nullptr);
							GAIA_ASSERT(size() == 1);

							const auto entity = m_pChunk->entity_view()[from()];
							const auto id = m_pTermIdMapping[termIdx];
							(void)world_query_entity_arg_by_id<U&>(*world(), entity, id);
							const auto& ec = ::gaia::ecs::fetch(*world(), entity);
							const auto* pEntities = ec.pChunk->entity_view().data() + ec.row;
							return SoATermViewSet<U>{nullptr, 0, pEntities, world(), id, 0, 1};
						}

						GAIA_ASSERT(compIdx < m_pChunk->ids_view().size());
						return SoATermViewSet<U>{
								m_pChunk->comp_ptr_mut(compIdx), m_pChunk->capacity(), nullptr, world(), EntityBad, from(), size()};
					} else {
						const auto compIdx = m_pCompIdxMapping[termIdx];
						const auto id = m_pTermIdMapping != nullptr ? m_pTermIdMapping[termIdx] : EntityBad;
						if (id != EntityBad && world_is_out_of_line_component(*world(), id))
							return EntityTermViewSet<U>::entity(m_pChunk->entity_view().data() + from(), world(), id, size());

						if (compIdx == 0xFF) {
							GAIA_ASSERT(m_pTermIdMapping != nullptr);
							GAIA_ASSERT(size() == 1);

							const auto entity = m_pChunk->entity_view()[from()];
							const auto id = m_pTermIdMapping[termIdx];
							auto& data = world_query_entity_arg_by_id<U&>(*world(), entity, id);
							return EntityTermViewSet<U>::pointer(&data, 1);
						}
						GAIA_ASSERT(compIdx < m_pChunk->ids_view().size());

						auto* pData = reinterpret_cast<U*>(m_pChunk->comp_ptr_mut(compIdx, from()));
						return EntityTermViewSet<U>::pointer(pData, size());
					}
				}

				//! Marks the component @a T as modified. Best used with sview to manually trigger
				//! an update at user's whim.
				//! If \tparam TriggerHooks is true, also triggers the component's set hooks.
				template <typename T, bool TriggerHooks>
				void modify() {
					m_pChunk->modify<T, TriggerHooks>();
				}

				//! Returns either a mutable or immutable entity/component view for the owned chunk-backed fast path.
				//! Value and const types are treated as immutable. Mutable references select the mutable path.
				//! Use view_auto_any() when the term may resolve to inherited, sparse, out-of-line, or other
				//! non-direct storage.
				//! \warning If @a T is a component it is expected to be present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Direct entity or component view
				template <typename T>
				GAIA_NODISCARD auto view_auto() {
					using UOriginal = typename actual_type_t<T>::TypeOriginal;
					if constexpr (core::is_mut_v<UOriginal>)
						return view_mut<T>();
					else
						return view<T>();
				}

				//! Returns either a mutable or immutable entity/component view that can resolve non-direct storage.
				//! Value and const types are considered immutable. Anything else is mutable.
				//! Use this when the term may resolve to inherited, sparse, out-of-line, or other entity-backed storage.
				//! \warning If @a T is a component it is expected to be present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view
				template <typename T>
				GAIA_NODISCARD auto view_auto_any() {
					using UOriginal = typename actual_type_t<T>::TypeOriginal;
					if constexpr (core::is_mut_v<UOriginal>)
						return view_mut_any<T>();
					else
						return view_any<T>();
				}

				//! Returns either a mutable or immutable entity/component view that can resolve non-direct storage.
				//! Value and const types are considered immutable. Anything else is mutable.
				//! Use this when the term may resolve to inherited, sparse, out-of-line, or other entity-backed storage.
				//! Doesn't update the world version when read-write access is acquired.
				//! \warning If @a T is a component it is expected to be present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view
				template <typename T>
				GAIA_NODISCARD auto sview_auto_any() {
					using UOriginal = typename actual_type_t<T>::TypeOriginal;
					if constexpr (core::is_mut_v<UOriginal>)
						return sview_mut_any<T>();
					else
						return view_any<T>();
				}

				//! Returns either a mutable or immutable entity/component view for the owned chunk-backed fast path.
				//! Value and const types are treated as immutable. Mutable references select the mutable path.
				//! Doesn't update the world version when read-write access is acquired.
				//! Use sview_auto_any() when the term may resolve to inherited, sparse, out-of-line, or other
				//! non-direct storage.
				//! \warning If @a T is a component it is expected to be present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Direct entity or component view
				template <typename T>
				GAIA_NODISCARD auto sview_auto() {
					using UOriginal = typename actual_type_t<T>::TypeOriginal;
					if constexpr (core::is_mut_v<UOriginal>)
						return sview_mut<T>();
					else
						return view<T>();
				}

				//! Checks if the entity at the current iterator index is enabled.
				//! \return True it the entity is enabled. False otherwise.
				GAIA_NODISCARD bool enabled(uint32_t index) const {
					const auto row = (uint16_t)(from() + index);
					return m_pChunk->enabled(row);
				}

				//! Checks if entity @a entity is present in the chunk.
				//! \param entity Entity
				//! \return True if the component is present. False otherwise.
				GAIA_NODISCARD bool has(Entity entity) const {
					return m_pChunk->has(entity);
				}

				//! Checks if relationship pair @a pair is present in the chunk.
				//! \param pair Relationship pair
				//! \return True if the component is present. False otherwise.
				GAIA_NODISCARD bool has(Pair pair) const {
					return m_pChunk->has((Entity)pair);
				}

				//! Checks if component @a T is present in the chunk.
				//! \tparam T Component
				//! \return True if the component is present. False otherwise.
				template <typename T>
				GAIA_NODISCARD bool has() const {
					return m_pChunk->has<T>();
				}

				GAIA_NODISCARD static uint16_t start_index(Chunk* pChunk) noexcept {
					if constexpr (IterConstraint == Constraints::EnabledOnly)
						return pChunk->size_disabled();
					else
						return 0;
				}

				GAIA_NODISCARD static uint16_t end_index(Chunk* pChunk) noexcept {
					if constexpr (IterConstraint == Constraints::DisabledOnly)
						return pChunk->size_enabled();
					else
						return pChunk->size();
				}

				GAIA_NODISCARD static uint16_t size(Chunk* pChunk) noexcept {
					if constexpr (IterConstraint == Constraints::EnabledOnly)
						return pChunk->size_enabled();
					else if constexpr (IterConstraint == Constraints::DisabledOnly)
						return pChunk->size_disabled();
					else
						return pChunk->size();
				}

				//! Returns the number of entities accessible via the iterator
				GAIA_NODISCARD uint16_t size() const noexcept {
					return (uint16_t)(to() - from());
				}

				//! Returns the absolute index that should be used to access an item in the chunk.
				//! AoS indices map directly, SoA indices need some adjustments because the view is
				//! always considered {0..ChunkCapacity} instead of {FirstEnabled..ChunkSize}.
				template <typename T>
				uint32_t acc_index(uint32_t idx) const noexcept {
					using U = typename actual_type_t<T>::Type;

					if constexpr (mem::is_soa_layout_v<U>)
						return idx + from();
					else
						return idx;
				}

				//! Returns the local row index for direct iterator views.
				//! Direct views already encode the iterator row base, so no additional adjustment is needed.
				template <typename T>
				uint32_t acc_index_direct(uint32_t idx) const noexcept {
					(void)sizeof(typename actual_type_t<T>::Type);
					return idx;
				}

			protected:
				//! Returns the starting index of the iterator
				GAIA_NODISCARD uint16_t from() const noexcept {
					return m_from;
				}

				//! Returns the ending index of the iterator (one past the last valid index)
				GAIA_NODISCARD uint16_t to() const noexcept {
					return m_to;
				}
			};
		} // namespace detail

		//! Iterator for iterating enabled entities
		class GAIA_API Iter final: public detail::ChunkIterImpl<Constraints::EnabledOnly> {};
		//! Iterator for iterating disabled entities
		class GAIA_API IterDisabled final: public detail::ChunkIterImpl<Constraints::DisabledOnly> {};

		//! Iterator for iterating both enabled and disabled entities.
		//! Disabled entities always precede enabled ones.
		class GAIA_API IterAll final: public detail::ChunkIterImpl<Constraints::AcceptAll> {
		public:
			//! Returns the number of enabled entities accessible via the iterator.
			GAIA_NODISCARD uint16_t size_enabled() const noexcept {
				return m_pChunk->size_enabled();
			}

			//! Returns the number of disabled entities accessible via the iterator.
			//! Can be read also as "the index of the first enabled entity".
			GAIA_NODISCARD uint16_t size_disabled() const noexcept {
				return m_pChunk->size_disabled();
			}
		};

		//! Iterator used when copying entities.
		class GAIA_API CopyIter final {
		protected:
			using CompIndicesBitView = core::bit_view<ChunkHeader::MAX_COMPONENTS_BITS>;

			//! World pointer
			const World* m_pWorld = nullptr;
			//! Currently iterated archetype
			const Archetype* m_pArchetype = nullptr;
			//! Chunk currently associated with the iterator
			Chunk* m_pChunk = nullptr;
			//! Row of the first entity we iterate from
			uint16_t m_from;
			//! The number of entities accessible via the iterator
			uint16_t m_cnt;

		public:
			CopyIter() = default;
			~CopyIter() = default;
			CopyIter(CopyIter&&) noexcept = default;
			CopyIter& operator=(CopyIter&&) noexcept = default;
			CopyIter(const CopyIter&) = delete;
			CopyIter& operator=(const CopyIter&) = delete;

			//! Sets the iterator's range.
			//! \param from Row of the first entity we want to iterate from
			//! \param cnt Number of entities we are going to iterate
			void set_range(uint16_t from, uint16_t cnt) {
				GAIA_ASSERT(from < m_pChunk->size());
				GAIA_ASSERT(from + cnt <= m_pChunk->size());
				m_from = from;
				m_cnt = cnt;
			}

			void set_world(const World* pWorld) {
				GAIA_ASSERT(pWorld != nullptr);
				m_pWorld = pWorld;
			}

			GAIA_NODISCARD World* world() {
				GAIA_ASSERT(m_pWorld != nullptr);
				return const_cast<World*>(m_pWorld);
			}

			GAIA_NODISCARD const World* world() const {
				GAIA_ASSERT(m_pWorld != nullptr);
				return m_pWorld;
			}

			void set_archetype(const Archetype* pArchetype) {
				GAIA_ASSERT(pArchetype != nullptr);
				m_pArchetype = pArchetype;
			}

			// GAIA_NODISCARD Archetype* archetype() {
			// 	GAIA_ASSERT(m_pArchetype != nullptr);
			// 	return m_pArchetype;
			// }

			GAIA_NODISCARD const Archetype* archetype() const {
				GAIA_ASSERT(m_pArchetype != nullptr);
				return m_pArchetype;
			}

			void set_chunk(Chunk* pChunk) {
				GAIA_ASSERT(pChunk != nullptr);
				m_pChunk = pChunk;
			}

			GAIA_NODISCARD const Chunk* chunk() const {
				GAIA_ASSERT(m_pChunk != nullptr);
				return m_pChunk;
			}

			GAIA_NODISCARD CommandBufferST& cmd_buffer_st() const {
				auto* pWorld = const_cast<World*>(m_pWorld);
				return cmd_buffer_st_get(*pWorld);
			}

			GAIA_NODISCARD CommandBufferMT& cmd_buffer_mt() const {
				auto* pWorld = const_cast<World*>(m_pWorld);
				return cmd_buffer_mt_get(*pWorld);
			}

			//! Returns a read-only entity or component view.
			//! \warning If @a T is a component it is expected it is present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \return Entity of component view with read-only access
			template <typename T>
			GAIA_NODISCARD auto view() const {
				return m_pChunk->view<T>(from(), to());
			}

			//! Returns a mutable entity or component view.
			//! \warning If @a T is a component it is expected it is present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \return Entity or component view with read-write access
			template <typename T>
			GAIA_NODISCARD auto view_mut() {
				return m_pChunk->view_mut<T>(from(), to());
			}

			//! Returns a mutable component view.
			//! Doesn't update the world version when the access is acquired.
			//! \warning It is expected the component @a T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \return Component view with read-write access
			template <typename T>
			GAIA_NODISCARD auto sview_mut() {
				return m_pChunk->sview_mut<T>(from(), to());
			}

			//! Marks the component @a T as modified. Best used with sview to manually trigger
			//! an update at user's whim.
			//! If \tparam TriggerHooks is true, also triggers the component's set hooks.
			template <typename T, bool TriggerHooks>
			void modify() {
				m_pChunk->modify<T, TriggerHooks>();
			}

			//! Returns either a mutable or immutable entity/component view based on the requested type.
			//! Value and const types are considered immutable. Anything else is mutable.
			//! \warning If @a T is a component it is expected to be present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \return Entity or component view
			template <typename T>
			GAIA_NODISCARD auto view_auto() {
				return m_pChunk->view_auto<T>(from(), to());
			}

			//! Returns either a mutable or immutable entity/component view based on the requested type.
			//! Value and const types are considered immutable. Anything else is mutable.
			//! Doesn't update the world version when read-write access is acquired.
			//! \warning If @a T is a component it is expected to be present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \return Entity or component view
			template <typename T>
			GAIA_NODISCARD auto sview_auto() {
				return m_pChunk->sview_auto<T>(from(), to());
			}

			//! Checks if the entity at the current iterator index is enabled.
			//! \return True it the entity is enabled. False otherwise.
			GAIA_NODISCARD bool enabled(uint32_t index) const {
				const auto row = (uint16_t)(from() + index);
				return m_pChunk->enabled(row);
			}

			//! Checks if entity @a entity is present in the chunk.
			//! \param entity Entity
			//! \return True if the component is present. False otherwise.
			GAIA_NODISCARD bool has(Entity entity) const {
				return m_pChunk->has(entity);
			}

			//! Checks if relationship pair @a pair is present in the chunk.
			//! \param pair Relationship pair
			//! \return True if the component is present. False otherwise.
			GAIA_NODISCARD bool has(Pair pair) const {
				return m_pChunk->has((Entity)pair);
			}

			//! Checks if component @a T is present in the chunk.
			//! \tparam T Component
			//! \return True if the component is present. False otherwise.
			template <typename T>
			GAIA_NODISCARD bool has() const {
				return m_pChunk->has<T>();
			}

			//! Returns the number of entities accessible via the iterator
			GAIA_NODISCARD uint16_t size() const noexcept {
				return m_cnt;
			}

		private:
			//! Returns the starting index of the iterator
			GAIA_NODISCARD uint16_t from() const noexcept {
				return m_from;
			}

			//! Returns the ending index of the iterator (one past the last valid index)
			GAIA_NODISCARD uint16_t to() const noexcept {
				return m_from + m_cnt;
			}
		};
	} // namespace ecs
} // namespace gaia

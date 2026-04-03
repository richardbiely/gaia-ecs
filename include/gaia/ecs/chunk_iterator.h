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
		void world_finish_write(World& world, Entity term, Entity entity);
		template <typename T>
		decltype(auto) world_query_entity_arg_by_id_raw(World& world, Entity entity, Entity id);
		template <typename T>
		decltype(auto) world_query_entity_arg_by_id_cached_const(
				World& world, Entity entity, Entity id, const Archetype*& pLastArchetype, Entity& cachedOwner,
				bool& cachedDirect);
		template <typename T>
		void world_init_query_entity_arg_by_id_chunk_stable_const(
				World& world, const Chunk& chunk, const Entity* pEntities, Entity id, bool& direct, uint32_t& compIdx,
				const std::remove_cv_t<std::remove_reference_t<T>>*& pDataInherited);
		template <typename T>
		const std::remove_cv_t<std::remove_reference_t<T>>*
		world_query_inherited_arg_data_const(World& world, Entity owner, Entity id);
		bool world_term_uses_inherit_policy(const World& world, Entity term);
		template <typename T>
		Entity world_query_arg_id(World& world);

		//! QueryImpl constraints
		enum class Constraints : uint8_t { EnabledOnly, DisabledOnly, AcceptAll };

		namespace detail {
			template <Constraints IterConstraint>
			struct ChunkIterTypedOps;

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
				mutable const Archetype* pLastArchetype = nullptr;
				mutable Entity cachedOwner = EntityBad;
				mutable bool cachedDirect = false;

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					return world_query_entity_arg_by_id_cached_const<const U&>(
							*pWorld, pEntities[idx], id, pLastArchetype, cachedOwner, cachedDirect);
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
			template <typename U, bool WriteIm>
			struct EntityTermViewSetEntity {
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t cnt = 0;

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) {
					GAIA_ASSERT(idx < cnt);
					if constexpr (WriteIm)
						return world_query_entity_arg_by_id<U&>(*pWorld, pEntities[idx], id);
					else
						return world_query_entity_arg_by_id_raw<U&>(*pWorld, pEntities[idx], id);
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
				const U* pDataInherited = nullptr;
				mutable const Archetype* pLastArchetype = nullptr;
				mutable Entity cachedOwner = EntityBad;
				mutable bool cachedDirect = false;

				static EntityTermViewGet pointer(const U* pData, uint32_t cnt) {
					return {pData, nullptr, nullptr, EntityBad, cnt, nullptr, nullptr, EntityBad, false};
				}

				static EntityTermViewGet entity(const Entity* pEntities, World* pWorld, Entity id, uint32_t cnt) {
					return {nullptr, pEntities, pWorld, id, cnt, nullptr, nullptr, EntityBad, false};
				}

				static EntityTermViewGet inherited(const U* pDataInherited, uint32_t cnt) {
					return {nullptr, nullptr, nullptr, EntityBad, cnt, pDataInherited, nullptr, EntityBad, false};
				}

				static EntityTermViewGet entity_chunk_stable(
						const Entity* pEntities, const Chunk* pChunk, World* pWorld, Entity id, uint16_t rowBase, uint32_t cnt) {
					uint32_t compIdx = BadIndex;
					const U* pDataInherited = nullptr;
					bool direct = false;
					world_init_query_entity_arg_by_id_chunk_stable_const<U>(
							*pWorld, *pChunk, pEntities, id, direct, compIdx, pDataInherited);
					const U* pData = nullptr;
					if (direct)
						pData = reinterpret_cast<const U*>(pChunk->comp_ptr(compIdx, rowBase));

					return {pData, nullptr, pWorld, id, cnt, pDataInherited, nullptr, EntityBad, false};
				}

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					if (pData != nullptr)
						return pData[idx];
					if (pDataInherited != nullptr) {
						GAIA_ASSERT(pDataInherited != nullptr);
						return *pDataInherited;
					}

					return world_query_entity_arg_by_id_cached_const<const U&>(
							*pWorld, pEntities[idx], id, pLastArchetype, cachedOwner, cachedDirect);
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
				bool writeIm = true;

				static EntityTermViewSet pointer(U* pData, uint32_t cnt) {
					return {pData, nullptr, nullptr, EntityBad, cnt, true};
				}

				static EntityTermViewSet
				entity(const Entity* pEntities, World* pWorld, Entity id, uint32_t cnt, bool writeIm = true) {
					return {nullptr, pEntities, pWorld, id, cnt, writeIm};
				}

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) {
					GAIA_ASSERT(idx < cnt);
					if (pData != nullptr)
						return pData[idx];

					if (writeIm)
						return EntityTermViewSetEntity<U, true>{pEntities, pWorld, id, cnt}[idx];
					return EntityTermViewSetEntity<U, false>{pEntities, pWorld, id, cnt}[idx];
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
			template <typename U, bool WriteIm>
			struct SoATermRowWriteProxyEntity {
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t idx = 0;

				GAIA_NODISCARD operator U() const {
					return world_query_entity_arg_by_id<const U&>(*pWorld, pEntities[idx], id);
				}

				SoATermRowWriteProxyEntity& operator=(const U& value) {
					if constexpr (WriteIm)
						world_query_entity_arg_by_id<U&>(*pWorld, pEntities[idx], id) = value;
					else
						world_query_entity_arg_by_id_raw<U&>(*pWorld, pEntities[idx], id) = value;
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
			template <typename U, size_t Item, bool WriteIm>
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
						if constexpr (WriteIm)
							world_query_entity_arg_by_id<U&>(*pWorld, pEntities[idx], id) = meta::tuple_to_struct<U>(GAIA_MOV(tuple));
						else
							world_query_entity_arg_by_id_raw<U&>(*pWorld, pEntities[idx], id) =
									meta::tuple_to_struct<U>(GAIA_MOV(tuple));
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
			template <typename U, bool WriteIm>
			struct SoATermViewSetEntity {
				const Entity* pEntities = nullptr;
				World* pWorld = nullptr;
				Entity id = EntityBad;
				uint32_t cnt = 0;

				GAIA_NODISCARD auto operator[](size_t idx) {
					GAIA_ASSERT(idx < cnt);
					return SoATermRowWriteProxyEntity<U, WriteIm>{pEntities, pWorld, id, (uint32_t)idx};
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
					return SoATermFieldWriteProxyEntity<U, Item, WriteIm>{pEntities, pWorld, id, cnt};
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
				bool writeIm = true;

				GAIA_NODISCARD operator U() const {
					if (pData != nullptr)
						return (U)SoATermRowWriteProxyPointer<U>{pData, dataSize, idx};

					if (writeIm)
						return (U)SoATermRowWriteProxyEntity<U, true>{pEntities, pWorld, id, idx};
					return (U)SoATermRowWriteProxyEntity<U, false>{pEntities, pWorld, id, idx};
				}

				SoATermRowWriteProxy& operator=(const U& value) {
					if (pData != nullptr) {
						SoATermRowWriteProxyPointer<U>{pData, dataSize, idx} = value;
					} else if (writeIm)
						SoATermRowWriteProxyEntity<U, true>{pEntities, pWorld, id, idx} = value;
					else
						SoATermRowWriteProxyEntity<U, false>{pEntities, pWorld, id, idx} = value;
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
				bool writeIm = true;

				//! Proxy representing a single writable SoA field element in the runtime fallback path only.
				//! Direct chunk-backed paths should use SoATermFieldWriteProxyPointer, which returns raw field references.
				struct ElementProxy {
					value_type* pData = nullptr;
					const Entity* pEntities = nullptr;
					World* pWorld = nullptr;
					Entity id = EntityBad;
					uint32_t idx = 0;
					bool writeIm = true;

					GAIA_NODISCARD operator value_type() const {
						if (pData != nullptr)
							return SoATermFieldWriteProxyPointer<U, Item>{pData, idx + 1}[idx];

						if (writeIm)
							return SoATermFieldWriteProxyEntity<U, Item, true>{pEntities, pWorld, id, idx + 1}[idx];
						return SoATermFieldWriteProxyEntity<U, Item, false>{pEntities, pWorld, id, idx + 1}[idx];
					}

					ElementProxy& operator=(const value_type& value) {
						if (pData != nullptr) {
							SoATermFieldWriteProxyPointer<U, Item>{pData, idx + 1}[idx] = value;
							return *this;
						}

						if (writeIm)
							SoATermFieldWriteProxyEntity<U, Item, true>{pEntities, pWorld, id, idx + 1}[idx] = value;
						else
							SoATermFieldWriteProxyEntity<U, Item, false>{pEntities, pWorld, id, idx + 1}[idx] = value;
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
					return ElementProxy{pData, pEntities, pWorld, id, (uint32_t)idx, writeIm};
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
				bool writeIm = true;

				GAIA_NODISCARD auto operator[](size_t idx) {
					GAIA_ASSERT(idx < cnt);
					return SoATermRowWriteProxy<U>{pData, dataSize, pEntities, pWorld, id, idxBase + (uint32_t)idx, writeIm};
				}

				GAIA_NODISCARD U operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					if (pData != nullptr)
						return SoATermViewSetPointer<U>{pData, dataSize, idxBase, cnt}[idx];

					return SoATermViewSetEntity<U, true>{pEntities, pWorld, id, cnt}[idx];
				}

				template <size_t Item>
				GAIA_NODISCARD auto get() const {
					if (pData != nullptr) {
						const auto field = SoATermViewSetPointer<U>{pData, dataSize, idxBase, cnt}.template get<Item>();
						return SoATermFieldReadProxy<U, Item>{field.pData, nullptr, pWorld, id, cnt};
					}

					const auto field = SoATermViewSetEntity<U, true>{pEntities, pWorld, id, cnt}.template get<Item>();
					return SoATermFieldReadProxy<U, Item>{nullptr, field.pEntities, field.pWorld, field.id, field.cnt};
				}

				template <size_t Item>
				GAIA_NODISCARD auto set() {
					if (pData != nullptr) {
						auto field = SoATermViewSetPointer<U>{pData, dataSize, idxBase, cnt}.template set<Item>();
						return SoATermFieldWriteProxy<U, Item>{field.pData, nullptr, pWorld, id, cnt};
					}

					if (writeIm) {
						const auto field = SoATermViewSetEntity<U, true>{pEntities, pWorld, id, cnt}.template set<Item>();
						return SoATermFieldWriteProxy<U, Item>{nullptr, field.pEntities, field.pWorld, field.id, field.cnt, true};
					}

					const auto field = SoATermViewSetEntity<U, false>{pEntities, pWorld, id, cnt}.template set<Item>();
					return SoATermFieldWriteProxy<U, Item>{nullptr, field.pEntities, field.pWorld, field.id, field.cnt, false};
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			template <Constraints IterConstraint>
			class ChunkIterImpl {
				friend struct ChunkIterTypedOps<IterConstraint>;

			protected:
				using CompIndicesBitView = core::bit_view<ChunkHeader::MAX_COMPONENTS_BITS>;

				//! World pointer
				const World* m_pWorld = nullptr;
				//! Currently iterated archetype
				const Archetype* m_pArchetype = nullptr;
				//! Chunk currently associated with the iterator
				Chunk* m_pChunk = nullptr;
				//! ChunkHeader::MAX_COMPONENTS values for component indices mapping for the parent archetype
				const uint8_t* m_pCompIndices = nullptr;
				//! Optional inherited term data view for exact semantic self-source terms.
				InheritedTermDataView m_inheritedData;
				//! Optional per-term ids used by one-row direct iterators when a term resolves semantically.
				const Entity* m_pTermIdMapping = nullptr;
				//! Whether mutable access should finish writes immediately or defer them until the callback returns.
				bool m_writeIm = true;
				//! Chunk-backed columns that were exposed as mutable during the current callback.
				uint8_t m_touchedCompIndices[ChunkHeader::MAX_COMPONENTS]{};
				uint8_t m_touchedCompCnt = 0;
				//! Entity-backed terms that were exposed as mutable during the current callback.
				Entity m_touchedTerms[ChunkHeader::MAX_COMPONENTS]{};
				uint8_t m_touchedTermCnt = 0;
				//! Stable copy of the currently iterated entity rows for mutable entity-backed views.
				Entity m_entitySnapshot[ChunkHeader::MAX_CHUNK_ENTITIES]{};
				bool m_entitySnapshotValid = false;
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
					m_entitySnapshotValid = false;

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
					m_entitySnapshotValid = false;
					m_from = from;
					m_to = to;
				}

				GAIA_NODISCARD const Chunk* chunk() const {
					GAIA_ASSERT(m_pChunk != nullptr);
					return m_pChunk;
				}

				void set_comp_indices(const uint8_t* pCompIndices) {
					m_pCompIndices = pCompIndices;
				}

				void set_inherited_data(InheritedTermDataView inheritedData) {
					m_inheritedData = inheritedData;
				}

				void set_term_ids(const Entity* pTermIds) {
					m_pTermIdMapping = pTermIds;
				}

				GAIA_NODISCARD const uint8_t* comp_indices() const {
					return m_pCompIndices;
				}

				GAIA_NODISCARD InheritedTermDataView inherited_data() const {
					return m_inheritedData;
				}

				GAIA_NODISCARD const Entity* term_ids() const {
					return m_pTermIdMapping;
				}

				void set_write_im(bool value) {
					m_writeIm = value;
					clear_touched_writes();
				}

				GAIA_NODISCARD bool write_im() const {
					return m_writeIm;
				}

				void clear_touched_writes() {
					m_touchedCompCnt = 0;
					m_touchedTermCnt = 0;
				}

				GAIA_NODISCARD const Entity* entity_snapshot() {
					if (!m_entitySnapshotValid) {
						const auto cnt = size();
						GAIA_ASSERT(cnt <= ChunkHeader::MAX_CHUNK_ENTITIES);

						const auto entities = m_pChunk->entity_view();
						GAIA_FOR(cnt) {
							m_entitySnapshot[i] = entities[from() + i];
						}

						m_entitySnapshotValid = true;
					}

					return m_entitySnapshot;
				}

				GAIA_NODISCARD auto touched_comp_indices() const {
					return std::span<const uint8_t>{m_touchedCompIndices, m_touchedCompCnt};
				}

				GAIA_NODISCARD auto touched_terms() const {
					return std::span<const Entity>{m_touchedTerms, m_touchedTermCnt};
				}

				GAIA_NODISCARD auto entity_rows() {
					if (m_entitySnapshotValid)
						return std::span<const Entity>{m_entitySnapshot, size()};

					return std::span<const Entity>{m_pChunk->entity_view().data() + from(), size()};
				}

				template <typename U>
				GAIA_NODISCARD auto entity_view_set(Entity termId, bool writeIm) {
					return EntityTermViewSet<U>::entity(entity_snapshot(), world(), termId, size(), writeIm);
				}

				template <typename U>
				GAIA_NODISCARD auto entity_soa_view_set(Entity termId, bool writeIm) {
					return SoATermViewSet<U>{nullptr, 0, entity_snapshot(), world(), termId, 0, size(), writeIm};
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

				struct IterTermDesc {
					Entity termId = EntityBad;
					bool isEntity = false;
					bool isOutOfLine = false;
				};

				template <typename T>
				GAIA_NODISCARD IterTermDesc term_desc() const {
					return ChunkIterTypedOps<IterConstraint>::template term_desc<T>(*this);
				}

				GAIA_NODISCARD IterTermDesc resolved_term_desc(uint32_t termIdx, IterTermDesc desc) const {
					if (m_pTermIdMapping != nullptr) {
						const auto mappedTermId = m_pTermIdMapping[termIdx];
						if (mappedTermId != EntityBad)
							desc.termId = mappedTermId;
					}

					if (!desc.isEntity && desc.termId != EntityBad)
						desc.isOutOfLine = world_is_out_of_line_component(*world(), desc.termId);

					return desc;
				}

				void touch_comp_idx(uint8_t compIdx) {
					GAIA_ASSERT(compIdx != 0xFF);
					GAIA_FOR(m_touchedCompCnt) {
						if (m_touchedCompIndices[i] == compIdx)
							return;
					}

					GAIA_ASSERT(m_touchedCompCnt < ChunkHeader::MAX_COMPONENTS);
					m_touchedCompIndices[m_touchedCompCnt++] = compIdx;
				}

				void touch_term(Entity term) {
					GAIA_ASSERT(term != EntityBad);
					GAIA_FOR(m_touchedTermCnt) {
						if (m_touchedTerms[i] == term)
							return;
					}

					GAIA_ASSERT(m_touchedTermCnt < ChunkHeader::MAX_COMPONENTS);
					m_touchedTerms[m_touchedTermCnt++] = term;
				}

				void touch_term_desc(const IterTermDesc& desc) {
					if (desc.isEntity)
						return;
					if (desc.isOutOfLine)
						touch_term(desc.termId);
					else
						touch_comp_idx((uint8_t)m_pChunk->comp_idx(desc.termId));
				}

				//! Returns a read-only entity or component view that can resolve non-direct storage.
				//! This is the fallback accessor for terms that may come from outside the chunk column,
				//! such as inherited, sparse, out-of-line, or other entity-backed storage.
				//! \warning If @a T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity of component view with read-only access
				template <typename T>
				GAIA_NODISCARD auto view_any() const {
					return ChunkIterTypedOps<IterConstraint>::template view_any<T>(*this);
				}

				//! Returns a read-only entity or component view for a query-term index that can resolve non-direct storage.
				//! Use this when the term may resolve to inherited, sparse, out-of-line, or other entity-backed storage
				//! instead of a dense chunk column.
				//! \warning It is expected the term index maps to a valid query term for @a T.
				//! \tparam T Component or Entity
				//! \param termIdx Query term index
				//! \return Entity or component view with read-only access
				template <typename T>
				GAIA_NODISCARD auto view_any(uint32_t termIdx) {
					return ChunkIterTypedOps<IterConstraint>::template view_any<T>(*this, termIdx);
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
					return ChunkIterTypedOps<IterConstraint>::template view<T>(*this);
				}

				//! Returns a read-only entity or component view for a query-term owned chunk-backed term.
				//! The caller is responsible for passing a term index that maps to a dense chunk column.
				//! Use view_any(termIdx) when the term may resolve to inherited, sparse, out-of-line, or other
				//! non-direct storage.
				//! \warning Passing a non-mapped or entity-backed term index is invalid.
				//! \tparam T Component or Entity
				//! \param termIdx Query term index
				//! \return Direct read-only entity or component view
				template <typename T>
				GAIA_NODISCARD auto view(uint32_t termIdx) const {
					return ChunkIterTypedOps<IterConstraint>::template view<T>(*this, termIdx);
				}

				//! Returns a mutable entity or component view that can resolve non-direct storage.
				//! This is the fallback accessor for inherited, sparse, out-of-line, or other entity-backed
				//! terms that are not guaranteed to be backed by the current chunk column.
				//! \warning If @a T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto view_any_mut() {
					return ChunkIterTypedOps<IterConstraint>::template view_any_mut<T>(*this);
				}

				//! Returns a mutable entity or component view for the owned chunk-backed fast path.
				//! Updates world versioning through the underlying chunk view and skips all non-direct dispatch.
				//! Use view_any_mut() when the term may resolve to inherited, sparse, out-of-line, or other
				//! entity-backed storage.
				//! \tparam T Component or Entity
				//! \return Direct entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto view_mut() {
					return ChunkIterTypedOps<IterConstraint>::template view_mut<T>(*this);
				}

				//! Returns a mutable entity or component view for a query-term owned chunk-backed term.
				//! Updates world versioning for the selected chunk column before handing out mutable access.
				//! Use view_any_mut(termIdx) when the term may resolve to inherited, sparse, out-of-line, or
				//! other non-direct storage.
				//! \warning Passing a non-mapped or entity-backed term index is invalid.
				//! \tparam T Component or Entity
				//! \param termIdx Query term index
				//! \return Direct entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto view_mut(uint32_t termIdx) {
					return ChunkIterTypedOps<IterConstraint>::template view_mut<T>(*this, termIdx);
				}

				//! Returns a mutable entity or component view for a query-term index that can resolve non-direct storage.
				//! Use this when the term may resolve to inherited, sparse, out-of-line, or other entity-backed
				//! storage instead of a dense chunk column.
				//! Updates world versioning for chunk-backed terms before handing out mutable access.
				//! \warning It is expected the term index maps to a valid query term for @a T.
				//! \tparam T Component or Entity
				//! \param termIdx Query term index
				//! \return Entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto view_any_mut(uint32_t termIdx) {
					return ChunkIterTypedOps<IterConstraint>::template view_any_mut<T>(*this, termIdx);
				}

				//! Returns a mutable component view that can resolve non-direct storage.
				//! This is the fallback accessor for inherited, sparse, out-of-line, or other entity-backed
				//! terms that are not guaranteed to be backed by the current chunk column.
				//! Doesn't update the world version when the access is acquired.
				//! \warning It is expected the component @a T is present. Undefined behavior otherwise.
				//! \tparam T Component
				//! \return Component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto sview_any_mut() {
					return ChunkIterTypedOps<IterConstraint>::template sview_any_mut<T>(*this);
				}

				//! Returns a mutable component view for the owned chunk-backed fast path.
				//! Doesn't update the world version when the access is acquired and skips all non-direct dispatch.
				//! Use sview_any_mut() when the term may resolve to inherited, sparse, out-of-line, or other
				//! entity-backed storage.
				//! \tparam T Component
				//! \return Direct component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto sview_mut() {
					return ChunkIterTypedOps<IterConstraint>::template sview_mut<T>(*this);
				}

				//! Returns a mutable component view for a query-term owned chunk-backed term.
				//! Doesn't update the world version when the access is acquired.
				//! Use sview_any_mut(termIdx) when the term may resolve to inherited, sparse, out-of-line, or
				//! other non-direct storage.
				//! \warning Passing a non-mapped or entity-backed term index is invalid.
				//! \tparam T Component
				//! \param termIdx Query term index
				//! \return Direct component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto sview_mut(uint32_t termIdx) {
					return ChunkIterTypedOps<IterConstraint>::template sview_mut<T>(*this, termIdx);
				}

				//! Returns a mutable component view for a query-term index that can resolve non-direct storage.
				//! Use this when the term may resolve to inherited, sparse, out-of-line, or other entity-backed
				//! storage instead of a dense chunk column.
				//! Doesn't update the world version when the access is acquired.
				//! \warning It is expected the term index maps to a valid query term for @a T.
				//! \tparam T Component
				//! \param termIdx Query term index
				//! \return Component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto sview_any_mut(uint32_t termIdx) {
					return ChunkIterTypedOps<IterConstraint>::template sview_any_mut<T>(*this, termIdx);
				}

				//! Marks the component @a T as modified. Best used with sview to manually trigger
				//! an update at user's whim.
				//! If \tparam TriggerHooks is true, also triggers the component's set hooks.
				template <typename T, bool TriggerHooks>
				void modify() {
					ChunkIterTypedOps<IterConstraint>::template modify<T, TriggerHooks>(*this);
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
					return ChunkIterTypedOps<IterConstraint>::template view_auto<T>(*this);
				}

				//! Returns either a mutable or immutable entity/component view that can resolve non-direct storage.
				//! Value and const types are considered immutable. Anything else is mutable.
				//! Use this when the term may resolve to inherited, sparse, out-of-line, or other entity-backed storage.
				//! \warning If @a T is a component it is expected to be present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view
				template <typename T>
				GAIA_NODISCARD auto view_auto_any() {
					return ChunkIterTypedOps<IterConstraint>::template view_auto_any<T>(*this);
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
					return ChunkIterTypedOps<IterConstraint>::template sview_auto_any<T>(*this);
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
					return ChunkIterTypedOps<IterConstraint>::template sview_auto<T>(*this);
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
					return ChunkIterTypedOps<IterConstraint>::template has<T>(*this);
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

				//! Returns the first row covered by the iterator in the current chunk.
				GAIA_NODISCARD uint16_t row_begin() const noexcept {
					return from();
				}

				//! Returns one-past-the-end row covered by the iterator in the current chunk.
				GAIA_NODISCARD uint16_t row_end() const noexcept {
					return to();
				}

				//! Returns the absolute index that should be used to access an item in the chunk.
				//! AoS indices map directly, SoA indices need some adjustments because the view is
				//! always considered {0..ChunkCapacity} instead of {FirstEnabled..ChunkSize}.
				template <typename T>
				uint32_t acc_index(uint32_t idx) const noexcept {
					return ChunkIterTypedOps<IterConstraint>::template acc_index<T>(*this, idx);
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
			GAIA_NODISCARD auto view() const;

			//! Returns a mutable entity or component view.
			//! \warning If @a T is a component it is expected it is present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \return Entity or component view with read-write access
			template <typename T>
			GAIA_NODISCARD auto view_mut();

			//! Returns a mutable component view.
			//! Doesn't update the world version when the access is acquired.
			//! \warning It is expected the component @a T is present. Undefined behavior otherwise.
			//! \tparam T Component
			//! \return Component view with read-write access
			template <typename T>
			GAIA_NODISCARD auto sview_mut();

			//! Marks the component @a T as modified. Best used with sview to manually trigger
			//! an update at user's whim.
			//! If \tparam TriggerHooks is true, also triggers the component's set hooks.
			template <typename T, bool TriggerHooks>
			void modify();

			//! Returns either a mutable or immutable entity/component view based on the requested type.
			//! Value and const types are considered immutable. Anything else is mutable.
			//! \warning If @a T is a component it is expected to be present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \return Entity or component view
			template <typename T>
			GAIA_NODISCARD auto view_auto();

			//! Returns either a mutable or immutable entity/component view based on the requested type.
			//! Value and const types are considered immutable. Anything else is mutable.
			//! Doesn't update the world version when read-write access is acquired.
			//! \warning If @a T is a component it is expected to be present. Undefined behavior otherwise.
			//! \tparam T Component or Entity
			//! \return Entity or component view
			template <typename T>
			GAIA_NODISCARD auto sview_auto();

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
			GAIA_NODISCARD bool has() const;

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

#include "gaia/ecs/chunk_iterator_typed.inl"

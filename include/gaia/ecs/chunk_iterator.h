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

			//! Lightweight term view that can read either contiguous chunk data or a world-backed
			//! out-of-line payload resolved by entity id.
			template <typename U>
			struct EntityTermViewGet {
				enum class Mode : uint8_t { Chunk, Pointer, Entity };

				const Chunk* pChunk = nullptr;
				World* pWorld = nullptr;
				const U* pData = nullptr;
				Entity id = EntityBad;
				uint32_t idxBase = 0;
				uint32_t cnt = 0;
				Mode mode = Mode::Chunk;

				static EntityTermViewGet chunk(const Chunk* pChunk, World* pWorld, uint32_t idxBase, uint32_t cnt) {
					return {pChunk, pWorld, nullptr, EntityBad, idxBase, cnt, Mode::Chunk};
				}

				static EntityTermViewGet pointer(const Chunk* pChunk, World* pWorld, const U* pData, uint32_t cnt) {
					return {pChunk, pWorld, pData, EntityBad, 0, cnt, Mode::Pointer};
				}

				static EntityTermViewGet entity(const Chunk* pChunk, World* pWorld, Entity id, uint32_t idxBase, uint32_t cnt) {
					return {pChunk, pWorld, nullptr, id, idxBase, cnt, Mode::Entity};
				}

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					switch (mode) {
						case Mode::Chunk:
							return pChunk->template view<U>()[idxBase + (uint32_t)idx];
						case Mode::Pointer:
							return pData[idx];
						case Mode::Entity: {
							const auto entity = pChunk->entity_view()[idxBase + (uint32_t)idx];
							return world_query_entity_arg_by_id<const U&>(*pWorld, entity, id);
						}
					}

					GAIA_ASSERT(false);
					return pData[0];
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}

				GAIA_NODISCARD const U* data() const noexcept {
					switch (mode) {
						case Mode::Chunk:
							return reinterpret_cast<const U*>(pChunk->template view<U>().data()) + idxBase;
						case Mode::Pointer:
							return pData;
						case Mode::Entity:
							return nullptr;
					}

					GAIA_ASSERT(false);
					return nullptr;
				}
			};

			//! Mutable counterpart to EntityTermViewGet. For chunk-backed terms it preserves the
			//! usual versioning behavior; for out-of-line terms it forwards to the world store.
			template <typename U>
			struct EntityTermViewSet {
				enum class Mode : uint8_t { ChunkVersioned, ChunkSilent, Pointer, Entity };

				Chunk* pChunk = nullptr;
				World* pWorld = nullptr;
				U* pData = nullptr;
				Entity id = EntityBad;
				uint32_t idxBase = 0;
				uint32_t cnt = 0;
				Mode mode = Mode::ChunkVersioned;

				static EntityTermViewSet chunk(Chunk* pChunk, World* pWorld, uint32_t idxBase, uint32_t cnt, bool updateVersion) {
					return updateVersion
								   ? EntityTermViewSet{pChunk, pWorld, nullptr, EntityBad, idxBase, cnt, Mode::ChunkVersioned}
								   : EntityTermViewSet{pChunk, pWorld, nullptr, EntityBad, idxBase, cnt, Mode::ChunkSilent};
				}

				static EntityTermViewSet pointer(Chunk* pChunk, World* pWorld, U* pData, uint32_t cnt) {
					return {pChunk, pWorld, pData, EntityBad, 0, cnt, Mode::Pointer};
				}

				static EntityTermViewSet entity(Chunk* pChunk, World* pWorld, Entity id, uint32_t idxBase, uint32_t cnt) {
					return {pChunk, pWorld, nullptr, id, idxBase, cnt, Mode::Entity};
				}

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) {
					GAIA_ASSERT(idx < cnt);
					switch (mode) {
						case Mode::ChunkVersioned:
							return pChunk->template view_mut<U>()[idxBase + (uint32_t)idx];
						case Mode::ChunkSilent:
							return pChunk->template sview_mut<U>()[idxBase + (uint32_t)idx];
						case Mode::Pointer:
							return pData[idx];
						case Mode::Entity: {
							const auto entity = pChunk->entity_view()[idxBase + (uint32_t)idx];
							return world_query_entity_arg_by_id<U&>(*pWorld, entity, id);
						}
					}

					GAIA_ASSERT(false);
					return pData[0];
				}

				GAIA_NODISCARD decltype(auto) operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					switch (mode) {
						case Mode::ChunkVersioned:
						case Mode::ChunkSilent:
							return pChunk->template view<U>()[idxBase + (uint32_t)idx];
						case Mode::Pointer:
							return pData[idx];
						case Mode::Entity: {
							const auto entity = pChunk->entity_view()[idxBase + (uint32_t)idx];
							return world_query_entity_arg_by_id<const U&>(*pWorld, entity, id);
						}
					}

					GAIA_ASSERT(false);
					return pData[0];
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}

				GAIA_NODISCARD U* data() noexcept {
					switch (mode) {
						case Mode::ChunkVersioned:
							return reinterpret_cast<U*>(pChunk->template view_mut<U>().data()) + idxBase;
						case Mode::ChunkSilent:
							return reinterpret_cast<U*>(pChunk->template sview_mut<U>().data()) + idxBase;
						case Mode::Pointer:
							return pData;
						case Mode::Entity:
							return nullptr;
					}

					GAIA_ASSERT(false);
					return nullptr;
				}

				GAIA_NODISCARD const U* data() const noexcept {
					switch (mode) {
						case Mode::ChunkVersioned:
						case Mode::ChunkSilent:
							return reinterpret_cast<const U*>(pChunk->template view<U>().data()) + idxBase;
						case Mode::Pointer:
							return pData;
						case Mode::Entity:
							return nullptr;
					}

					GAIA_ASSERT(false);
					return nullptr;
				}
			};

			template <typename U>
			struct SoATermRowWriteProxy {
				Chunk* pChunk = nullptr;
				World* pWorld = nullptr;
				Entity entity = EntityBad;
				Entity id = EntityBad;
				uint32_t idx = 0;

				GAIA_NODISCARD operator U() const {
					if (pChunk != nullptr)
						return pChunk->template view<U>()[idx];

					return world_query_entity_arg_by_id<const U&>(*pWorld, entity, id);
				}

				SoATermRowWriteProxy& operator=(const U& value) {
					if (pChunk != nullptr)
						pChunk->template sview_mut<U>()[idx] = value;
					else
						world_query_entity_arg_by_id<U&>(*pWorld, entity, id) = value;
					return *this;
				}
			};

			template <typename U, size_t Item>
			struct SoATermFieldReadProxy {
				using view_policy = mem::auto_view_policy_get<U>;
				using value_type = typename view_policy::template data_view_policy_idx_info<Item>::const_value_type;

				const Chunk* pChunk = nullptr;
				World* pWorld = nullptr;
				Entity entity = EntityBad;
				Entity id = EntityBad;
				uint32_t idxBase = 0;
				uint32_t cnt = 0;

				GAIA_NODISCARD value_type operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					if (pChunk != nullptr)
						return pChunk->template view<U>().template get<Item>()[idxBase + idx];

					const U value = world_query_entity_arg_by_id<const U&>(*pWorld, entity, id);
					return std::get<Item>(meta::struct_to_tuple(value));
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			template <typename U, size_t Item>
			struct SoATermFieldWriteProxy {
				using view_policy = mem::auto_view_policy_set<U>;
				using value_type = typename view_policy::template data_view_policy_idx_info<Item>::value_type;

				Chunk* pChunk = nullptr;
				World* pWorld = nullptr;
				Entity entity = EntityBad;
				Entity id = EntityBad;
				uint32_t idxBase = 0;
				uint32_t cnt = 0;

				struct ElementProxy {
					Chunk* pChunk = nullptr;
					World* pWorld = nullptr;
					Entity entity = EntityBad;
					Entity id = EntityBad;
					uint32_t idx = 0;

					GAIA_NODISCARD operator value_type() const {
						if (pChunk != nullptr)
							return pChunk->template view<U>().template get<Item>()[idx];

						const U value = world_query_entity_arg_by_id<const U&>(*pWorld, entity, id);
						return std::get<Item>(meta::struct_to_tuple(value));
					}

					ElementProxy& operator=(const value_type& value) {
						if (pChunk != nullptr) {
							pChunk->template sview_mut<U>().template set<Item>()[idx] = value;
							return *this;
						}

						auto data = world_query_entity_arg_by_id<const U&>(*pWorld, entity, id);
						auto tuple = meta::struct_to_tuple(data);
						std::get<Item>(tuple) = value;
						world_query_entity_arg_by_id<U&>(*pWorld, entity, id) = meta::tuple_to_struct<U>(GAIA_MOV(tuple));
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
					return ElementProxy{pChunk, pWorld, entity, id, idxBase + (uint32_t)idx};
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			template <typename U>
			struct SoATermViewGet {
				const Chunk* pChunk = nullptr;
				World* pWorld = nullptr;
				Entity entity = EntityBad;
				Entity id = EntityBad;
				uint32_t idxBase = 0;
				uint32_t cnt = 0;

				GAIA_NODISCARD U operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					if (pChunk != nullptr)
						return pChunk->template view<U>()[idxBase + idx];

					return world_query_entity_arg_by_id<const U&>(*pWorld, entity, id);
				}

				template <size_t Item>
				GAIA_NODISCARD auto get() const {
					return SoATermFieldReadProxy<U, Item>{pChunk, pWorld, entity, id, idxBase, cnt};
				}

				GAIA_NODISCARD constexpr size_t size() const noexcept {
					return cnt;
				}
			};

			template <typename U>
			struct SoATermViewSet {
				Chunk* pChunk = nullptr;
				World* pWorld = nullptr;
				Entity entity = EntityBad;
				Entity id = EntityBad;
				uint32_t idxBase = 0;
				uint32_t cnt = 0;

				GAIA_NODISCARD auto operator[](size_t idx) {
					GAIA_ASSERT(idx < cnt);
					return SoATermRowWriteProxy<U>{pChunk, pWorld, entity, id, idxBase + (uint32_t)idx};
				}

				GAIA_NODISCARD U operator[](size_t idx) const {
					GAIA_ASSERT(idx < cnt);
					if (pChunk != nullptr)
						return pChunk->template view<U>()[idxBase + idx];

					return world_query_entity_arg_by_id<const U&>(*pWorld, entity, id);
				}

				template <size_t Item>
				GAIA_NODISCARD auto get() const {
					return SoATermFieldReadProxy<U, Item>{pChunk, pWorld, entity, id, idxBase, cnt};
				}

				template <size_t Item>
				GAIA_NODISCARD auto set() {
					return SoATermFieldWriteProxy<U, Item>{pChunk, pWorld, entity, id, idxBase, cnt};
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

				//! Returns a read-only entity or component view.
				//! \warning If @a T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity of component view with read-only access
				template <typename T>
				GAIA_NODISCARD auto view() const {
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
							return EntityTermViewGet<U>::entity(m_pChunk, const_cast<World*>(world()), id, from(), size());
						return EntityTermViewGet<U>::chunk(m_pChunk, const_cast<World*>(world()), from(), size());
					}
				}

				template <typename T>
				GAIA_NODISCARD auto view(uint32_t termIdx) {
					using U = typename actual_type_t<T>::Type;

					if constexpr (mem::is_soa_layout_v<U>) {
						const auto compIdx = m_pCompIdxMapping[termIdx];
						if (compIdx == 0xFF) {
							GAIA_ASSERT(m_pTermIdMapping != nullptr);
							GAIA_ASSERT(size() == 1);

							const auto entity = m_pChunk->entity_view()[from()];
							const auto id = m_pTermIdMapping[termIdx];
							return SoATermViewGet<U>{nullptr, world(), entity, id, 0, 1};
						}

						GAIA_ASSERT(compIdx < m_pChunk->ids_view().size());
						return SoATermViewGet<U>{m_pChunk, world(), EntityBad, EntityBad, from(), size()};
					} else {
						const auto compIdx = m_pCompIdxMapping[termIdx];
						const auto id = m_pTermIdMapping != nullptr ? m_pTermIdMapping[termIdx] : EntityBad;
						if (id != EntityBad && world_is_out_of_line_component(*world(), id))
							return EntityTermViewGet<U>::entity(m_pChunk, const_cast<World*>(world()), id, from(), size());

						if (compIdx == 0xFF) {
							GAIA_ASSERT(m_pTermIdMapping != nullptr);
							GAIA_ASSERT(size() == 1);

							const auto entity = m_pChunk->entity_view()[from()];
							const auto id = m_pTermIdMapping[termIdx];
							const auto& data = world_query_entity_arg_by_id<const U&>(*world(), entity, id);
							return EntityTermViewGet<U>::pointer(m_pChunk, const_cast<World*>(world()), &data, 1);
						}
						GAIA_ASSERT(compIdx < m_pChunk->ids_view().size());

						auto* pData = reinterpret_cast<const U*>(m_pChunk->comp_ptr_mut(compIdx, from()));
						return EntityTermViewGet<U>::pointer(m_pChunk, const_cast<World*>(world()), pData, size());
					}
				}

				//! Returns a mutable entity or component view.
				//! \warning If @a T is a component it is expected it is present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto view_mut() {
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
							return EntityTermViewSet<U>::entity(m_pChunk, world(), id, from(), size());
						return EntityTermViewSet<U>::chunk(m_pChunk, world(), from(), size(), true);
					}
				}

				template <typename T>
				GAIA_NODISCARD auto view_mut(uint32_t termIdx) {
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
							return SoATermViewSet<U>{ec.pChunk, world(), EntityBad, EntityBad, ec.row, 1};
						}

						GAIA_ASSERT(compIdx < m_pChunk->comp_rec_view().size());
						m_pChunk->update_world_version(compIdx);
						return SoATermViewSet<U>{m_pChunk, world(), EntityBad, EntityBad, from(), size()};
					} else {
						const auto compIdx = m_pCompIdxMapping[termIdx];
						const auto id = m_pTermIdMapping != nullptr ? m_pTermIdMapping[termIdx] : EntityBad;
						if (id != EntityBad && world_is_out_of_line_component(*world(), id))
							return EntityTermViewSet<U>::entity(m_pChunk, world(), id, from(), size());

						if (compIdx == 0xFF) {
							GAIA_ASSERT(m_pTermIdMapping != nullptr);
							GAIA_ASSERT(size() == 1);

							const auto entity = m_pChunk->entity_view()[from()];
							const auto id = m_pTermIdMapping[termIdx];
							auto& data = world_query_entity_arg_by_id<U&>(*world(), entity, id);
							return EntityTermViewSet<U>::pointer(m_pChunk, world(), &data, 1);
						}
						GAIA_ASSERT(compIdx < m_pChunk->comp_rec_view().size());

						m_pChunk->update_world_version(compIdx);

						auto* pData = reinterpret_cast<U*>(m_pChunk->comp_ptr_mut(compIdx, from()));
						return EntityTermViewSet<U>::pointer(m_pChunk, world(), pData, size());
					}
				}

				//! Returns a mutable component view.
				//! Doesn't update the world version when the access is acquired.
				//! \warning It is expected the component @a T is present. Undefined behavior otherwise.
				//! \tparam T Component
				//! \return Component view with read-write access
				template <typename T>
				GAIA_NODISCARD auto sview_mut() {
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
							return EntityTermViewSet<U>::entity(m_pChunk, world(), id, from(), size());
						return EntityTermViewSet<U>::chunk(m_pChunk, world(), from(), size(), false);
					}
				}

				template <typename T>
				GAIA_NODISCARD auto sview_mut(uint32_t termIdx) {
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
							return SoATermViewSet<U>{ec.pChunk, world(), EntityBad, EntityBad, ec.row, 1};
						}

						GAIA_ASSERT(compIdx < m_pChunk->ids_view().size());
						return SoATermViewSet<U>{m_pChunk, world(), EntityBad, EntityBad, from(), size()};
					} else {
						const auto compIdx = m_pCompIdxMapping[termIdx];
						const auto id = m_pTermIdMapping != nullptr ? m_pTermIdMapping[termIdx] : EntityBad;
						if (id != EntityBad && world_is_out_of_line_component(*world(), id))
							return EntityTermViewSet<U>::entity(m_pChunk, world(), id, from(), size());

						if (compIdx == 0xFF) {
							GAIA_ASSERT(m_pTermIdMapping != nullptr);
							GAIA_ASSERT(size() == 1);

							const auto entity = m_pChunk->entity_view()[from()];
							const auto id = m_pTermIdMapping[termIdx];
							auto& data = world_query_entity_arg_by_id<U&>(*world(), entity, id);
							return EntityTermViewSet<U>::pointer(m_pChunk, world(), &data, 1);
						}
						GAIA_ASSERT(compIdx < m_pChunk->ids_view().size());

						auto* pData = reinterpret_cast<U*>(m_pChunk->comp_ptr_mut(compIdx, from()));
						return EntityTermViewSet<U>::pointer(m_pChunk, world(), pData, size());
					}
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
					using UOriginal = typename actual_type_t<T>::TypeOriginal;
					if constexpr (core::is_mut_v<UOriginal>)
						return view_mut<T>();
					else
						return view<T>();
				}

				//! Returns either a mutable or immutable entity/component view based on the requested type.
				//! Value and const types are considered immutable. Anything else is mutable.
				//! Doesn't update the world version when read-write access is acquired.
				//! \warning If @a T is a component it is expected to be present. Undefined behavior otherwise.
				//! \tparam T Component or Entity
				//! \return Entity or component view
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

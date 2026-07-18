#pragma once
#include "gaia/config/config.h"

#include <type_traits>

#include "gaia/cnt/darray.h"
#include "gaia/cnt/map.h"
#include "gaia/cnt/sarray.h"
#include "gaia/cnt/sarray_ext.h"
#include "gaia/core/bit_utils.h"
#include "gaia/core/hashing_policy.h"
#include "gaia/core/span.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/api.h"
#include "gaia/ecs/component.h"
#include "gaia/ecs/id.h"
#include "gaia/ecs/query_fwd.h"
#include "gaia/ecs/query_mask.h"

namespace gaia {
	namespace ecs {
		class World;
		class Archetype;
		//! Read-only component-data pointers for every query field of one matched archetype.
		using InheritedTermDataView = std::span<const void* const>;
		GAIA_NODISCARD uint32_t world_rel_version(const World& world, Entity relation);
		GAIA_NODISCARD bool world_has_entity_term(const World& world, Entity entity, Entity term);
		GAIA_NODISCARD bool world_has_entity_term_in(const World& world, Entity entity, Entity term);
		GAIA_NODISCARD bool world_has_entity_term_direct(const World& world, Entity entity, Entity term);
		GAIA_NODISCARD bool world_term_uses_inherit_policy(const World& world, Entity term);
		GAIA_NODISCARD bool world_relation_uses_non_fragmenting_storage(const World& world, Entity relation);
		GAIA_NODISCARD bool world_component_uses_sparse_storage(const World& world, Entity component);
		GAIA_NODISCARD bool world_component_is_non_fragmenting(const World& world, Entity component);
		GAIA_NODISCARD uint32_t world_count_direct_term_entities(const World& world, Entity term);
		GAIA_NODISCARD uint32_t world_count_in_term_entities(const World& world, Entity term);
		GAIA_NODISCARD uint32_t world_count_direct_term_entities_direct(const World& world, Entity term);
		void world_collect_direct_term_entities(const World& world, Entity term, cnt::darray<Entity>& out);
		void world_collect_in_term_entities(const World& world, Entity term, cnt::darray<Entity>& out);
		void world_collect_direct_term_entities_direct(const World& world, Entity term, cnt::darray<Entity>& out);
		GAIA_NODISCARD bool
		world_for_each_direct_term_entity(const World& world, Entity term, void* ctx, bool (*func)(void*, Entity));
		GAIA_NODISCARD bool
		world_for_each_in_term_entity(const World& world, Entity term, void* ctx, bool (*func)(void*, Entity));
		GAIA_NODISCARD bool
		world_for_each_direct_term_entity_direct(const World& world, Entity term, void* ctx, bool (*func)(void*, Entity));
		GAIA_NODISCARD bool world_entity_enabled(const World& world, Entity entity);
		GAIA_NODISCARD bool world_entity_prefab(const World& world, Entity entity);
		GAIA_NODISCARD const Archetype* world_entity_archetype(const World& world, Entity entity);
		void world_finish_write(World& world, Entity term, Entity entity);
		GAIA_NODISCARD uint32_t world_component_index_bucket_size(const World& world, Entity term);
		GAIA_NODISCARD uint32_t world_component_index_comp_idx(const World& world, const Archetype& archetype, Entity term);
		GAIA_NODISCARD uint32_t
		world_component_index_match_count(const World& world, const Archetype& archetype, Entity term);
		//! Groups fragmenting hierarchy archetypes by depth for depth-ordered top-down iteration.
		//! \param world World containing the archetype hierarchy.
		//! \param archetype Archetype whose hierarchy depth determines the group.
		//! \param relation Traversal relation defining the hierarchy.
		//! \return Hierarchy depth encoded as the query group identifier.
		GAIA_NODISCARD GroupId group_by_func_depth_order(const World& world, const Archetype& archetype, Entity relation);
		template <typename T>
		GAIA_NODISCARD decltype(auto) world_direct_entity_arg(World& world, Entity entity);
		template <typename T>
		GAIA_NODISCARD decltype(auto) world_query_entity_arg(World& world, Entity entity);
		template <typename T>
		GAIA_NODISCARD Entity world_query_arg_id(World& world);
		template <typename T>
		GAIA_NODISCARD decltype(auto) world_query_entity_arg_by_id(World& world, Entity entity, Entity id);
		template <typename T>
		GAIA_NODISCARD decltype(auto) world_query_entity_arg_by_id_raw(World& world, Entity entity, Entity id);
		//! Returns the per-entity archetype version used for targeted source-query freshness checks.
		GAIA_NODISCARD uint32_t world_entity_archetype_version(const World& world, Entity entity);

		//! Number of items that can be a part of Query
		static constexpr uint32_t MAX_ITEMS_IN_QUERY = 12U;
		//! Maximum traversal depth.
		static constexpr uint32_t MAX_TRAV_DEPTH = 128U;

		GAIA_GCC_WARNING_PUSH()
		// GCC is unnecessarily too strict about shadowing.
		// We have a globally defined entity All and thinks QueryOpKind::All shadows it.
		GAIA_GCC_WARNING_DISABLE("-Wshadow")

		//! Boolean operation applied to a query term.
		enum class QueryOpKind : uint8_t {
			//! Requires the term to match.
			All,
			//! Accepts the term as one alternative in an OR block.
			Or,
			//! Requires the term not to match.
			Not,
			//! Treats the term as optional for matching.
			Any,
			//! Number of operation kinds.
			Count
		};
		//! Component access requested by a query term.
		enum class QueryAccess : uint8_t {
			//! Access is inferred from the typed term.
			None,
			//! Read-only component access.
			Read,
			//! Mutable component access.
			Write
		};
		//! Term match semantics.
		enum class QueryMatchKind : uint8_t {
			//! Applies Gaia-ECS semantic matching, including inherited Is targets.
			Semantic,
			//! Matches entities containing the term directly or through inheritance.
			In,
			//! Matches only directly stored terms.
			Direct
		};
		//! Flags derived from query input parsing.
		enum class QueryInputFlags : uint8_t {
			//! Input contains no special form.
			None,
			//! Input contains a query variable.
			Variable
		};
		//! Source traversal filter used for source lookups.
		enum class QueryTravKind : uint8_t {
			//! Does not inspect any source entity.
			None = 0x00,
			//! Inspects the source entity itself.
			Self = 0x01,
			//! Traverses relation targets from the source.
			Up = 0x02,
			//! Traverses entities that target the source through the relation.
			Down = 0x04
		};

		GAIA_GCC_WARNING_POP()

		//! Combines source traversal filters.
		//! \param lhs First traversal filter.
		//! \param rhs Second traversal filter.
		//! \return Bitwise union of \a lhs and \a rhs.
		GAIA_NODISCARD constexpr QueryTravKind operator|(QueryTravKind lhs, QueryTravKind rhs) {
			return (QueryTravKind)((uint8_t)lhs | (uint8_t)rhs);
		}

		//! Tests whether a traversal filter contains a requested bit.
		//! \param value Combined traversal filter.
		//! \param bit Traversal bit to test.
		//! \return True when \a bit is set in \a value.
		GAIA_NODISCARD constexpr bool query_trav_has(QueryTravKind value, QueryTravKind bit) {
			return (((uint8_t)value) & ((uint8_t)bit)) != 0;
		}

		//! Direct-hash representation of a canonical query lookup key.
		using QueryLookupHash = core::direct_hash_key<uint64_t>;
		//! Fixed-capacity entity storage used by compiled query metadata.
		using QueryEntityArray = cnt::sarray<Entity, MAX_ITEMS_IN_QUERY>;
		//! Per-query serialization buffers indexed by query identifier.
		using QuerySerMap = cnt::map<QueryId, QuerySerBuffer>;

		//! Incremental query-matching cursor for one entity-to-archetype lookup bucket.
		struct QueryArchetypeCacheCursor {
			//! Number of bucket records that were already matched at \a revision.
			uint32_t index = 0;
			//! Lookup-bucket revision associated with \a index.
			uint32_t revision = 0;
		};

		//! Per-lookup-key cursors used for incremental archetype matching.
		using QueryArchetypeCacheIndexMap = cnt::map<EntityLookupKey, QueryArchetypeCacheCursor>;
		//! Revision table for entity-to-archetype lookup buckets whose record order changed.
		using EntityToArchetypeVersionMap = cnt::map<EntityLookupKey, uint32_t>;

		//! Sentinel indicating that a component column index is unavailable.
		static constexpr uint16_t ComponentIndexBad = (uint16_t)-1;

		//! One archetype record stored in the entity-to-archetype reverse index.
		struct ComponentIndexEntry {
			//! Archetype matched by the indexed entity or pair key.
			Archetype* pArchetype = nullptr;
			//! Component column index in \a pArchetype, or ComponentIndexBad when no direct column exists.
			uint16_t compIdx = ComponentIndexBad;
			//! Number of archetype components contributing this lookup-key match.
			uint16_t matchCount = 0;

			//! Tests whether this record belongs to an archetype.
			//! \param pOther Archetype pointer to compare.
			//! \return True when \a pOther equals pArchetype.
			GAIA_NODISCARD bool matches(const Archetype* pOther) const {
				return pArchetype == pOther;
			}
		};

		//! Small reverse-lookup bucket for entity-to-archetype matches.
		//! Most buckets contain one archetype, so keep that record inline and spill only when needed.
		class ComponentIndexEntryArray {
		public:
			//! Stored reverse-index record type.
			using value_type = ComponentIndexEntry;
			//! Unsigned element-count and index type.
			using size_type = uint32_t;

		private:
			ComponentIndexEntry m_inline{};
			cnt::darray<ComponentIndexEntry> m_items;
			size_type m_size = 0;

			void collapse_to_inline_if_needed() {
				if (m_size != 1 || m_items.empty())
					return;

				m_inline = m_items[0];
				m_items.clear();
			}

		public:
			ComponentIndexEntryArray() = default;
			ComponentIndexEntryArray(ComponentIndexEntryArray&&) noexcept = default;
			ComponentIndexEntryArray(const ComponentIndexEntryArray&) = default;
			ComponentIndexEntryArray& operator=(ComponentIndexEntryArray&&) noexcept = default;
			ComponentIndexEntryArray& operator=(const ComponentIndexEntryArray&) = default;

			//! Checks whether the bucket contains no reverse-index records.
			//! \return True when size is zero.
			GAIA_NODISCARD bool empty() const noexcept {
				return m_size == 0;
			}

			//! Returns the number of reverse-index records.
			//! \return Number of stored records.
			GAIA_NODISCARD size_type size() const noexcept {
				return m_size;
			}

			//! Returns mutable contiguous record storage.
			//! \return Pointer to the inline record or spill storage.
			GAIA_NODISCARD ComponentIndexEntry* data() noexcept {
				return m_size <= 1 ? &m_inline : m_items.data();
			}

			//! Returns read-only contiguous record storage.
			//! \return Pointer to the inline record or spill storage.
			GAIA_NODISCARD const ComponentIndexEntry* data() const noexcept {
				return m_size <= 1 ? &m_inline : m_items.data();
			}

			//! Returns a mutable iterator to the first record.
			//! \return Pointer to the first stored record.
			GAIA_NODISCARD ComponentIndexEntry* begin() noexcept {
				return data();
			}

			//! Returns a read-only iterator to the first record.
			//! \return Pointer to the first stored record.
			GAIA_NODISCARD const ComponentIndexEntry* begin() const noexcept {
				return data();
			}

			//! Returns a mutable iterator past the final record.
			//! \return Pointer one past the last stored record.
			GAIA_NODISCARD ComponentIndexEntry* end() noexcept {
				return data() + m_size;
			}

			//! Returns a read-only iterator past the final record.
			//! \return Pointer one past the last stored record.
			GAIA_NODISCARD const ComponentIndexEntry* end() const noexcept {
				return data() + m_size;
			}

			//! Returns a mutable record by index.
			//! \param idx Zero-based record index.
			//! \return Record at \a idx.
			GAIA_NODISCARD ComponentIndexEntry& operator[](size_type idx) noexcept {
				GAIA_ASSERT(idx < m_size);
				return data()[idx];
			}

			//! Returns a read-only record by index.
			//! \param idx Zero-based record index.
			//! \return Record at \a idx.
			GAIA_NODISCARD const ComponentIndexEntry& operator[](size_type idx) const noexcept {
				GAIA_ASSERT(idx < m_size);
				return data()[idx];
			}

			//! Returns the final mutable record.
			//! \return Last record in the non-empty bucket.
			GAIA_NODISCARD ComponentIndexEntry& back() noexcept {
				GAIA_ASSERT(m_size > 0);
				return (*this)[m_size - 1];
			}

			//! Returns the final read-only record.
			//! \return Last record in the non-empty bucket.
			GAIA_NODISCARD const ComponentIndexEntry& back() const noexcept {
				GAIA_ASSERT(m_size > 0);
				return (*this)[m_size - 1];
			}

			//! Appends a reverse-index record, spilling inline storage when necessary.
			//! \param entry Record to append.
			void push_back(ComponentIndexEntry entry) {
				if (m_size == 0) {
					m_inline = entry;
					m_size = 1;
					return;
				}

				if (m_size == 1) {
					m_items.push_back(m_inline);
					m_items.push_back(entry);
					m_size = 2;
					return;
				}

				m_items.push_back(entry);
				++m_size;
			}

			//! Removes the final record and restores inline storage when one record remains.
			void pop_back() {
				GAIA_ASSERT(m_size > 0);
				if (m_size == 1) {
					m_size = 0;
					return;
				}

				m_items.pop_back();
				--m_size;
				collapse_to_inline_if_needed();
			}
		};

		//! One lookup-key record collected while indexing a single archetype.
		struct SingleArchetypeLookupItem {
			//! Entity or wildcard-pair lookup key represented by this record.
			EntityLookupKey key = EntityBadLookupKey;
			//! Reverse-index data associated with \a key.
			ComponentIndexEntry entry{};

			//! Tests whether this item represents a lookup key.
			//! \param other Lookup key to compare.
			//! \return True when \a other equals key.
			GAIA_NODISCARD bool matches(EntityLookupKey other) const {
				return key == other;
			}
		};

		//! Exact-id and wildcard-derived lookup records generated for one archetype.
		using SingleArchetypeLookup = cnt::sarray_ext<SingleArchetypeLookupItem, ChunkHeader::MAX_COMPONENTS * 4>;

		//! Sentinel identifying an invalid query slot.
		static constexpr QueryId QueryIdBad = (QueryId)-1;
		//! Greatest group identifier available to query grouping callbacks.
		static constexpr GroupId GroupIdMax = ((GroupId)-1) - 1;

		//! Stable query-slot identifier combining a slot index and generation.
		struct QueryHandle {
			//! Bit mask spanning the query-id portion of a packed handle.
			static constexpr uint32_t IdMask = QueryIdBad;

		private:
			struct HandleData {
				QueryId id;
				uint32_t gen;
			};

			union {
				HandleData data;
				uint64_t val;
			};

		public:
			constexpr QueryHandle() noexcept: val((uint64_t)-1) {};

			//! Constructs a handle from query slot metadata.
			//! \param id Query slot identifier.
			//! \param gen Query slot generation.
			QueryHandle(QueryId id, uint32_t gen) {
				data.id = id;
				data.gen = gen;
			}
			~QueryHandle() = default;

			QueryHandle(QueryHandle&&) noexcept = default;
			QueryHandle(const QueryHandle&) = default;
			QueryHandle& operator=(QueryHandle&&) noexcept = default;
			QueryHandle& operator=(const QueryHandle&) = default;

			//! Compares packed query handles.
			//! \param other Handle to compare.
			//! \return True when slot identifier and generation match.
			GAIA_NODISCARD constexpr bool operator==(const QueryHandle& other) const noexcept {
				return val == other.val;
			}
			//! Compares packed query handles for inequality.
			//! \param other Handle to compare.
			//! \return True when slot identifier or generation differs.
			GAIA_NODISCARD constexpr bool operator!=(const QueryHandle& other) const noexcept {
				return val != other.val;
			}

			//! Returns the query slot identifier.
			//! \return Query slot identifier stored in the handle.
			GAIA_NODISCARD auto id() const {
				return data.id;
			}
			//! Returns the query slot generation.
			//! \return Generation stored in the handle.
			GAIA_NODISCARD auto gen() const {
				return data.gen;
			}
			//! Returns the packed handle representation.
			//! \return Combined query identifier and generation bits.
			GAIA_NODISCARD auto value() const {
				return val;
			}
		};

		//! Invalid query handle sentinel.
		inline static const QueryHandle QueryHandleBad = QueryHandle();

		//! Hashmap lookup structure used for Entity
		struct QueryHandleLookupKey {
			//! Direct hash value cached for the packed query handle.
			using LookupHash = core::direct_hash_key<uint64_t>;

		private:
			//! Entity
			QueryHandle m_handle;
			//! Entity hash
			LookupHash m_hash;

			static LookupHash calc(QueryHandle handle) {
				return {core::calculate_hash64(handle.value())};
			}

		public:
			//! Indicates that Gaia containers may consume hash() without rehashing the key.
			static constexpr bool IsDirectHashKey = true;

			QueryHandleLookupKey() = default;
			//! Constructs a lookup key and precomputes its hash.
			//! \param handle Query handle represented by the key.
			explicit QueryHandleLookupKey(QueryHandle handle): m_handle(handle), m_hash(calc(handle)) {}
			~QueryHandleLookupKey() = default;

			QueryHandleLookupKey(const QueryHandleLookupKey&) = default;
			QueryHandleLookupKey(QueryHandleLookupKey&&) = default;
			QueryHandleLookupKey& operator=(const QueryHandleLookupKey&) = default;
			QueryHandleLookupKey& operator=(QueryHandleLookupKey&&) = default;

			//! Returns the represented query handle.
			//! \return Query handle stored by the lookup key.
			QueryHandle handle() const {
				return m_handle;
			}

			//! Returns the precomputed container hash.
			//! \return Hash of the packed query handle.
			size_t hash() const {
				return (size_t)m_hash.hash;
			}

			//! Compares lookup keys using their cached hashes and handles.
			//! \param other Lookup key to compare.
			//! \return True when both keys represent the same query handle.
			bool operator==(const QueryHandleLookupKey& other) const {
				if GAIA_LIKELY (m_hash != other.m_hash)
					return false;

				return m_handle == other.m_handle;
			}

			//! Compares lookup keys for inequality.
			//! \param other Lookup key to compare.
			//! \return True when the represented handles differ.
			bool operator!=(const QueryHandleLookupKey& other) const {
				return !operator==(other);
			}
		};

		//! Invalid query-handle lookup-key sentinel.
		inline static const QueryHandleLookupKey QueryHandleBadLookupKey = QueryHandleLookupKey(QueryHandleBad);

		//! User-provided query input
		struct QueryInput {
			//! Traversal-depth value selecting the internally bounded unlimited mode.
			static constexpr uint8_t TravDepthUnlimited = 0;

			//! Operation to perform with the input
			QueryOpKind op = QueryOpKind::All;
			//! Access type
			QueryAccess access = QueryAccess::Read;
			//! Entity/Component/Pair to query
			Entity id;
			//! Source entity to query the id on.
			//! If id==EntityBad the source is fixed.
			//! If id!=src the source is variable.
			Entity entSrc = EntityBad;
			//! Optional traversal relation for source lookups.
			//! When set, the lookup starts at src and then walks relation targets upwards and/or downwards.
			Entity entTrav = EntityBad;
			//! Source traversal filter.
			//! `Self` means checking the source itself, `Up` means checking traversed ancestors,
			//! `Down` means checking traversed descendants.
			QueryTravKind travKind = QueryTravKind::Self | QueryTravKind::Up;
			//! Maximum number of traversal steps.
			//! 0 means unlimited traversal depth (bounded internally, at most MAX_TRAV_DEPTH steps).
			uint8_t travDepth = TravDepthUnlimited;
			//! Match semantics for terms with special meaning, such as `Pair(Is, X)`.
			QueryMatchKind matchKind = QueryMatchKind::Semantic;
		};

		//! Additional options for query terms.
		//! This can be used to configure source lookup, traversal and access mode
		//! without relying on many positional overloads.
		struct QueryTermOptions {
			//! Traversal-depth value selecting the internally bounded unlimited mode.
			static constexpr uint8_t TravDepthUnlimited = QueryInput::TravDepthUnlimited;

			//! Source entity to query from.
			Entity entSrc = EntityBad;
			//! Optional traversal relation used for source lookup.
			Entity entTrav = EntityBad;
			//! Source traversal filter.
			QueryTravKind travKind = QueryTravKind::Self | QueryTravKind::Up;
			//! Maximum number of traversal steps.
			//! 0 means unlimited traversal depth (bounded internally, at most MAX_TRAV_DEPTH steps).
			uint8_t travDepth = TravDepthUnlimited;
			//! Access mode for the term.
			//! When None, typed query terms infer read/write access from template mutability.
			QueryAccess access = QueryAccess::None;
			//! Match semantics for terms with special meaning, such as `Pair(Is, X)`.
			QueryMatchKind matchKind = QueryMatchKind::Semantic;

			//! Selects a fixed or variable source entity for the term.
			//! \param source Source entity or variable identifier.
			//! \return This options object.
			QueryTermOptions& src(Entity source) {
				entSrc = source;
				return *this;
			}

			//! Traverses the source and its relation targets without a user depth limit.
			//! \param relation Relation to traverse.
			//! \return This options object.
			QueryTermOptions& trav(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Self | QueryTravKind::Up;
				travDepth = TravDepthUnlimited;
				return *this;
			}

			//! Traverses relation targets without checking the source itself.
			//! \param relation Relation to traverse.
			//! \return This options object.
			QueryTermOptions& trav_up(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Up;
				travDepth = TravDepthUnlimited;
				return *this;
			}

			//! Checks only the immediate relation target of the source.
			//! \param relation Relation to traverse.
			//! \return This options object.
			QueryTermOptions& trav_parent(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Up;
				travDepth = 1;
				return *this;
			}

			//! Checks the source and its immediate relation target.
			//! \param relation Relation to traverse.
			//! \return This options object.
			QueryTermOptions& trav_self_parent(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Self | QueryTravKind::Up;
				travDepth = 1;
				return *this;
			}

			//! Traverses entities targeting the source without a user depth limit.
			//! \param relation Relation to traverse in reverse.
			//! \return This options object.
			QueryTermOptions& trav_down(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Down;
				travDepth = TravDepthUnlimited;
				return *this;
			}

			//! Checks the source and recursively traverses entities targeting it.
			//! \param relation Relation to traverse in reverse.
			//! \return This options object.
			QueryTermOptions& trav_self_down(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Self | QueryTravKind::Down;
				travDepth = TravDepthUnlimited;
				return *this;
			}

			//! Checks only immediate entities targeting the source.
			//! \param relation Relation to traverse in reverse.
			//! \return This options object.
			QueryTermOptions& trav_child(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Down;
				travDepth = 1;
				return *this;
			}

			//! Checks the source and immediate entities targeting it.
			//! \param relation Relation to traverse in reverse.
			//! \return This options object.
			QueryTermOptions& trav_self_child(Entity relation = ChildOf) {
				entTrav = relation;
				travKind = QueryTravKind::Self | QueryTravKind::Down;
				travDepth = 1;
				return *this;
			}

			//! Replaces the traversal filter without changing its relation or depth.
			//! \param kind Traversal locations to inspect.
			//! \return This options object.
			QueryTermOptions& trav_kind(QueryTravKind kind) {
				travKind = kind;
				return *this;
			}

			//! Sets the maximum traversal distance.
			//! \param maxDepth Maximum steps, or TravDepthUnlimited for the internal limit.
			//! \return This options object.
			QueryTermOptions& trav_depth(uint8_t maxDepth) {
				travDepth = maxDepth;
				return *this;
			}

			//! Requests read-only access to the term.
			//! \return This options object.
			QueryTermOptions& read() {
				access = QueryAccess::Read;
				return *this;
			}

			//! Requests mutable access to the term.
			//! \return This options object.
			QueryTermOptions& write() {
				access = QueryAccess::Write;
				return *this;
			}

			//! Restricts the term to direct storage matches.
			//! \return This options object.
			QueryTermOptions& direct() {
				matchKind = QueryMatchKind::Direct;
				return *this;
			}

			//! Allows direct and inherited matches for the term.
			//! \return This options object.
			QueryTermOptions& in() {
				matchKind = QueryMatchKind::In;
				return *this;
			}
		};

		//! Explicit component/entity access declarations used for scheduling decisions.
		//!
		//! Queries derive read/write access from their positive query terms. This small side-band set stores accesses that
		//! are not part of the query shape, for example a component fetched manually inside the query callback.
		//!
		//! \note This metadata is intentionally not part of query identity. It does not affect query hashing, matching,
		//! cache sharing, or cache invalidation.
		struct QueryAccessSet {
			//! Component/entity ids read by the callback outside the query terms.
			cnt::sarray<Entity, MAX_ITEMS_IN_QUERY> reads;
			//! Component/entity ids written by the callback outside the query terms.
			cnt::sarray<Entity, MAX_ITEMS_IN_QUERY> writes;
			//! Number of valid entries in reads.
			uint8_t readCnt = 0;
			//! Number of valid entries in writes.
			uint8_t writeCnt = 0;

			//! Returns the explicitly declared read ids.
			//! \return Read-only span over explicit read ids.
			GAIA_NODISCARD std::span<const Entity> reads_view() const {
				return {reads.data(), readCnt};
			}

			//! Returns the explicitly declared write ids.
			//! \return Read-only span over explicit write ids.
			GAIA_NODISCARD std::span<const Entity> writes_view() const {
				return {writes.data(), writeCnt};
			}

			//! Declares that an id is read.
			//! \param entity Component/entity id read by user code.
			void add_read(Entity entity) {
				if (entity == EntityBad || core::has(reads_view(), entity))
					return;

				GAIA_ASSERT(readCnt < MAX_ITEMS_IN_QUERY);
				if (readCnt < MAX_ITEMS_IN_QUERY)
					reads[readCnt++] = entity;
			}

			//! Declares that an id is written.
			//! \param entity Component/entity id written by user code.
			void add_write(Entity entity) {
				if (entity == EntityBad || core::has(writes_view(), entity))
					return;

				GAIA_ASSERT(writeCnt < MAX_ITEMS_IN_QUERY);
				if (writeCnt < MAX_ITEMS_IN_QUERY)
					writes[writeCnt++] = entity;
			}

			//! Returns explicitly declared access for an id.
			//! \param entity Component/entity id to look up.
			//! \return Write if explicitly written, Read if explicitly read, None otherwise.
			GAIA_NODISCARD QueryAccess access(Entity entity) const {
				if (core::has(writes_view(), entity))
					return QueryAccess::Write;
				if (core::has(reads_view(), entity))
					return QueryAccess::Read;
				return QueryAccess::None;
			}
		};

		//! Internal representation of QueryInput
		struct QueryTerm {
			//! Queried id
			Entity id;
			//! Source of where the queried id is looked up at
			Entity src;
			//! Optional traversal relation for source lookups
			Entity entTrav;
			//! Source traversal filter.
			QueryTravKind travKind;
			//! Maximum number of traversal steps.
			uint8_t travDepth;
			//! Match semantics for this term.
			QueryMatchKind matchKind;
			//! Archetype of the src entity
			Archetype* srcArchetype;
			//! Operation to perform with the term
			QueryOpKind op;
			//! Stable execution field index matching the user-defined query field order.
			uint8_t fieldIndex = 0;

			//! Compares the matching identity of two compiled terms.
			//! \param other Term to compare.
			//! \return True when matching-relevant fields are equal.
			bool operator==(const QueryTerm& other) const {
				return id == other.id && src == other.src && entTrav == other.entTrav && travKind == other.travKind &&
							 travDepth == other.travDepth && matchKind == other.matchKind && op == other.op;
			}
			//! Compares the matching identity of two compiled terms for inequality.
			//! \param other Term to compare.
			//! \return True when a matching-relevant field differs.
			bool operator!=(const QueryTerm& other) const {
				return !operator==(other);
			}
		};

		//! Orders compiled terms into their canonical query-lookup representation.
		//! \param lhs First term.
		//! \param rhs Second term.
		//! \return True when \a lhs precedes \a rhs in lookup order.
		constexpr bool query_term_less_for_lookup(const QueryTerm& lhs, const QueryTerm& rhs) {
			if (lhs.op != rhs.op)
				return lhs.op < rhs.op;

			if (lhs.id != rhs.id)
				return SortComponentCond()(lhs.id, rhs.id);

			if (lhs.src != rhs.src)
				return SortComponentCond()(lhs.src, rhs.src);

			if (lhs.entTrav != rhs.entTrav)
				return SortComponentCond()(lhs.entTrav, rhs.entTrav);

			if (lhs.travKind != rhs.travKind)
				return (uint8_t)lhs.travKind < (uint8_t)rhs.travKind;

			if (lhs.travDepth != rhs.travDepth)
				return lhs.travDepth < rhs.travDepth;

			return (uint8_t)lhs.matchKind < (uint8_t)rhs.matchKind;
		}

		//! Canonicalizes terms for query identity hashing and equality.
		//! \param terms Mutable query terms to normalize and sort.
		inline void canonicalize_lookup_terms(std::span<QueryTerm> terms) {
			const auto idsCnt = (uint32_t)terms.size();
			if (idsCnt > 0) {
				uint32_t orCnt = 0;
				uint32_t orIdx = BadIndex;
				GAIA_FOR(idsCnt) {
					if (terms[i].op != QueryOpKind::Or)
						continue;
					++orCnt;
					orIdx = i;
					if (orCnt > 1)
						break;
				}

				if (orCnt == 1)
					terms[orIdx].op = QueryOpKind::All;
			}

			core::sort(terms.data(), terms.data() + idsCnt, [](const QueryTerm& left, const QueryTerm& right) {
				return query_term_less_for_lookup(left, right);
			});
		}

		//! Sorts changed-filter identifiers into canonical query-identity order.
		//! \param changed Mutable changed-filter identifiers.
		inline void canonicalize_lookup_changed(std::span<Entity> changed) {
			const auto changedCnt = (uint32_t)changed.size();
			if (changedCnt > 1)
				core::sort(changed.data(), changed.data() + changedCnt, SortComponentCond{});
		}

		//! Checks whether a compiled term contains a source or id variable.
		//! \param term Compiled query term.
		//! \return True when any source, relation, or target identifier is variable.
		GAIA_NODISCARD inline bool term_has_variables(const QueryTerm& term) {
			if (is_variable(term.src))
				return true;

			if (term.id.pair())
				return is_variable(EntityId(term.id.id())) || is_variable(EntityId(term.id.gen()));

			return is_variable(EntityId(term.id.id()));
		}

		//! Checks whether a query term may map to a column owned by the currently iterated archetype.
		//! \param term Query term.
		//! \return True for non-variable self-source terms without traversal.
		GAIA_NODISCARD inline bool query_term_maps_to_current_archetype(const QueryTerm& term) {
			return term.src == EntityBad && term.entTrav == EntityBad && !term_has_variables(term);
		}

		//! Returns whether a term shape can resolve through inherited-id matching.
		//! This ignores mutable world metadata such as the current OnInstantiate policy.
		//! \param term Query term.
		//! \return True if the term shape can use inherited-id matching.
		GAIA_NODISCARD inline bool query_term_uses_potential_inherited_id_matching(const QueryTerm& term) {
			const auto id = term.id;
			return term.matchKind == QueryMatchKind::Semantic && term.src == EntityBad && term.entTrav == EntityBad &&
						 !term_has_variables(term) && !is_wildcard(id) && !is_variable((EntityId)id.id()) &&
						 (!id.pair() || !is_variable((EntityId)id.gen()));
		}

		//! Fixed-capacity compiled query-term sequence.
		using QueryTermArray = cnt::sarray_ext<QueryTerm, MAX_ITEMS_IN_QUERY>;
		//! Mutable view over compiled query terms.
		using QueryTermSpan = std::span<QueryTerm>;
		//! Fixed-capacity mapping between canonical and user query-field indices.
		using QueryRemappingArray = cnt::sarray_ext<uint8_t, MAX_ITEMS_IN_QUERY>;

		//! Slot and serialization identity owned by a compiled query context.
		struct QueryIdentity {
			//! Query id
			QueryHandle handle = {};
			//! Serialization id
			QueryId serId = QueryIdBad;

			//! Returns this query's serialization buffer in a world.
			//! \param world World owning the query serialization map.
			//! \return Mutable serialization buffer associated with serId.
			GAIA_NODISCARD QuerySerBuffer& ser_buffer(World* world) {
				return query_buffer(*world, serId);
			}
			//! Resets this query's serialization buffer in a world.
			//! \param world World owning the query serialization map.
			void ser_buffer_reset(World* world) {
				query_buffer_reset(*world, serId);
			}
		};

		//! Authored and compiled state defining query identity and execution behavior.
		struct QueryCtx {
			//! World against which the query is compiled and executed.
			const World* w{};
			//! Component cache
			ComponentCache* cc{};
			//! Lookup hash for this query
			QueryLookupHash hashLookup{};
			//! Query identity
			QueryIdentity q{};

			//! Query maintenance and execution flags derived during compilation.
			enum QueryFlags : uint16_t {
				//! No query flags are set.
				Empty = 0x00,
				//! Cached entity slices require sorting.
				SortEntities = 0x01,
				//! Cached group ranges require sorting.
				SortGroups = 0x02,
				//! Query requires the general matching path.
				Complex = 0x04,
				//! VM opcode recompilation is pending.
				Recompile = 0x08,
				//! Query contains fixed-source lookup terms.
				HasSourceTerms = 0x10,
				//! Query contains variable-based lookup terms.
				HasVariableTerms = 0x20,
				//! Includes prefab entities without requiring an explicit Prefab term.
				MatchPrefab = 0x40,
				//! Query explicitly mentions Prefab and therefore bypasses automatic exclusion.
				HasPrefabTerms = 0x80,
				//! Grouped archetypes are ordered by group identifier during cache refresh.
				OrderGroups = 0x100,
			};

			//! Strategy used to maintain cached archetype matches.
			enum class CachePolicy : uint8_t {
				//! Updates a structural query immediately when an archetype is created.
				Immediate,
				//! Refreshes a structural query lazily on the next read.
				Lazy,
				//! Repairs source- or variable-dependent cached state on demand.
				Dynamic,
			};

			//! Matcher selected for newly created archetypes.
			enum class CreateArchetypeMatchKind : uint8_t {
				//! Uses the normal one-archetype VM path.
				Vm,
				//! Evaluates a small immediate ALL, OR, and NOT query directly on the archetype.
				DirectStructuralTerms,
			};

			//! Dynamic-cache dependency shape derived from compiled query metadata.
			enum class DynamicCacheKind : uint8_t {
				//! Query does not use dynamic-cache validation.
				None,
				//! Dynamic cache is invalidated by relation version dependencies only.
				RelationOnly,
				//! Dynamic cache tracks concrete source entity archetype versions.
				DirectSource,
				//! Dynamic cache tracks a traversed source closure.
				TraversedSource,
				//! Dynamic cache tracks runtime variable bindings only.
				Variable,
				//! Dynamic cache tracks more than one dependency family.
				Mixed
			};

			//! Specialized evaluation shape for concrete target entities.
			enum class DirectTargetEvalKind : uint8_t {
				//! Uses the general compiled query evaluator.
				Generic,
				//! Evaluates one required direct-storage term.
				SingleAllDirect,
				//! Evaluates one required semantic Is term.
				SingleAllSemanticIs,
				//! Evaluates one required inherited-inclusive Is term.
				SingleAllInIs,
				//! Evaluates one required term through inherited component data.
				SingleAllInherited,
			};

			//! Dependency facts derived from the compiled term set.
			enum DependencyFlags : uint16_t {
				//! No dependency facts are present.
				DependencyNone = 0x00,
				//! At least one term uses a fixed source entity.
				DependencyHasSourceTerms = 0x01,
				//! At least one term contains a runtime variable.
				DependencyHasVariableTerms = 0x02,
				//! At least one positive ALL or OR term is present.
				DependencyHasPositiveTerms = 0x04,
				//! At least one negative NOT term is present.
				DependencyHasNegativeTerms = 0x08,
				//! At least one optional ANY term is present.
				DependencyHasAnyTerms = 0x10,
				//! At least one wildcard id or pair term is present.
				DependencyHasWildcardTerms = 0x20,
				//! Query has an entity sorting callback.
				DependencyHasSort = 0x40,
				//! Query has an archetype grouping callback.
				DependencyHasGroup = 0x80,
				//! At least one source term traverses a relation.
				DependencyHasTraversalTerms = 0x100,
				//! At least one term requires per-entity filtering.
				DependencyHasEntityFilterTerms = 0x200,
				//! Iteration requires cached inherited component pointers.
				DependencyHasInheritedDataTerms = 0x400,
				//! A term shape may resolve through inherited-id matching.
				DependencyHasPotentialInheritedIdTerms = 0x800,
			};

			//! Compact compiled query payload used by matching, identity, and cache maintenance.
			struct Data {
				//! Deduplicated entities and flags that can invalidate or update a query cache.
				struct Dependencies {
					//! Positive selector ids used for archetype-create propagation.
					QueryEntityArray createSelectors;
					//! Negative selector ids used to reject new archetypes.
					QueryEntityArray exclusions;
					//! Relations whose topology versions affect cached results.
					QueryEntityArray relations;
					//! Concrete source entities whose archetype versions affect cached results.
					QueryEntityArray sourceEntities;
					//! Number of valid entries in createSelectors.
					uint8_t createSelectorCnt = 0;
					//! Number of valid entries in exclusions.
					uint8_t exclusionCnt = 0;
					//! Number of valid entries in relations.
					uint8_t relationCnt = 0;
					//! Number of valid entries in sourceEntities.
					uint8_t sourceEntityCnt = 0;
					//! Number of fixed-source terms, including duplicate source entities.
					uint8_t sourceTermCnt = 0;
					//! Combined DependencyFlags describing the compiled query shape.
					DependencyFlags flags = DependencyNone;

					//! Resets dependency counts and flags while retaining fixed storage.
					void clear() {
						createSelectorCnt = 0;
						exclusionCnt = 0;
						relationCnt = 0;
						sourceEntityCnt = 0;
						sourceTermCnt = 0;
						flags = DependencyNone;
					}

					//! Returns positive selector ids used during archetype creation.
					//! \return Read-only view over valid createSelectors entries.
					GAIA_NODISCARD std::span<const Entity> create_selectors_view() const {
						return {createSelectors.data(), createSelectorCnt};
					}

					//! Returns negative selector ids used during archetype creation.
					//! \return Read-only view over valid exclusions entries.
					GAIA_NODISCARD std::span<const Entity> exclusions_view() const {
						return {exclusions.data(), exclusionCnt};
					}

					//! Returns relation-version dependencies.
					//! \return Read-only view over valid relations entries.
					GAIA_NODISCARD std::span<const Entity> relations_view() const {
						return {relations.data(), relationCnt};
					}

					//! Returns concrete source-entity dependencies.
					//! \return Read-only view over valid sourceEntities entries.
					GAIA_NODISCARD std::span<const Entity> src_entities_view() const {
						return {sourceEntities.data(), sourceEntityCnt};
					}

					//! Records a dependency fact.
					//! \param dependency Flag to add to flags.
					void set_dep_flag(DependencyFlags dependency) {
						flags = (DependencyFlags)(flags | dependency);
					}

					//! Tests whether a dependency fact was recorded.
					//! \param dependency Flag to test.
					//! \return True when \a dependency is set in flags.
					GAIA_NODISCARD bool has_dep_flag(DependencyFlags dependency) const {
						return (flags & dependency) != 0;
					}

					//! Adds a unique relation-version dependency.
					//! \param relation Relation whose topology affects query results.
					void add_rel(Entity relation) {
						if (relation == EntityBad || core::has(relations_view(), relation))
							return;

						GAIA_ASSERT(relationCnt < MAX_ITEMS_IN_QUERY);
						relations[relationCnt++] = relation;
					}

					//! Adds a unique concrete source-entity dependency.
					//! \param entity Source entity whose archetype membership affects results.
					void add_src_entity(Entity entity) {
						if (entity == EntityBad || core::has(src_entities_view(), entity))
							return;

						GAIA_ASSERT(sourceEntityCnt < MAX_ITEMS_IN_QUERY);
						sourceEntities[sourceEntityCnt++] = entity;
					}

					//! Checks whether each fixed-source term has a distinct tracked entity.
					//! \return True when targeted source-version checks can validate the cache.
					GAIA_NODISCARD bool can_reuse_src_cache() const {
						return sourceTermCnt > 0 && sourceTermCnt == sourceEntityCnt;
					}
				};

				//! Cold canonical payload used exclusively by shared-query hashing and equality.
				struct LookupIdentity {
					//! Canonicalized lookup terms reused by hash/equality for shared query dedup.
					cnt::sarray<QueryTerm, MAX_ITEMS_IN_QUERY> lookupTerms;
					//! Canonicalized changed-filter ids reused by hash/equality for shared query dedup.
					QueryEntityArray changed;
					//! Canonicalized group dependency ids reused by hash/equality for shared query dedup.
					QueryEntityArray groupDeps;
				};

				//! Array of queried ids
				QueryEntityArray ids;
				//! Array of terms
				cnt::sarray<QueryTerm, MAX_ITEMS_IN_QUERY> terms;
				//! Index of the last checked archetype in the component-to-archetype map
				QueryArchetypeCacheIndexMap lastMatchedArchetypeIdx_All;
				//! Incremental lookup cursors for OR selector terms.
				QueryArchetypeCacheIndexMap lastMatchedArchetypeIdx_Or;
				//! Incremental lookup cursors for NOT selector terms.
				QueryArchetypeCacheIndexMap lastMatchedArchetypeIdx_Not;
				//! Number of valid ids and terms in the fixed-capacity query arrays.
				uint8_t idsCnt = 0;
				//! Number of valid changed-filter ids and field mappings.
				uint8_t changedCnt = 0;
				//! Array of filtered components
				QueryEntityArray changed;
				//! Query term index for each changed-filter component after query canonicalization.
				cnt::sarray<uint8_t, MAX_ITEMS_IN_QUERY> changedFields;
				//! Explicit grouping invalidation dependencies for custom group_by callbacks.
				QueryEntityArray groupDeps;
				//! Entity to sort the archetypes by. EntityBad for no sorting.
				Entity sortBy;
				//! Function to use to perform sorting
				TSortByFunc sortByFunc;
				//! Entity to group the archetypes by. EntityBad for no grouping.
				Entity groupBy;
				//! Function to use to perform the grouping
				TGroupByFunc groupByFunc;
				//! Component mask used for faster matching of simple queries
				QueryMask queryMask;
				//! Mask for items with Is relationship pair.
				//! If the id is a pair, the first part (id) is written here.
				uint32_t as_mask_0;
				//! Mask for items with Is relationship pair.
				//! If the id is a pair, the second part (gen) is written here.
				uint32_t as_mask_1;
				//! First NOT record in pairs/ids/ops
				uint8_t firstNot;
				//! First ANY record in pairs/ids/ops
				uint8_t firstAny;
				//! First OR record in pairs/ids/ops
				uint8_t firstOr;
				//! Number of defined group dependencies
				uint8_t groupDepCnt = 0;
				//! Read-write mask. Bit 0 stands for component 0 in component arrays.
				//! A set bit means write access is requested.
				uint16_t readWriteMask;
				//! Query flags
				uint16_t flags;
				//! Maximum allowed size of an explicitly cached traversed-source lookup closure.
				uint16_t cacheSrcTrav = 0;
				//! Specialized direct-target evaluation shape for single-term queries.
				DirectTargetEvalKind directTargetEvalKind = DirectTargetEvalKind::Generic;
				//! Term id used by the specialized direct-target evaluation shape.
				Entity directTargetEvalId = EntityBad;
				//! True when the query can evaluate concrete target entities directly.
				bool canDirectTargetEval = false;
				//! True when the query shape is eligible for direct entity seed evaluation.
				bool canDirectEntitySeedEvalShape = false;
				//! True when the query contains only direct OR/NOT terms and at least one OR term.
				bool hasOnlyDirectOrTerms = false;
				//! True when a dynamic cache can be reused by checking tracked runtime inputs.
				bool canReuseDynamicCache = false;
				//! Explicit dependency metadata derived from query shape.
				Dependencies deps;
				//! Cache maintenance policy derived from query shape.
				CachePolicy cachePolicy = CachePolicy::Lazy;
				//! Create-time archetype matcher derived from query shape.
				CreateArchetypeMatchKind createArchetypeMatchKind = CreateArchetypeMatchKind::Vm;
				//! Dynamic-cache dependency shape derived from compiled query metadata.
				DynamicCacheKind dynamicCacheKind = DynamicCacheKind::None;
				//! Cold canonical shared-query identity payload.
				LookupIdentity lookupIdentity;

				//! Returns authored query ids in canonical execution order.
				//! \return Read-only view over valid ids entries.
				GAIA_NODISCARD std::span<const Entity> ids_view() const {
					return {ids.data(), idsCnt};
				}

				//! Returns changed-filter component ids.
				//! \return Read-only view over valid changed entries.
				GAIA_NODISCARD std::span<const Entity> changed_view() const {
					return {changed.data(), changedCnt};
				}

				//! Returns query-term indices matching changed-filter components.
				//! \return Read-only field-index view aligned with changed_view().
				GAIA_NODISCARD std::span<const uint8_t> changed_fields_view() const {
					return {changedFields.data(), changedCnt};
				}

				//! Returns explicit grouping invalidation dependencies.
				//! \return Read-only view over valid groupDeps entries.
				GAIA_NODISCARD std::span<const Entity> group_deps_view() const {
					return {groupDeps.data(), groupDepCnt};
				}

				//! Returns canonicalized lookup terms used by shared query deduplication.
				//! \return Read-only canonical term view.
				GAIA_NODISCARD std::span<const QueryTerm> lookup_terms_view() const {
					return {lookupIdentity.lookupTerms.data(), idsCnt};
				}

				//! Returns mutable canonicalized lookup terms used by shared query deduplication.
				//! \return Mutable canonical term view.
				GAIA_NODISCARD std::span<QueryTerm> lookup_terms_view_mut() {
					return {lookupIdentity.lookupTerms.data(), idsCnt};
				}

				//! Returns canonicalized changed-filter lookup ids used by shared query deduplication.
				//! \return Read-only canonical changed-filter view.
				GAIA_NODISCARD std::span<const Entity> changed_lookup_view() const {
					return {lookupIdentity.changed.data(), changedCnt};
				}

				//! Returns mutable canonicalized changed-filter lookup ids used by shared query deduplication.
				//! \return Mutable canonical changed-filter view.
				GAIA_NODISCARD std::span<Entity> changed_lookup_view_mut() {
					return {lookupIdentity.changed.data(), changedCnt};
				}

				//! Returns canonicalized group dependency lookup ids used by shared query deduplication.
				//! \return Read-only canonical grouping-dependency view.
				GAIA_NODISCARD std::span<const Entity> group_deps_lookup_view() const {
					return {lookupIdentity.groupDeps.data(), groupDepCnt};
				}

				//! Returns mutable canonicalized group dependency lookup ids used by shared query deduplication.
				//! \return Mutable canonical grouping-dependency view.
				GAIA_NODISCARD std::span<Entity> group_deps_lookup_view_mut() {
					return {lookupIdentity.groupDeps.data(), groupDepCnt};
				}

				//! Adds a declared grouping invalidation dependency.
				//! \param relation Relation whose topology can change group assignment.
				void add_group_dep(Entity relation) {
					if (relation == EntityBad || core::has(group_deps_view(), relation))
						return;

					GAIA_ASSERT(groupDepCnt < MAX_ITEMS_IN_QUERY);
					if (groupDepCnt < MAX_ITEMS_IN_QUERY)
						groupDeps[groupDepCnt++] = relation;
				}

				//! Adds all grouping invalidation relations to the dependency set.
				void add_group_deps() {
					deps.set_dep_flag(DependencyHasGroup);

					const bool hasBuiltInGroupDep = groupBy != EntityBad && (groupByFunc == group_by_func_default ||
																																	 groupByFunc == group_by_func_depth_order);
					if (hasBuiltInGroupDep)
						deps.add_rel(groupBy);
					for (auto relation: group_deps_view())
						deps.add_rel(relation);
				}

				//! Returns mutable compiled terms in execution order.
				//! \return Mutable view over valid terms entries.
				GAIA_NODISCARD std::span<QueryTerm> terms_view_mut() {
					return {terms.data(), idsCnt};
				}
				//! Returns compiled terms in execution order.
				//! \return Read-only view over valid terms entries.
				GAIA_NODISCARD std::span<const QueryTerm> terms_view() const {
					return {terms.data(), idsCnt};
				}

				//! Returns the dynamic-cache dependency shape derived from cache policy and dependencies.
				//! \return Dynamic-cache shape used by storage and invalidation planning.
				GAIA_NODISCARD DynamicCacheKind calc_dynamic_cache_kind() const {
					if (cachePolicy != CachePolicy::Dynamic)
						return DynamicCacheKind::None;

					const bool hasVariables = deps.has_dep_flag(DependencyHasVariableTerms);
					const bool hasSources = deps.has_dep_flag(DependencyHasSourceTerms);
					const bool hasTraversal = deps.has_dep_flag(DependencyHasTraversalTerms);
					const bool hasRelations = deps.relationCnt != 0;
					if (hasSources && hasTraversal && !hasVariables)
						return DynamicCacheKind::TraversedSource;

					const uint8_t depCnt = (uint8_t)hasVariables + (uint8_t)hasSources + (uint8_t)hasRelations;
					if (depCnt > 1)
						return DynamicCacheKind::Mixed;
					if (hasVariables)
						return DynamicCacheKind::Variable;
					if (hasSources)
						return hasTraversal ? DynamicCacheKind::TraversedSource : DynamicCacheKind::DirectSource;
					if (hasRelations)
						return DynamicCacheKind::RelationOnly;
					return DynamicCacheKind::None;
				}

				//! Returns whether reusable dynamic-cache checks use direct source entity archetype versions.
				//! \return True when direct source entity archetype versions validate this dynamic cache.
				GAIA_NODISCARD bool uses_direct_src_version_tracking() const {
					return canReuseDynamicCache && dynamicCacheKind == DynamicCacheKind::DirectSource;
				}

				//! Returns whether reusable dynamic-cache checks use a traversed source closure snapshot.
				//! \return True when a traversed-source snapshot validates this dynamic cache.
				GAIA_NODISCARD bool uses_src_trav_snapshot() const {
					return canReuseDynamicCache && dynamicCacheKind == DynamicCacheKind::TraversedSource;
				}

				//! Returns whether the current query shape can reuse dynamic-cache results.
				//! \return True when tracked runtime inputs are sufficient to validate the dynamic cache.
				GAIA_NODISCARD bool calc_can_reuse_dynamic_cache() const {
					if (cachePolicy != CachePolicy::Dynamic)
						return false;

					switch (dynamicCacheKind) {
						case DynamicCacheKind::None:
							return false;
						case DynamicCacheKind::RelationOnly:
						case DynamicCacheKind::Variable:
							return true;
						case DynamicCacheKind::DirectSource:
							return deps.can_reuse_src_cache();
						case DynamicCacheKind::TraversedSource:
							return deps.can_reuse_src_cache() && cacheSrcTrav != 0;
						case DynamicCacheKind::Mixed:
							if (!deps.has_dep_flag(DependencyHasSourceTerms))
								return true;
							if (!deps.can_reuse_src_cache())
								return false;
							return !deps.has_dep_flag(DependencyHasTraversalTerms) || cacheSrcTrav != 0;
					}

					GAIA_ASSERT(false);
					return false;
				}

				//! Refreshes canonical lookup arrays used by shared query deduplication.
				void refresh_lookup_keys() {
					GAIA_FOR(idsCnt) lookupIdentity.lookupTerms[i] = terms[i];
					canonicalize_lookup_terms(lookup_terms_view_mut());
					GAIA_FOR(changedCnt) lookupIdentity.changed[i] = changed[i];
					canonicalize_lookup_changed(changed_lookup_view_mut());
					GAIA_FOR(groupDepCnt) lookupIdentity.groupDeps[i] = groupDeps[i];
					canonicalize_lookup_changed(group_deps_lookup_view_mut());
				}

				//! Returns true when canonical lookup arrays match another query context payload.
				//! \param other Compiled payload to compare.
				//! \return True when terms, changed filters, and group dependencies match canonically.
				GAIA_NODISCARD bool lookup_keys_equal(const Data& other) const {
					{
						const auto left = lookup_terms_view();
						const auto right = other.lookup_terms_view();
						GAIA_FOR((uint32_t)left.size()) {
							if (left[i] != right[i])
								return false;
						}
					}

					{
						const auto left = changed_lookup_view();
						const auto right = other.changed_lookup_view();
						GAIA_FOR((uint32_t)left.size()) {
							if (left[i] != right[i])
								return false;
						}
					}

					{
						const auto left = group_deps_lookup_view();
						const auto right = other.group_deps_lookup_view();
						GAIA_FOR((uint32_t)left.size()) {
							if (left[i] != right[i])
								return false;
						}
					}

					return true;
				}

				//! Returns the hash contribution from canonical lookup-key payload arrays.
				//! \return Combined hash of canonical terms, filters, grouping dependencies, and identity flags.
				GAIA_NODISCARD QueryLookupHash::Type hash_lookup_key_payload() const {
					QueryLookupHash::Type hashLookup = 0;

					// Ids & ops
					{
						QueryLookupHash::Type hash = 0;

						for (const auto& pair: lookup_terms_view()) {
							hash = core::hash_combine(hash, (QueryLookupHash::Type)pair.op);
							hash = core::hash_combine(hash, (QueryLookupHash::Type)pair.id.value());
							hash = core::hash_combine(hash, (QueryLookupHash::Type)pair.src.value());
							hash = core::hash_combine(hash, (QueryLookupHash::Type)pair.entTrav.value());
							hash = core::hash_combine(hash, (QueryLookupHash::Type)(uint8_t)pair.travKind);
							hash = core::hash_combine(hash, (QueryLookupHash::Type)pair.travDepth);
							hash = core::hash_combine(hash, (QueryLookupHash::Type)(uint8_t)pair.matchKind);
						}
						hash = core::hash_combine(hash, (QueryLookupHash::Type)idsCnt);
						hash = core::hash_combine(hash, (QueryLookupHash::Type)readWriteMask);
						hash = core::hash_combine(hash, (QueryLookupHash::Type)cacheSrcTrav);

						const bool matchPrefab = (flags & QueryFlags::MatchPrefab) != 0;
						hash = core::hash_combine(hash, (QueryLookupHash::Type)matchPrefab);

						hashLookup = hash;
					}

					// Filters
					{
						QueryLookupHash::Type hash = 0;

						for (const auto entity: changed_lookup_view())
							hash = core::hash_combine(hash, (QueryLookupHash::Type)entity.value());
						hash = core::hash_combine(hash, (QueryLookupHash::Type)changedCnt);

						hashLookup = core::hash_combine(hashLookup, hash);
					}

					// Explicit grouping dependencies
					{
						QueryLookupHash::Type hash = 0;

						for (const auto entity: group_deps_lookup_view())
							hash = core::hash_combine(hash, (QueryLookupHash::Type)entity.value());
						hash = core::hash_combine(hash, (QueryLookupHash::Type)groupDepCnt);

						hashLookup = core::hash_combine(hashLookup, hash);
					}

					return hashLookup;
				}

				//! Returns true when grouping identity payload matches another query context payload.
				//! \param other Compiled payload to compare.
				//! \return True when grouping entity, callback, and ordering mode match.
				GAIA_NODISCARD bool grouping_payload_equal(const Data& other) const {
					if (groupBy != other.groupBy)
						return false;
					if (groupByFunc != other.groupByFunc)
						return false;
					return (flags & QueryFlags::OrderGroups) == (other.flags & QueryFlags::OrderGroups);
				}

				//! Returns true when the sort identity payload matches another query context payload.
				//! \param other Compiled payload to compare.
				//! \return True when sorting entity and callback match.
				GAIA_NODISCARD bool sort_payload_equal(const Data& other) const {
					return sortBy == other.sortBy && sortByFunc == other.sortByFunc;
				}

				//! Returns true when sort identity payload is active.
				//! \return True when either a sort entity or callback is configured.
				GAIA_NODISCARD bool has_sort_payload() const {
					return sortBy != EntityBad || sortByFunc != nullptr;
				}

				//! Returns the hash contribution from sort identity payload.
				//! \return Combined hash of the sort entity and callback.
				GAIA_NODISCARD QueryLookupHash::Type hash_sort_payload() const {
					QueryLookupHash::Type hash = 0;
					hash = core::hash_combine(hash, (QueryLookupHash::Type)sortBy.value());
					hash = core::hash_combine(hash, (QueryLookupHash::Type)sortByFunc);
					return hash;
				}

				//! Returns true when the shared query identity payload matches another query context payload.
				//! \param other Compiled payload to compare.
				//! \return True when every field contributing to shared query identity matches.
				GAIA_NODISCARD bool identity_payload_equal(const Data& other) const {
					if (idsCnt != other.idsCnt)
						return false;
					if (changedCnt != other.changedCnt)
						return false;
					if (groupDepCnt != other.groupDepCnt)
						return false;
					if (readWriteMask != other.readWriteMask)
						return false;
					if (cacheSrcTrav != other.cacheSrcTrav)
						return false;
					if (!lookup_keys_equal(other))
						return false;
					if (!sort_payload_equal(other))
						return false;
					return grouping_payload_equal(other);
				}

				//! Returns the hash contribution from grouping identity payload.
				//! \return Combined hash of grouping entity, callback, and ordering mode.
				GAIA_NODISCARD QueryLookupHash::Type hash_grouping_payload() const {
					QueryLookupHash::Type hash = 0;
					hash = core::hash_combine(hash, (QueryLookupHash::Type)groupBy.value());
					hash = core::hash_combine(hash, (QueryLookupHash::Type)groupByFunc);
					hash = core::hash_combine(hash, (QueryLookupHash::Type)((flags & QueryFlags::OrderGroups) != 0));
					return hash;
				}

				//! Returns the hash contribution from the full shared query identity payload.
				//! \return Combined lookup, optional sorting, and grouping identity hash.
				GAIA_NODISCARD QueryLookupHash::Type hash_identity_payload() const {
					QueryLookupHash::Type hash = hash_lookup_key_payload();
					if (has_sort_payload())
						hash = core::hash_combine(hash, hash_sort_payload());
					return core::hash_combine(hash, hash_grouping_payload());
				}

				//! Returns the finalized lookup hash for shared query identity.
				//! \return Direct-hash key calculated from the complete identity payload.
				GAIA_NODISCARD QueryLookupHash calc_lookup_hash() const {
					return {core::calculate_hash64(hash_identity_payload())};
				}
			} data{}; //!< Compiled query payload.
			// Make sure that MAX_ITEMS_IN_QUERY can fit into data.readWriteMask
			static_assert(MAX_ITEMS_IN_QUERY < 16);

			//! Attaches the query context to a world and its component cache.
			//! \param pWorld World that will compile and execute the query.
			void init(World* pWorld) {
				w = pWorld;
				cc = &comp_cache_mut(*pWorld);
			}

			//! Rebuilds derived masks, dependency metadata, and cache policy after authored terms change.
			void refresh() {
				const auto mask0_old = data.as_mask_0;
				const auto mask1_old = data.as_mask_1;
				const auto isComplex_old = data.flags & QueryFlags::Complex;
				const auto hasSourceTerms_old = data.flags & QueryFlags::HasSourceTerms;
				const auto hasVariableTerms_old = data.flags & QueryFlags::HasVariableTerms;
				const auto cachePolicy_old = data.cachePolicy;
				const auto createArchetypeMatchKind_old = data.createArchetypeMatchKind;
				const auto dynamicCacheKind_old = data.dynamicCacheKind;
				const auto canDirectTargetEval_old = data.canDirectTargetEval;
				const auto canDirectEntitySeedEvalShape_old = data.canDirectEntitySeedEvalShape;
				const auto hasOnlyDirectOrTerms_old = data.hasOnlyDirectOrTerms;
				const auto canReuseDynamicCache_old = data.canReuseDynamicCache;
				const auto dependencyFlags_old = data.deps.flags;
				const auto createSelectorCnt_old = data.deps.createSelectorCnt;
				const auto exclusionCnt_old = data.deps.exclusionCnt;
				const auto relationCnt_old = data.deps.relationCnt;
				const auto sourceEntityCnt_old = data.deps.sourceEntityCnt;
				const auto sourceTermCnt_old = data.deps.sourceTermCnt;
				auto createSelectors_old = data.deps.createSelectors;
				auto exclusions_old = data.deps.exclusions;
				auto relations_old = data.deps.relations;
				auto sourceEntities_old = data.deps.sourceEntities;

				// Update masks
				{
					uint32_t as_mask_0 = 0;
					uint32_t as_mask_1 = 0;
					bool isComplex = false;
					bool hasSourceTerms = false;
					bool hasVariableTerms = false;
					bool hasPrefabTerms = false;
					bool hasCreateSelector = false;
					bool canDirectCreateArchetypeMatch = true;
					bool hasEntityFilterTerms = false;
					bool canDirectTargetEval = true;
					bool hasOnlyDirectOrTerms = true;
					bool hasOrTerms = false;
					bool hasDirectTargetEvalPositiveTerms = false;
					const QueryTerm* pSingleDirectTargetAllTerm = nullptr;
					bool singleDirectTargetEvalPossible = true;
					QueryEntityArray idsNoSrc;
					QueryEntityArray createSelectorsAll;
					QueryEntityArray createSelectorsOr;
					uint32_t idsNoSrcCnt = 0;
					uint8_t createSelectorAllCnt = 0;
					uint8_t createSelectorOrCnt = 0;
					data.deps.clear();
					data.directTargetEvalKind = DirectTargetEvalKind::Generic;
					data.directTargetEvalId = EntityBad;
					if (data.sortByFunc != nullptr)
						data.deps.set_dep_flag(DependencyHasSort);
					if (data.groupBy != EntityBad)
						data.add_group_deps();

					auto terms = data.terms_view();
					const auto cnt = (uint32_t)terms.size();
					GAIA_FOR(cnt) {
						const auto& term = terms[i];
						const auto id = term.id;
						hasPrefabTerms |= id == Prefab;
						const bool isDirectIsTerm = term.src == EntityBad && term.entTrav == EntityBad &&
																				!term_has_variables(term) && term.matchKind != QueryMatchKind::Direct &&
																				id.pair() && id.id() == Is.id() && !is_wildcard(id.gen()) &&
																				!is_variable((EntityId)id.gen());
						const bool isPotentialInheritedTerm = query_term_uses_potential_inherited_id_matching(term);
						const bool isInheritedTerm = isPotentialInheritedTerm && world_term_uses_inherit_policy(*w, id);
						const bool isNonFragmentingTerm =
								term.src == EntityBad && term.entTrav == EntityBad && !term_has_variables(term) &&
								((id.pair() && world_relation_uses_non_fragmenting_storage(*w, pair_rel(*w, id))) ||
								 (!id.pair() && world_component_is_non_fragmenting(*w, id)));
						hasEntityFilterTerms |= isNonFragmentingTerm || isDirectIsTerm || isInheritedTerm;
					}

					GAIA_FOR(cnt) {
						const auto& term = terms[i];
						const auto id = term.id;
						if (term.src != EntityBad || term.entTrav != EntityBad || term_has_variables(term)) {
							singleDirectTargetEvalPossible = false;
							canDirectTargetEval = false;
							hasOnlyDirectOrTerms = false;
						}
						switch (term.op) {
							case QueryOpKind::All:
								hasDirectTargetEvalPositiveTerms = true;
								if (pSingleDirectTargetAllTerm == nullptr)
									pSingleDirectTargetAllTerm = &term;
								else
									singleDirectTargetEvalPossible = false;
								hasOnlyDirectOrTerms = false;
								break;
							case QueryOpKind::Or:
								hasDirectTargetEvalPositiveTerms = true;
								hasOrTerms = true;
								break;
							case QueryOpKind::Not:
								break;
							case QueryOpKind::Any:
							case QueryOpKind::Count:
								singleDirectTargetEvalPossible = false;
								canDirectTargetEval = false;
								hasOnlyDirectOrTerms = false;
								break;
						}
						const bool isDirectIsTerm = term.src == EntityBad && term.entTrav == EntityBad &&
																				!term_has_variables(term) && term.matchKind != QueryMatchKind::Direct &&
																				id.pair() && id.id() == Is.id() && !is_wildcard(id.gen()) &&
																				!is_variable((EntityId)id.gen());
						const bool isPotentialInheritedTerm = query_term_uses_potential_inherited_id_matching(term);
						const bool isInheritedTerm = isPotentialInheritedTerm && world_term_uses_inherit_policy(*w, id);
						const bool isCachedInheritedDataTerm = isInheritedTerm && !world_component_uses_sparse_storage(*w, id);
						const bool isNonFragmentingTerm =
								term.src == EntityBad && term.entTrav == EntityBad && !term_has_variables(term) &&
								((id.pair() && world_relation_uses_non_fragmenting_storage(*w, pair_rel(*w, id))) ||
								 (!id.pair() && world_component_is_non_fragmenting(*w, id)));
						canDirectCreateArchetypeMatch &= term.src == EntityBad;
						if (id.pair() && (is_wildcard(id.id()) || is_wildcard(id.gen())))
							data.deps.set_dep_flag(DependencyHasWildcardTerms);
						const bool hasDynamicRelationUsage =
								term.entTrav != EntityBad || term.src != EntityBad || term_has_variables(term);
						if (id.pair() && hasDynamicRelationUsage && !is_wildcard(id.id()) && !is_variable((EntityId)id.id()))
							data.deps.add_rel(pair_rel(*w, id));
						if (term.entTrav != EntityBad) {
							data.deps.add_rel(term.entTrav);
							data.deps.set_dep_flag(DependencyHasTraversalTerms);
						}
						if (term.src != EntityBad) {
							hasSourceTerms = true;
							data.deps.set_dep_flag(DependencyHasSourceTerms);
							++data.deps.sourceTermCnt;
							if (!is_variable(term.src))
								data.deps.add_src_entity(term.src);
						}

						if (term_has_variables(term)) {
							hasVariableTerms = true;
							data.deps.set_dep_flag(DependencyHasVariableTerms);
							isComplex = true;
							continue;
						}

						if (isPotentialInheritedTerm)
							data.deps.set_dep_flag(DependencyHasPotentialInheritedIdTerms);

						if (isNonFragmentingTerm || isDirectIsTerm || isInheritedTerm) {
							data.deps.set_dep_flag(DependencyHasEntityFilterTerms);
							if (isCachedInheritedDataTerm)
								data.deps.set_dep_flag(DependencyHasInheritedDataTerms);
							if (id.pair() && !is_wildcard(id.id()) && !is_variable((EntityId)id.id()))
								data.deps.add_rel(pair_rel(*w, id));
							continue;
						}

						if (hasEntityFilterTerms && term.op == QueryOpKind::Or) {
							isComplex = true;
							continue;
						}

						// Source terms are evaluated separately by the VM.
						// They should not affect archetype-level query masks.
						if (term.src != EntityBad) {
							continue;
						}

						// ANY terms are not hard requirements and must not affect archetype prefilter masks.
						if (term.op != QueryOpKind::Any)
							idsNoSrc[idsNoSrcCnt++] = id;

						if (term.op == QueryOpKind::All || term.op == QueryOpKind::Or) {
							hasCreateSelector = true;
							data.deps.set_dep_flag(DependencyHasPositiveTerms);
							if (term.op == QueryOpKind::All)
								createSelectorsAll[createSelectorAllCnt++] = id;
							else
								createSelectorsOr[createSelectorOrCnt++] = id;
						} else if (term.op == QueryOpKind::Not) {
							data.deps.set_dep_flag(DependencyHasNegativeTerms);
							data.deps.exclusions[data.deps.exclusionCnt++] = id;
						} else if (term.op == QueryOpKind::Any) {
							data.deps.set_dep_flag(DependencyHasAnyTerms);
						}

						// Build the Is mask.
						// We will use it to identify entities with an Is relationship quickly.
						const bool allowSemanticIs = !(
								term.matchKind == QueryMatchKind::Direct && id.pair() && id.id() == Is.id() && !is_wildcard(id.gen()));
						if (!id.pair()) {
							const auto j = (uint32_t)i;
							const auto has_as = allowSemanticIs ? (uint32_t)is_base(*w, id) : 0U;
							as_mask_0 |= (has_as << j);
						} else {
							const bool idIsWildcard = is_wildcard(id.id());
							const bool isGenWildcard = is_wildcard(id.gen());
							isComplex |= (idIsWildcard || isGenWildcard);

							if (!idIsWildcard) {
								const auto j = (uint32_t)i;
								const auto e = pair_rel(*w, id);
								const auto has_as = allowSemanticIs ? (uint32_t)is_base(*w, e) : 0U;
								as_mask_0 |= (has_as << j);
							}

							if (!isGenWildcard) {
								const auto j = (uint32_t)i;
								const auto e = pair_tgt(*w, id);
								const auto has_as = allowSemanticIs ? (uint32_t)is_base(*w, e) : 0U;
								as_mask_1 |= (has_as << j);
							}
						}
					}

					if (singleDirectTargetEvalPossible && pSingleDirectTargetAllTerm != nullptr) {
						const auto& term = *pSingleDirectTargetAllTerm;
						const auto id = term.id;
						if (term.matchKind == QueryMatchKind::In && id.pair() && id.id() == Is.id() && !is_wildcard(id.gen()) &&
								!is_variable((EntityId)id.gen())) {
							data.directTargetEvalKind = DirectTargetEvalKind::SingleAllInIs;
						} else if (
								term.matchKind == QueryMatchKind::Semantic && id.pair() && id.id() == Is.id() &&
								!is_wildcard(id.gen()) && !is_variable((EntityId)id.gen())) {
							data.directTargetEvalKind = DirectTargetEvalKind::SingleAllSemanticIs;
						} else if (
								term.matchKind == QueryMatchKind::Semantic && !is_wildcard(id) && !is_variable((EntityId)id.id()) &&
								(!id.pair() || !is_variable((EntityId)id.gen())) && world_term_uses_inherit_policy(*w, id)) {
							data.directTargetEvalKind = DirectTargetEvalKind::SingleAllInherited;
						} else {
							data.directTargetEvalKind = DirectTargetEvalKind::SingleAllDirect;
						}
						data.directTargetEvalId = id;
					}
					data.canDirectTargetEval = canDirectTargetEval && hasDirectTargetEvalPositiveTerms;
					data.canDirectEntitySeedEvalShape =
							data.canDirectTargetEval && data.sortByFunc == nullptr && data.groupBy == EntityBad;
					data.hasOnlyDirectOrTerms = hasOnlyDirectOrTerms && hasOrTerms;

					// Update the mask
					data.as_mask_0 = as_mask_0;
					data.as_mask_1 = as_mask_1;
					data.deps.createSelectorCnt = 0;
					if (createSelectorAllCnt != 0) {
						auto selector_rank = [](Entity term) {
							if (!term.pair())
								return 2;
							if (!is_wildcard(term.id()) && !is_wildcard(term.gen()))
								return 0;
							if (is_wildcard(term.id()) && is_wildcard(term.gen()))
								return 3;
							return 1;
						};

						// For immediate structural queries, we choose one required ALL selector as the create-time wake-up key.
						// This choice is ordered by:
						//   1) smaller component index bucket size first
						//   2) if equal, more specific selector first
						uint8_t bestIdx = 0;
						auto bestBucketSize = world_component_index_bucket_size(*w, createSelectorsAll[0]);
						auto bestRank = selector_rank(createSelectorsAll[0]);
						GAIA_FOR2_(1, createSelectorAllCnt, i) {
							const auto bucketSize = world_component_index_bucket_size(*w, createSelectorsAll[i]);
							const auto rank = selector_rank(createSelectorsAll[i]);
							if (bucketSize < bestBucketSize || (bucketSize == bestBucketSize && rank < bestRank)) {
								bestBucketSize = bucketSize;
								bestRank = rank;
								bestIdx = (uint8_t)i;
							}
						}
						data.deps.createSelectors[data.deps.createSelectorCnt++] = createSelectorsAll[bestIdx];
					} else {
						GAIA_FOR(createSelectorOrCnt) {
							data.deps.createSelectors[data.deps.createSelectorCnt++] = createSelectorsOr[i];
						}
					}
					if (hasPrefabTerms)
						data.flags |= QueryCtx::QueryFlags::HasPrefabTerms;
					else
						data.flags &= ~QueryCtx::QueryFlags::HasPrefabTerms;

					if (hasSourceTerms)
						data.flags |= QueryCtx::QueryFlags::HasSourceTerms;
					else
						data.flags &= ~QueryCtx::QueryFlags::HasSourceTerms;

					if (hasVariableTerms)
						data.flags |= QueryCtx::QueryFlags::HasVariableTerms;
					else
						data.flags &= ~QueryCtx::QueryFlags::HasVariableTerms;

					if (hasSourceTerms || hasVariableTerms)
						data.cachePolicy = CachePolicy::Dynamic;
					else if (
							!hasEntityFilterTerms && data.sortByFunc == nullptr && data.groupBy == EntityBad && hasCreateSelector)
						data.cachePolicy = CachePolicy::Immediate;
					else
						data.cachePolicy = CachePolicy::Lazy;

					data.createArchetypeMatchKind = data.cachePolicy == CachePolicy::Immediate && canDirectCreateArchetypeMatch
																							? CreateArchetypeMatchKind::DirectStructuralTerms
																							: CreateArchetypeMatchKind::Vm;
					data.dynamicCacheKind = data.calc_dynamic_cache_kind();

					// Traversed-source snapshot caching is only effective for traversed source terms.
					if (!data.deps.has_dep_flag(DependencyHasSourceTerms) || !data.deps.has_dep_flag(DependencyHasTraversalTerms))
						data.cacheSrcTrav = 0;

					data.canReuseDynamicCache = data.calc_can_reuse_dynamic_cache();

					// Calculate the component mask for simple queries
					isComplex |= ((data.as_mask_0 + data.as_mask_1) != 0);
					if (isComplex) {
						data.queryMask = {};
						data.flags |= QueryCtx::QueryFlags::Complex;
					} else {
						data.queryMask = build_entity_mask(EntitySpan{idsNoSrc.data(), idsNoSrcCnt});
						data.flags &= ~QueryCtx::QueryFlags::Complex;
					}
				}

				// Request recompilation of the query if the mask has changed
				data.refresh_lookup_keys();

				// Request recompilation of the query if the mask has changed
				if (mask0_old != data.as_mask_0 || mask1_old != data.as_mask_1 ||
						isComplex_old != (data.flags & QueryFlags::Complex) ||
						hasSourceTerms_old != (data.flags & QueryFlags::HasSourceTerms) ||
						hasVariableTerms_old != (data.flags & QueryFlags::HasVariableTerms) ||
						canDirectTargetEval_old != data.canDirectTargetEval ||
						canDirectEntitySeedEvalShape_old != data.canDirectEntitySeedEvalShape ||
						hasOnlyDirectOrTerms_old != data.hasOnlyDirectOrTerms ||
						canReuseDynamicCache_old != data.canReuseDynamicCache || cachePolicy_old != data.cachePolicy ||
						createArchetypeMatchKind_old != data.createArchetypeMatchKind ||
						dynamicCacheKind_old != data.dynamicCacheKind || dependencyFlags_old != data.deps.flags ||
						createSelectorCnt_old != data.deps.createSelectorCnt || exclusionCnt_old != data.deps.exclusionCnt ||
						relationCnt_old != data.deps.relationCnt || sourceEntityCnt_old != data.deps.sourceEntityCnt ||
						sourceTermCnt_old != data.deps.sourceTermCnt || createSelectors_old != data.deps.createSelectors ||
						exclusions_old != data.deps.exclusions || relations_old != data.deps.relations ||
						sourceEntities_old != data.deps.sourceEntities)
					data.flags |= QueryCtx::QueryFlags::Recompile;
			}

			//! Compares shared query identity without requiring invalid handles.
			//! \param leftCtx First query context.
			//! \param rightCtx Second query context.
			//! \return True when lookup hashes and complete identity payloads match.
			GAIA_NODISCARD static bool
			equals_no_handle_assumption(const QueryCtx& leftCtx, const QueryCtx& rightCtx) noexcept {
				// Lookup hash must match
				if (leftCtx.hashLookup != rightCtx.hashLookup)
					return false;

				const auto& left = leftCtx.data;
				const auto& right = rightCtx.data;
				return left.identity_payload_equal(right);
			}

			//! Compares query contexts during initial shared-query lookup.
			//! \param other Query context to compare.
			//! \return True when both contexts have the same shared query identity.
			GAIA_NODISCARD bool operator==(const QueryCtx& other) const noexcept {
				// Comparison expected to be done only the first time the query is set up
				GAIA_ASSERT(q.handle.id() == QueryIdBad);
				// Fast path when cache ids are set
				// if (queryId != QueryIdBad && queryId == other.queryId)
				// 	return true;

				return equals_no_handle_assumption(*this, other);
			}

			//! Compares query contexts for distinct shared query identity.
			//! \param other Query context to compare.
			//! \return True when the contexts do not have the same identity.
			GAIA_NODISCARD bool operator!=(const QueryCtx& other) const noexcept {
				return !operator==(other);
			}
		};

		//! Functor for sorting terms in a query before compilation
		struct query_sort_cond {
			//! Orders two terms by their canonical lookup representation.
			//! \param lhs First term.
			//! \param rhs Second term.
			//! \return True when \a lhs precedes \a rhs.
			constexpr bool operator()(const QueryTerm& lhs, const QueryTerm& rhs) const {
				return query_term_less_for_lookup(lhs, rhs);
			}
		};

		//! Sorts internal component arrays
		//! \param ctx Query context whose terms and changed filters are canonicalized.
		inline void sort(QueryCtx& ctx) {
			const uint32_t idsCnt = ctx.data.idsCnt;
			const uint32_t changedCnt = ctx.data.changedCnt;

			auto& ctxData = ctx.data;
			// Canonicalize degenerate OR queries: a single OR term has AND semantics.
			// Rewriting it here keeps ordering/hash behavior identical to an explicit ALL term.
			if (idsCnt > 0) {
				uint32_t orCnt = 0;
				uint32_t orIdx = BadIndex;
				GAIA_FOR(idsCnt) {
					if (ctxData.terms[i].op != QueryOpKind::Or)
						continue;
					++orCnt;
					orIdx = i;
					if (orCnt > 1)
						break;
				}

				if (orCnt == 1)
					ctxData.terms[orIdx].op = QueryOpKind::All;
			}

			// Sort data. Necessary for correct hash calculation.
			// Without sorting query.all<XXX, YYY> would be different than query.all<YYY, XXX>.
			// Also makes sure data is in optimal order for query processing.
			core::sort(
					ctxData.terms.data(), ctxData.terms.data() + ctxData.idsCnt, query_sort_cond{}, //
					[&](uint32_t left, uint32_t right) {
						core::swap(ctxData.ids[left], ctxData.ids[right]);
						core::swap(ctxData.terms[left], ctxData.terms[right]);

						// Make sure masks remains correct after sorting
						core::swap_bits(ctxData.readWriteMask, left, right);
						core::swap_bits(ctxData.as_mask_0, left, right);
						core::swap_bits(ctxData.as_mask_1, left, right);
					});

			if (idsCnt > 0) {
				uint32_t i = 0;
				while (i < idsCnt && ctxData.terms[i].op == QueryOpKind::All)
					++i;
				ctxData.firstOr = (uint8_t)i;
				while (i < idsCnt && ctxData.terms[i].op == QueryOpKind::Or)
					++i;
				ctxData.firstNot = (uint8_t)i;
				while (i < idsCnt && ctxData.terms[i].op == QueryOpKind::Not)
					++i;
				ctxData.firstAny = (uint8_t)i;
			} else
				ctxData.firstOr = ctxData.firstNot = ctxData.firstAny = 0;

			// Canonicalize filter order. This enables monotonic component lookup in filter matching
			// and keeps cache keys stable regardless of changed() call order.
			if (changedCnt > 1) {
				core::sort(ctxData.changed.data(), ctxData.changed.data() + changedCnt, SortComponentCond{});
			}

			GAIA_FOR(changedCnt) {
				const auto comp = ctxData.changed[i];
				uint32_t compIdx = 0;
				while (compIdx < idsCnt && ctxData.ids[compIdx] != comp)
					++compIdx;

				GAIA_ASSERT(compIdx < idsCnt);
				ctxData.changedFields[i] = compIdx < idsCnt ? (uint8_t)compIdx : (uint8_t)0xFF;
			}
		}

		//! Traversed-source snapshot caching only has meaning for terms that use both a source and traversal.
		//! All other query shapes normalize the effective cap to 0 so shared cache identity does not diverge.
		//! \param ctx Query context whose traversed-source cache limit is normalized.
		inline void normalize_cache_src_trav(QueryCtx& ctx) {
			auto& ctxData = ctx.data;
			if (ctxData.cacheSrcTrav == 0)
				return;

			bool hasTraversedSourceTerm = false;
			for (const auto& term: ctxData.terms_view()) {
				if (term.src == EntityBad || term.entTrav == EntityBad)
					continue;

				hasTraversedSourceTerm = true;
				break;
			}

			if (!hasTraversedSourceTerm)
				ctxData.cacheSrcTrav = 0;
		}

		//! Calculates and stores the shared-query lookup hash after canonicalization.
		//! \param ctx Query context whose hashLookup field is initialized.
		inline void calc_lookup_hash(QueryCtx& ctx) {
			GAIA_ASSERT(ctx.cc != nullptr);
			// Make sure we don't calculate the hash twice
			GAIA_ASSERT(ctx.hashLookup.hash == 0);

			ctx.hashLookup = ctx.data.calc_lookup_hash();
		}

		//! Finds the index at which the provided component id is located in the component array
		//! \param pTerms Pointer to the start of the terms array
		//! \param entity Entity we search for
		//! \param src Source entity
		//! \tparam MAX_COMPONENTS Number of terms to scan. Known at compile time so the loop can be optimized.
		//! \return Index of the component id in the array
		//! \warning The component id must be present in the array
		template <uint32_t MAX_COMPONENTS>
		GAIA_NODISCARD inline uint32_t comp_idx(const QueryTerm* pTerms, Entity entity, Entity src) {
			// We let the compiler know the upper iteration bound at compile-time.
			// This way it can optimize better (e.g. loop unrolling, vectorization).
			GAIA_FOR(MAX_COMPONENTS) {
				if (pTerms[i].id == entity && pTerms[i].src == src)
					return i;
			}

			GAIA_ASSERT(false);
			return BadIndex;
		}
	} // namespace ecs
} // namespace gaia

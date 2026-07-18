#pragma once
#include "gaia/config/config.h"

#include "gaia/cnt/darray_ext.h"
#include "gaia/cnt/map.h"
#include "gaia/cnt/set.h"
#include "gaia/ecs/id.h"

//! \cond INTERNAL
namespace gaia {
	namespace ecs {
		using PairLookupMap = cnt::map<EntityLookupKey, cnt::set<EntityLookupKey>>;

		class PairLookup {
			//! Map of relation -> targets.
			PairLookupMap m_relToTgt;
			//! Map of target -> relations.
			PairLookupMap m_tgtToRel;

		public:
			//! Clears all pair lookup entries.
			void clear() {
				m_relToTgt = {};
				m_tgtToRel = {};
			}

			//! Returns targets for \a relation.
			//! \param relation Relation entity.
			//! \return Target set or nullptr when no target is stored.
			GAIA_NODISCARD const cnt::set<EntityLookupKey>* targets(Entity relation) const {
				const auto it = m_relToTgt.find(EntityLookupKey(relation));
				if (it == m_relToTgt.end())
					return nullptr;

				return &it->second;
			}

			//! Returns relations for \a target.
			//! \param target Target entity.
			//! \return Relation set or nullptr when no relation is stored.
			GAIA_NODISCARD const cnt::set<EntityLookupKey>* relations(Entity target) const {
				const auto it = m_tgtToRel.find(EntityLookupKey(target));
				if (it == m_tgtToRel.end())
					return nullptr;

				return &it->second;
			}

			//! Returns an iterator to the first relation bucket.
			//! \return Iterator to the first relation bucket.
			GAIA_NODISCARD auto relation_begin() const {
				return m_relToTgt.begin();
			}

			//! Returns an iterator past the last relation bucket.
			//! \return Iterator past the last relation bucket.
			GAIA_NODISCARD auto relation_end() const {
				return m_relToTgt.end();
			}

			//! Adds an exact relation-target pair to the lookup.
			//! \param relation Pair relation entity.
			//! \param target Pair target entity.
			void add_pair(Entity relation, Entity target) {
				const auto relKey = EntityLookupKey(relation);
				const auto tgtKey = EntityLookupKey(target);
				const auto allKey = EntityLookupKey(All);

				auto& relTargets = m_relToTgt[relKey];
				const bool relHadTargets = !relTargets.empty();
				relTargets.insert(tgtKey);

				auto& tgtRelations = m_tgtToRel[tgtKey];
				const bool tgtHadRelations = !tgtRelations.empty();
				tgtRelations.insert(relKey);

				if (!tgtHadRelations)
					m_relToTgt[allKey].insert(tgtKey);
				if (!relHadTargets)
					m_tgtToRel[allKey].insert(relKey);
			}

			//! Removes one target from a source bucket.
			//! \param map Map containing the source bucket.
			//! \param source Source bucket key.
			//! \param remove Entry to remove from the source bucket.
			//! \return True when the source bucket became empty.
			static bool del_bucket_entry(PairLookupMap& map, EntityLookupKey source, EntityLookupKey remove) {
				auto itTargets = map.find(source);
				if (itTargets == map.end())
					return false;

				auto& targets = itTargets->second;
				targets.erase(remove);
				return targets.empty();
			}

			//! Copies bucket entries before pair lookup mutation invalidates iterators.
			//! \param map Map containing the source bucket.
			//! \param source Source bucket key.
			//! \param out Copied bucket entries.
			static void collect_bucket_entries(
					const PairLookupMap& map, EntityLookupKey source, cnt::darray_ext<EntityLookupKey, 64>& out) {
				const auto it = map.find(source);
				if (it == map.end())
					return;

				for (auto entry: it->second)
					out.push_back(entry);
			}

			//! Removes an exact relation-target pair from the lookup.
			//! \param relation Pair relation entity.
			//! \param target Pair target entity.
			void del_pair(Entity relation, Entity target) {
				const auto relKey = EntityLookupKey(relation);
				const auto tgtKey = EntityLookupKey(target);
				const auto allKey = EntityLookupKey(All);

				const bool relEmpty = del_bucket_entry(m_relToTgt, relKey, tgtKey);
				const bool tgtEmpty = del_bucket_entry(m_tgtToRel, tgtKey, relKey);
				if (tgtEmpty)
					(void)del_bucket_entry(m_relToTgt, allKey, tgtKey);
				if (relEmpty)
					(void)del_bucket_entry(m_tgtToRel, allKey, relKey);
			}

			//! Removes all entries whose relation or target is \a entity.
			//! \param entity Entity being deleted.
			void del_entity_pairs(Entity entity) {
				const auto key = EntityLookupKey(entity);

				cnt::darray_ext<EntityLookupKey, 64> targets;
				cnt::darray_ext<EntityLookupKey, 64> relations;
				collect_bucket_entries(m_relToTgt, key, targets);
				collect_bucket_entries(m_tgtToRel, key, relations);

				for (auto target: targets)
					del_pair(entity, target.entity());
				for (auto relation: relations)
					del_pair(relation.entity(), entity);

				m_relToTgt.erase(key);
				m_tgtToRel.erase(key);
			}
		};
	} // namespace ecs
} // namespace gaia
//! \endcond

#pragma once
#include "gaia/config/config.h"

#include <cstdint>

#include "gaia/cnt/darray.h"
#include "gaia/cnt/map.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/id.h"

namespace gaia {
	namespace ecs {
		namespace detail {
			//! Storage for exclusive relation pairs that do not fragment archetypes.
			struct NonFragmentingRelationStore {
			private:
				//! Source-side state for an exact pair record.
				struct PairSourceRecord {
					//! Bound target.
					Entity target = EntityBad;
					//! Position in the bound target's source list.
					uint32_t targetIdx = BadIndex;
				};

				//! Direct source-entity-id indexed target lookup. EntityBad means no binding for that source.
				cnt::darray<Entity> srcToTgt;
				//! Direct source-entity-id indexed position in the current target's source list.
				cnt::darray<uint32_t> srcToTgtIdx;
				//! Full-identity lookup for exact pair sources, which share ids with their relation endpoints.
				cnt::map<EntityLookupKey, PairSourceRecord> pairSrcToTgt;
				//! Number of active ordinary and exact pair source bindings.
				uint32_t srcToTgtCnt = 0;
				//! Direct target-entity-id indexed source buckets used for traversal and wildcard operations.
				cnt::darray<cnt::darray<Entity>> tgtToSrc;

				//! Returns the source's position in its target bucket.
				//! \param source Source entity.
				//! \return Bucket position or BadIndex when the source is not bound.
				GAIA_NODISCARD uint32_t target_source_index(Entity source) const {
					if GAIA_UNLIKELY (source.pair()) {
						const auto it = pairSrcToTgt.find(EntityLookupKey(source));
						return it != pairSrcToTgt.end() ? it->second.targetIdx : BadIndex;
					}

					return source.id() < srcToTgtIdx.size() ? srcToTgtIdx[source.id()] : BadIndex;
				}

				//! Updates the source's position after a target bucket swap.
				//! \param source Source entity.
				//! \param index New position in the target bucket.
				void set_target_source_index(Entity source, uint32_t index) {
					if GAIA_UNLIKELY (source.pair()) {
						const auto it = pairSrcToTgt.find(EntityLookupKey(source));
						GAIA_ASSERT(it != pairSrcToTgt.end());
						if (it != pairSrcToTgt.end())
							it->second.targetIdx = index;
						return;
					}

					GAIA_ASSERT(source.id() < srcToTgtIdx.size());
					srcToTgtIdx[source.id()] = index;
				}

			public:
				//! Ensures source-indexed storage can hold \a source.
				//! \param source Source entity.
				void ensure_source_capacity(Entity source) {
					const auto required = (uint32_t)source.id() + 1;
					if (srcToTgt.size() >= required)
						return;

					const auto oldSize = (uint32_t)srcToTgt.size();
					auto newSize = oldSize == 0 ? 16U : oldSize;
					while (newSize < required)
						newSize *= 2U;

					srcToTgt.resize(newSize, EntityBad);
					srcToTgtIdx.resize(newSize, BadIndex);
				}

				//! Ensures target-indexed storage can hold \a target.
				//! \param target Target entity.
				void ensure_target_capacity(Entity target) {
					const auto required = target.id() + 1;
					if (tgtToSrc.size() >= required)
						return;

					const auto oldSize = (uint32_t)tgtToSrc.size();
					auto newSize = oldSize == 0 ? 16U : oldSize;
					while (newSize < required)
						newSize *= 2U;

					tgtToSrc.resize(newSize);
				}

				//! Returns the target currently bound to \a source.
				//! \param source Source entity.
				//! \return Bound target or EntityBad when no binding exists.
				GAIA_NODISCARD Entity target(Entity source) const {
					if GAIA_UNLIKELY (source.pair()) {
						const auto it = pairSrcToTgt.find(EntityLookupKey(source));
						return it != pairSrcToTgt.end() ? it->second.target : EntityBad;
					}

					if (source.id() >= srcToTgt.size())
						return EntityBad;

					return srcToTgt[source.id()];
				}

				//! Returns sources currently bound to \a target.
				//! \param target Target entity.
				//! \return Source bucket or nullptr when no source is bound.
				GAIA_NODISCARD const cnt::darray<Entity>* sources(Entity target) const {
					if (target.id() >= tgtToSrc.size())
						return nullptr;

					const auto& sources = tgtToSrc[target.id()];
					return sources.empty() ? nullptr : &sources;
				}

				//! Returns the number of active source bindings.
				//! \return Number of bound source entities.
				GAIA_NODISCARD uint32_t source_count() const {
					return srcToTgtCnt;
				}

				//! Appends ids of all bound source entities.
				//! \param out Output array receiving source entity ids.
				void collect_source_ids(cnt::darray<EntityId>& out) const {
					out.reserve(out.size() + srcToTgtCnt);
					GAIA_FOR((uint32_t)srcToTgt.size()) {
						if (srcToTgt[i] == EntityBad)
							continue;

						out.push_back((EntityId)i);
					}
				}

				//! Appends all bound exact pair sources in deterministic entity order.
				//! \param out Output array receiving exact pair sources.
				void collect_pair_sources(cnt::darray<Entity>& out) const {
					const auto first = out.size();
					out.reserve(first + (uint32_t)pairSrcToTgt.size());
					for (const auto& pair: pairSrcToTgt)
						out.push_back(pair.first.entity());

					core::sort(out.begin() + first, out.end(), [](Entity left, Entity right) {
						return left.value() < right.value();
					});
				}

				//! Removes \a source from the source bucket for \a target.
				//! \param target Target entity.
				//! \param source Source entity.
				void remove_target_source(Entity target, Entity source) {
					GAIA_ASSERT(target.id() < tgtToSrc.size());
					if (target.id() >= tgtToSrc.size())
						return;

					auto& sources = tgtToSrc[target.id()];
					const auto idx = target_source_index(source);
					GAIA_ASSERT(idx != BadIndex && idx < sources.size());
					if (idx == BadIndex || idx >= sources.size())
						return;

					const auto lastIdx = (uint32_t)sources.size() - 1;
					if (idx != lastIdx) {
						const auto movedSource = sources[lastIdx];
						sources[idx] = movedSource;
						set_target_source_index(movedSource, idx);
					}

					sources.pop_back();
				}

				//! Binds \a source to \a target.
				//! \param source Source entity.
				//! \param target Target entity.
				//! \return True when the stored binding changed.
				GAIA_NODISCARD bool set(Entity source, Entity target) {
					if GAIA_LIKELY (!source.pair())
						ensure_source_capacity(source);
					const auto oldTarget = this->target(source);
					if (oldTarget != EntityBad) {
						if (oldTarget == target)
							return false;

						remove_target_source(oldTarget, source);
					} else {
						++srcToTgtCnt;
					}

					ensure_target_capacity(target);
					auto& sources = tgtToSrc[target.id()];
					const auto sourceIdx = (uint32_t)sources.size();
					if GAIA_UNLIKELY (source.pair())
						pairSrcToTgt[EntityLookupKey(source)] = PairSourceRecord{target, sourceIdx};
					else {
						srcToTgt[source.id()] = target;
						srcToTgtIdx[source.id()] = sourceIdx;
					}
					sources.push_back(source);

					return true;
				}

				//! Removes \a source from the store.
				//! \param source Source entity.
				//! \param target Required target, or EntityBad to remove any target.
				//! \return True when a binding was removed.
				GAIA_NODISCARD bool remove(Entity source, Entity target) {
					const auto oldTarget = this->target(source);
					if (oldTarget == EntityBad)
						return false;
					if (target != EntityBad && oldTarget != target)
						return false;

					remove_target_source(oldTarget, source);
					if GAIA_UNLIKELY (source.pair())
						pairSrcToTgt.erase(EntityLookupKey(source));
					else {
						srcToTgt[source.id()] = EntityBad;
						srcToTgtIdx[source.id()] = BadIndex;
					}
					GAIA_ASSERT(srcToTgtCnt > 0);
					--srcToTgtCnt;
					return true;
				}

				//! Checks whether the store has no source bindings.
				//! \return True when no source is bound.
				GAIA_NODISCARD bool empty() const {
					return srcToTgtCnt == 0;
				}
			};
		} // namespace detail
	} // namespace ecs
} // namespace gaia

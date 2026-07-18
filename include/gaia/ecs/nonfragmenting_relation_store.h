#pragma once
#include "gaia/config/config.h"

#include <cstdint>

#include "gaia/cnt/darray.h"
#include "gaia/core/utility.h"
#include "gaia/ecs/id.h"

namespace gaia {
	namespace ecs {
		namespace detail {
			//! Storage for exclusive relation pairs that do not fragment archetypes.
			struct NonFragmentingRelationStore {
			private:
				//! Direct source-entity-id indexed target lookup. EntityBad means no binding for that source.
				cnt::darray<Entity> srcToTgt;
				//! Direct source-entity-id indexed position in the current target's source list.
				cnt::darray<uint32_t> srcToTgtIdx;
				//! Number of active source bindings in srcToTgt.
				uint32_t srcToTgtCnt = 0;
				//! Direct target-entity-id indexed source buckets used for traversal and wildcard operations.
				cnt::darray<cnt::darray<Entity>> tgtToSrc;

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

				//! Removes \a source from the source bucket for \a target.
				//! \param target Target entity.
				//! \param source Source entity.
				void remove_target_source(Entity target, Entity source) {
					GAIA_ASSERT(target.id() < tgtToSrc.size());
					if (target.id() >= tgtToSrc.size())
						return;

					auto& sources = tgtToSrc[target.id()];
					const auto idx = source.id() < srcToTgtIdx.size() ? srcToTgtIdx[source.id()] : BadIndex;
					GAIA_ASSERT(idx != BadIndex && idx < sources.size());
					if (idx == BadIndex || idx >= sources.size())
						return;

					const auto lastIdx = (uint32_t)sources.size() - 1;
					if (idx != lastIdx) {
						const auto movedSource = sources[lastIdx];
						sources[idx] = movedSource;
						GAIA_ASSERT(movedSource.id() < srcToTgtIdx.size());
						srcToTgtIdx[movedSource.id()] = idx;
					}

					sources.pop_back();
				}

				//! Binds \a source to \a target.
				//! \param source Source entity.
				//! \param target Target entity.
				//! \return True when the stored binding changed.
				GAIA_NODISCARD bool set(Entity source, Entity target) {
					ensure_source_capacity(source);
					const auto oldTarget = srcToTgt[source.id()];
					if (oldTarget != EntityBad) {
						if (oldTarget == target)
							return false;

						remove_target_source(oldTarget, source);
					} else {
						++srcToTgtCnt;
					}

					ensure_target_capacity(target);
					auto& sources = tgtToSrc[target.id()];
					srcToTgt[source.id()] = target;
					srcToTgtIdx[source.id()] = (uint32_t)sources.size();
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
					srcToTgt[source.id()] = EntityBad;
					srcToTgtIdx[source.id()] = BadIndex;
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

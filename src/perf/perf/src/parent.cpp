#include "common.h"
#include "registry.h"

template <bool UseParent>
void add_hierarchy_edge(ecs::World& w, ecs::Entity entity, ecs::Entity parent) {
	if constexpr (UseParent)
		w.parent(entity, parent);
	else
		w.add(entity, ecs::Pair(ecs::ChildOf, parent));
}

inline void add_dependency_edge(ecs::World& w, ecs::Entity entity, ecs::Entity dependency) {
	w.add(entity, ecs::Pair(ecs::DependsOn, dependency));
}

template <bool UseParent>
void create_hierarchy_tree(
		ecs::World& w, cnt::darray<ecs::Entity>& entities, uint32_t count, uint32_t branchingFactor = 4U) {
	entities.clear();
	entities.reserve(count);
	GAIA_ASSERT(branchingFactor > 0);

	GAIA_FOR(count) {
		const auto e = w.add();
		entities.push_back(e);

		if (i == 0)
			continue;

		const auto parentIdx = (i - 1U) / branchingFactor;
		add_hierarchy_edge<UseParent>(w, e, entities[parentIdx]);
	}
}

template <bool UseParent>
void create_hierarchy_tree_with_position(
		ecs::World& w, cnt::darray<ecs::Entity>& entities, uint32_t count, uint32_t branchingFactor = 4U) {
	create_hierarchy_tree<UseParent>(w, entities, count, branchingFactor);
	for (auto e: entities)
		w.add<Position>(e, {1.0f, 2.0f, 3.0f});
}

static constexpr uint32_t HierarchyBatchLeafCount = 5U;

template <bool UseParent>
void create_hierarchy_batch_tree(ecs::World& w, ecs::Entity scene, uint32_t rootCount, uint64_t& entitySum) {
	GAIA_FOR(rootCount) {
		const auto root = w.add();
		w.add<Position>(root, {1.0f, 2.0f, 3.0f});
		add_hierarchy_edge<UseParent>(w, root, scene);
		entitySum += root.id();

		GAIA_FOR_(HierarchyBatchLeafCount, childIdx) {
			const auto leaf = w.add();
			w.add<Position>(leaf, {(float)(childIdx + 1U), 2.0f, 3.0f});
			add_hierarchy_edge<UseParent>(w, leaf, root);
			entitySum += leaf.id();
		}
	}
}

template <bool UseParent>
void create_hierarchy_batch_tree_multi(ecs::World& w, ecs::Entity scene, uint32_t rootCount, uint64_t& entitySum) {
	GAIA_FOR(rootCount) {
		const auto root = w.add();
		w.add<Position>(root, {1.0f, 2.0f, 3.0f});
		w.add<Velocity>(root, {4.0f, 5.0f, 6.0f});
		w.add<Acceleration>(root, {7.0f, 8.0f, 9.0f});
		add_hierarchy_edge<UseParent>(w, root, scene);
		entitySum += root.id();

		GAIA_FOR_(HierarchyBatchLeafCount, childIdx) {
			const auto leaf = w.add();
			w.add<Position>(leaf, {(float)(childIdx + 1U), 2.0f, 3.0f});
			w.add<Velocity>(leaf, {4.0f, (float)(childIdx + 5U), 6.0f});
			w.add<Acceleration>(leaf, {7.0f, 8.0f, (float)(childIdx + 9U)});
			add_hierarchy_edge<UseParent>(w, leaf, root);
			entitySum += leaf.id();
		}
	}
}

void create_hierarchy_batch_parent_prefab(ecs::World& w, ecs::Entity& rootPrefab) {
	rootPrefab = w.prefab();
	w.add<Position>(rootPrefab, {1.0f, 2.0f, 3.0f});

	GAIA_FOR(HierarchyBatchLeafCount) {
		const auto childPrefab = w.prefab();
		w.add<Position>(childPrefab, {(float)(i + 1U), 2.0f, 3.0f});
		w.parent(childPrefab, rootPrefab);
	}
}

void create_hierarchy_batch_parent_root_prefab(ecs::World& w, ecs::Entity& rootPrefab) {
	rootPrefab = w.prefab();
	w.add<Position>(rootPrefab, {1.0f, 2.0f, 3.0f});
}

void create_hierarchy_batch_childof_prefab(ecs::World& w, ecs::Entity& rootPrefab) {
	rootPrefab = w.prefab();
	w.add<Position>(rootPrefab, {1.0f, 2.0f, 3.0f});

	GAIA_FOR(HierarchyBatchLeafCount) {
		const auto childPrefab = w.prefab();
		w.add<Position>(childPrefab, {(float)(i + 1U), 2.0f, 3.0f});
		w.add(childPrefab, ecs::Pair(ecs::ChildOf, rootPrefab));
	}
}

template <bool UseParent>
void create_hierarchy_batch_prefab_children(
		ecs::World& w, ecs::Entity& rootPrefab, cnt::sarray<ecs::Entity, HierarchyBatchLeafCount>& childPrefabs) {
	rootPrefab = w.prefab();
	w.add<Position>(rootPrefab, {1.0f, 2.0f, 3.0f});

	GAIA_FOR(HierarchyBatchLeafCount) {
		childPrefabs[i] = w.prefab();
		w.add<Position>(childPrefabs[i], {(float)(i + 1U), 2.0f, 3.0f});
		add_hierarchy_edge<UseParent>(w, childPrefabs[i], rootPrefab);
	}
}

template <bool UseParent>
void BM_HierarchyBatch_SpawnManual(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();
	uint64_t total = 0;

	GAIA_FOR((uint32_t)state.iterations()) {
		ecs::World w;
		const auto scene = w.add();
		uint64_t entitySum = 0;
		const auto t0 = std::chrono::steady_clock::now();

		create_hierarchy_batch_tree<UseParent>(w, scene, rootCount, entitySum);

		const auto t1 = std::chrono::steady_clock::now();
		state.add_custom_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
		total += entitySum;
	}

	dont_optimize(total);
}

void BM_HierarchyBatch_SpawnFlatPositions(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();
	uint64_t total = 0;

	GAIA_FOR((uint32_t)state.iterations()) {
		ecs::World w;
		uint64_t entitySum = 0;
		const auto t0 = std::chrono::steady_clock::now();

		GAIA_FOR(rootCount) {
			const auto root = w.add();
			w.add<Position>(root, {1.0f, 2.0f, 3.0f});
			entitySum += root.id();

			GAIA_FOR_(HierarchyBatchLeafCount, childIdx) {
				const auto leaf = w.add();
				w.add<Position>(leaf, {(float)(childIdx + 1U), 2.0f, 3.0f});
				entitySum += leaf.id();
			}
		}

		const auto t1 = std::chrono::steady_clock::now();
		state.add_custom_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
		total += entitySum;
	}

	dont_optimize(total);
}

template <bool UseParent>
void BM_HierarchyBatch_EdgeOnly(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();
	uint64_t total = 0;

	GAIA_FOR((uint32_t)state.iterations()) {
		ecs::World w;
		const auto scene = w.add();
		cnt::darray<ecs::Entity> roots;
		cnt::darray<ecs::Entity> leaves;
		roots.reserve(rootCount);
		leaves.reserve(rootCount * HierarchyBatchLeafCount);
		GAIA_FOR(rootCount) {
			const auto root = w.add();
			roots.push_back(root);
			GAIA_FOR_(HierarchyBatchLeafCount, childIdx) {
				(void)childIdx;
				leaves.push_back(w.add());
			}
		}
		uint64_t entitySum = 0;
		const auto t0 = std::chrono::steady_clock::now();

		uint32_t leafIdx = 0;
		for (auto root: roots) {
			add_hierarchy_edge<UseParent>(w, root, scene);
			entitySum += root.id();
			GAIA_FOR_(HierarchyBatchLeafCount, childIdx) {
				(void)childIdx;
				const auto leaf = leaves[leafIdx++];
				add_hierarchy_edge<UseParent>(w, leaf, root);
				entitySum += leaf.id();
			}
		}

		const auto t1 = std::chrono::steady_clock::now();
		state.add_custom_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
		total += entitySum;
	}

	dont_optimize(total);
}

void BM_HierarchyBatch_ParentEdgeOnlyExistingTargets(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();
	uint64_t total = 0;

	GAIA_FOR((uint32_t)state.iterations()) {
		ecs::World w;
		const auto scene = w.add();
		const auto sceneWarmup = w.add();
		w.parent(sceneWarmup, scene);

		cnt::darray<ecs::Entity> roots;
		cnt::darray<ecs::Entity> leaves;
		cnt::darray<ecs::Entity> warmups;
		roots.reserve(rootCount);
		leaves.reserve(rootCount * HierarchyBatchLeafCount);
		warmups.reserve(rootCount);
		GAIA_FOR(rootCount) {
			const auto root = w.add();
			roots.push_back(root);
			const auto warmup = w.add();
			w.parent(warmup, root);
			warmups.push_back(warmup);
			GAIA_FOR_(HierarchyBatchLeafCount, childIdx) {
				(void)childIdx;
				leaves.push_back(w.add());
			}
		}

		uint64_t entitySum = 0;
		const auto t0 = std::chrono::steady_clock::now();

		uint32_t leafIdx = 0;
		for (auto root: roots) {
			w.parent(root, scene);
			entitySum += root.id();
			GAIA_FOR_(HierarchyBatchLeafCount, childIdx) {
				(void)childIdx;
				const auto leaf = leaves[leafIdx++];
				w.parent(leaf, root);
				entitySum += leaf.id();
			}
		}

		const auto t1 = std::chrono::steady_clock::now();
		state.add_custom_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
		total += entitySum + sceneWarmup.id();
		for (auto warmup: warmups)
			total += warmup.id();
	}

	dont_optimize(total);
}

void BM_HierarchyBatch_ParentTargetPrepare(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();
	uint64_t total = 0;

	GAIA_FOR((uint32_t)state.iterations()) {
		ecs::World w;
		cnt::darray<ecs::Entity> roots;
		cnt::darray<ecs::Entity> warmups;
		roots.reserve(rootCount);
		warmups.reserve(rootCount);
		GAIA_FOR(rootCount) {
			roots.push_back(w.add());
			warmups.push_back(w.add());
		}

		uint64_t entitySum = 0;
		const auto t0 = std::chrono::steady_clock::now();

		GAIA_FOR(rootCount) {
			w.parent(warmups[i], roots[i]);
			entitySum += warmups[i].id();
		}

		const auto t1 = std::chrono::steady_clock::now();
		state.add_custom_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
		total += entitySum;
	}

	dont_optimize(total);
}

void BM_HierarchyBatch_SpawnParentPrefab(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();
	uint64_t total = 0;

	GAIA_FOR((uint32_t)state.iterations()) {
		ecs::World w;
		const auto scene = w.add();
		ecs::Entity rootPrefab = ecs::EntityBad;
		create_hierarchy_batch_parent_prefab(w, rootPrefab);
		uint64_t entitySum = 0;
		const auto t0 = std::chrono::steady_clock::now();

		w.instantiate_n(rootPrefab, scene, rootCount, [&](ecs::Entity instance) {
			entitySum += instance.id();
		});

		const auto t1 = std::chrono::steady_clock::now();
		state.add_custom_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
		total += entitySum;
	}

	dont_optimize(total);
}

//! Benchmarks the manual root-only part of the hierarchy-batch `Parent` spawn shape.
//! This isolates root creation plus the scene-parent edge from the per-root prefab-child work.
void BM_HierarchyBatch_SpawnParentRootOnly(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();
	uint64_t total = 0;

	GAIA_FOR((uint32_t)state.iterations()) {
		ecs::World w;
		const auto scene = w.add();
		uint64_t entitySum = 0;
		const auto t0 = std::chrono::steady_clock::now();

		GAIA_FOR(rootCount) {
			const auto root = w.add();
			w.add<Position>(root, {1.0f, 2.0f, 3.0f});
			w.parent(root, scene);
			entitySum += root.id();
		}

		const auto t1 = std::chrono::steady_clock::now();
		state.add_custom_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
		total += entitySum;
	}

	dont_optimize(total);
}

//! Benchmarks root-only `instantiate_n(...)` under a scene parent for the hierarchy-batch prefab shape.
//! This separates root prefab copy and direct parent batching from prefab child spawning/attachment.
void BM_HierarchyBatch_SpawnParentPrefabRootOnly(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();
	uint64_t total = 0;

	GAIA_FOR((uint32_t)state.iterations()) {
		ecs::World w;
		const auto scene = w.add();
		ecs::Entity rootPrefab = ecs::EntityBad;
		create_hierarchy_batch_parent_root_prefab(w, rootPrefab);
		uint64_t entitySum = 0;
		const auto t0 = std::chrono::steady_clock::now();

		w.instantiate_n(rootPrefab, scene, rootCount, [&](ecs::Entity instance) {
			entitySum += instance.id();
		});

		const auto t1 = std::chrono::steady_clock::now();
		state.add_custom_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
		total += entitySum;
	}

	dont_optimize(total);
}

//! Benchmarks child-to-root hierarchy attachment only for an already-created hierarchy-batch shape.
//! This mirrors the prefab subtree attach phase without timing entity/component creation.
template <bool UseParent>
void BM_HierarchyBatch_PrefabChildAttachOnly(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();
	uint64_t total = 0;

	GAIA_FOR((uint32_t)state.iterations()) {
		ecs::World w;
		cnt::darray<ecs::Entity> roots;
		cnt::darray<ecs::Entity> leaves;
		roots.reserve(rootCount);
		leaves.reserve(rootCount * HierarchyBatchLeafCount);

		GAIA_FOR(rootCount) {
			const auto root = w.add();
			w.add<Position>(root, {1.0f, 2.0f, 3.0f});
			roots.push_back(root);

			GAIA_FOR_(HierarchyBatchLeafCount, childIdx) {
				const auto leaf = w.add();
				w.add<Position>(leaf, {(float)(childIdx + 1U), 2.0f, 3.0f});
				leaves.push_back(leaf);
			}
		}

		uint64_t entitySum = 0;
		const auto t0 = std::chrono::steady_clock::now();

		uint32_t leafIdx = 0;
		for (auto root: roots) {
			GAIA_FOR_(HierarchyBatchLeafCount, childIdx) {
				(void)childIdx;
				const auto leaf = leaves[leafIdx++];
				add_hierarchy_edge<UseParent>(w, leaf, root);
				entitySum += leaf.id();
			}
		}

		const auto t1 = std::chrono::steady_clock::now();
		state.add_custom_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
		total += entitySum;
	}

	dont_optimize(total);
}

//! Benchmarks child prefab instance creation without attaching spawned children to root instances.
//! This isolates prefab child-node copy cost from relation attachment cost.
template <bool UseParent>
void BM_HierarchyBatch_PrefabChildCopyOnly(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();
	uint64_t total = 0;

	GAIA_FOR((uint32_t)state.iterations()) {
		ecs::World w;
		ecs::Entity rootPrefab = ecs::EntityBad;
		cnt::sarray<ecs::Entity, HierarchyBatchLeafCount> childPrefabs{};
		create_hierarchy_batch_prefab_children<UseParent>(w, rootPrefab, childPrefabs);
		uint64_t entitySum = 0;
		const auto t0 = std::chrono::steady_clock::now();

		for (const auto childPrefab: childPrefabs) {
			w.instantiate_n(childPrefab, rootCount, [&](ecs::Entity instance) {
				entitySum += instance.id();
			});
		}

		const auto t1 = std::chrono::steady_clock::now();
		state.add_custom_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
		total += entitySum + rootPrefab.id();
	}

	dont_optimize(total);
}

void BM_HierarchyBatch_SpawnParentPrefabSingle(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();
	uint64_t total = 0;

	GAIA_FOR((uint32_t)state.iterations()) {
		ecs::World w;
		const auto scene = w.add();
		ecs::Entity rootPrefab = ecs::EntityBad;
		create_hierarchy_batch_parent_prefab(w, rootPrefab);
		uint64_t entitySum = 0;
		const auto t0 = std::chrono::steady_clock::now();

		GAIA_FOR(rootCount) {
			const auto instance = w.instantiate(rootPrefab, scene);
			entitySum += instance.id();
		}

		const auto t1 = std::chrono::steady_clock::now();
		state.add_custom_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
		total += entitySum;
	}

	dont_optimize(total);
}

void BM_HierarchyBatch_SpawnParentPrefabCopyIter(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();
	uint64_t total = 0;

	GAIA_FOR((uint32_t)state.iterations()) {
		ecs::World w;
		const auto scene = w.add();
		ecs::Entity rootPrefab = ecs::EntityBad;
		create_hierarchy_batch_parent_prefab(w, rootPrefab);
		uint64_t entitySum = 0;
		const auto t0 = std::chrono::steady_clock::now();

		w.instantiate_n(rootPrefab, scene, rootCount, [&](ecs::CopyIter& it) {
			auto entityView = it.view<ecs::Entity>();
			GAIA_EACH(it) {
				entitySum += entityView[i].id();
			}
		});

		const auto t1 = std::chrono::steady_clock::now();
		state.add_custom_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
		total += entitySum;
	}

	dont_optimize(total);
}

//! Benchmarks batched prefab-tree spawning when the child prefab edges use fragmenting `ChildOf`.
//! Root instances are still attached to the scene through the public parented `instantiate_n(...)` API.
void BM_HierarchyBatch_SpawnChildOfPrefab(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();
	uint64_t total = 0;

	GAIA_FOR((uint32_t)state.iterations()) {
		ecs::World w;
		const auto scene = w.add();
		ecs::Entity rootPrefab = ecs::EntityBad;
		create_hierarchy_batch_childof_prefab(w, rootPrefab);
		uint64_t entitySum = 0;
		const auto t0 = std::chrono::steady_clock::now();

		w.instantiate_n(rootPrefab, scene, rootCount, [&](ecs::Entity instance) {
			entitySum += instance.id();
		});

		const auto t1 = std::chrono::steady_clock::now();
		state.add_custom_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
		total += entitySum;
	}

	dont_optimize(total);
}

//! Benchmarks the `CopyIter` callback shape for a prefab tree whose child edges use `ChildOf`.
//! This keeps the comparison with `Parent` prefab children callback-equivalent.
void BM_HierarchyBatch_SpawnChildOfPrefabCopyIter(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();
	uint64_t total = 0;

	GAIA_FOR((uint32_t)state.iterations()) {
		ecs::World w;
		const auto scene = w.add();
		ecs::Entity rootPrefab = ecs::EntityBad;
		create_hierarchy_batch_childof_prefab(w, rootPrefab);
		uint64_t entitySum = 0;
		const auto t0 = std::chrono::steady_clock::now();

		w.instantiate_n(rootPrefab, scene, rootCount, [&](ecs::CopyIter& it) {
			auto entityView = it.view<ecs::Entity>();
			GAIA_EACH(it) {
				entitySum += entityView[i].id();
			}
		});

		const auto t1 = std::chrono::steady_clock::now();
		state.add_custom_duration(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
		total += entitySum;
	}

	dont_optimize(total);
}

template <bool UseParent>
void BM_HierarchyBatch_QueryPlain(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();

	ecs::World w;
	const auto scene = w.add();
	uint64_t entitySum = 0;
	create_hierarchy_batch_tree<UseParent>(w, scene, rootCount, entitySum);

	auto q = w.query().all<Position>();
	float sum = 0.0f;

	q.each([&](const Position& pos) {
		sum += pos.x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](const Position& pos) {
			sum += pos.x;
		});
	}

	dont_optimize(sum + (float)entitySum);
}

void BM_HierarchyBatch_QueryChildOfDepthOrder(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();

	ecs::World w;
	const auto scene = w.add();
	uint64_t entitySum = 0;
	create_hierarchy_batch_tree<false>(w, scene, rootCount, entitySum);

	auto q = w.query().all<Position>().depth_order(ecs::ChildOf);
	float sum = 0.0f;

	q.each([&](const Position& pos) {
		sum += pos.x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](const Position& pos) {
			sum += pos.x;
		});
	}

	dont_optimize(sum + (float)entitySum);
}

template <bool UseParent, bool DepthOrder>
void BM_HierarchyBatch_QueryMulti(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();

	ecs::World w;
	const auto scene = w.add();
	uint64_t entitySum = 0;
	create_hierarchy_batch_tree_multi<UseParent>(w, scene, rootCount, entitySum);

	auto q = w.query().all<Position>().all<Velocity>().all<Acceleration>();
	if constexpr (DepthOrder)
		q.depth_order(ecs::ChildOf);
	float sum = 0.0f;

	q.each([&](const Acceleration& acc, const Velocity& vel, const Position& pos) {
		sum += acc.x + vel.y + pos.z;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](const Acceleration& acc, const Velocity& vel, const Position& pos) {
			sum += acc.x + vel.y + pos.z;
		});
	}

	dont_optimize(sum + (float)entitySum);
}

void BM_HierarchyBatch_QueryParentOrderBy(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();

	ecs::World w;
	const auto scene = w.add();
	uint64_t entitySum = 0;
	create_hierarchy_batch_tree<true>(w, scene, rootCount, entitySum);

	auto q = w.query().all<Position>();
	float sum = 0.0f;

	q.order_by(ecs::Parent, ecs::TravOrder::Down).each([&](ecs::Entity, const Position& pos) {
		sum += pos.x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.order_by(ecs::Parent, ecs::TravOrder::Down).each([&](ecs::Entity, const Position& pos) {
			sum += pos.x;
		});
	}

	dont_optimize(sum + (float)entitySum);
}

template <bool UseParent>
void BM_HierarchyBatch_QueryIterBatchesPlain(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();

	ecs::World w;
	const auto scene = w.add();
	uint64_t entitySum = 0;
	create_hierarchy_batch_tree<UseParent>(w, scene, rootCount, entitySum);

	auto q = w.query().all<Position>();
	uint64_t visits = 0;

	q.each([&](ecs::Iter& it) {
		++visits;
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			visits += (uint64_t)posView[i].x;
		}
	});
	visits = 0;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Iter& it) {
			++visits;
			auto posView = it.view<Position>();
			GAIA_EACH(it) {
				visits += (uint64_t)posView[i].x;
			}
		});
	}

	dont_optimize(visits + entitySum);
}

template <bool UseParent, bool DepthOrder, bool UsePtr>
void BM_HierarchyBatch_QueryChunkRaw(picobench::state& state) {
	const uint32_t rootCount = (uint32_t)state.user_data();

	ecs::World w;
	const auto scene = w.add();
	uint64_t entitySum = 0;
	create_hierarchy_batch_tree<UseParent>(w, scene, rootCount, entitySum);

	auto q = w.query().all<Position>();
	if constexpr (DepthOrder) {
		if constexpr (UseParent)
			q.order_by(ecs::Parent, ecs::TravOrder::Down);
		else
			q.depth_order(ecs::ChildOf);
	}
	auto& queryInfo = q.fetch();
	q.match_all(queryInfo);
	const auto posComp = w.get<Position>();
	float sum = 0.0f;

	for (auto _: state) {
		(void)_;

		for (const auto* pArchetype: queryInfo.cache_archetype_view()) {
			const auto& chunks = pArchetype->chunks();
			for (auto* pChunk: chunks) {
				const auto from = ecs::Iter::start_index(pChunk);
				const auto to = ecs::Iter::end_index(pChunk);
				if (from == to)
					continue;

				if constexpr (UsePtr) {
					const auto recs = pChunk->comp_rec_view();
					const auto* p = (const Position*)recs[pChunk->comp_idx(posComp)].pData + from;
					const auto cnt = (uint32_t)(to - from);
					GAIA_FOR(cnt) {
						sum += p[i].x;
					}
				} else {
					auto posView = pChunk->view<Position>(from, to);
					const auto cnt = (uint32_t)(to - from);
					GAIA_FOR(cnt) {
						sum += posView[i].x;
					}
				}
			}
		}
	}

	dont_optimize(sum + (float)entitySum);
}

void create_dependency_tree(
		ecs::World& w, cnt::darray<ecs::Entity>& entities, uint32_t count, uint32_t branchingFactor = 4U) {
	entities.clear();
	entities.reserve(count);
	GAIA_ASSERT(branchingFactor > 0);

	GAIA_FOR(count) {
		const auto e = w.add();
		entities.push_back(e);

		if (i == 0)
			continue;

		const auto dependencyIdx = (i - 1U) / branchingFactor;
		add_dependency_edge(w, e, entities[dependencyIdx]);
	}
}

void create_dependency_tree_with_position(
		ecs::World& w, cnt::darray<ecs::Entity>& entities, uint32_t count, uint32_t branchingFactor = 4U) {
	create_dependency_tree(w, entities, count, branchingFactor);
	for (auto e: entities)
		w.add<Position>(e, {1.0f, 2.0f, 3.0f});
}

inline void disable_hierarchy_barrier(ecs::World& w, const cnt::darray<ecs::Entity>& entities) {
	if (entities.size() > 1)
		w.enable(entities[1], false);
}

template <bool UseParent>
void BM_Hierarchy_Set(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> entities;
		entities.reserve(n);

		const auto root = w.add();
		GAIA_FOR(n) {
			const auto e = w.add();
			entities.push_back(e);
		}

		state.start_timer();
		for (auto e: entities)
			add_hierarchy_edge<UseParent>(w, e, root);
		state.stop_timer();
	}
}

template <bool UseParent, bool WithDisabledBarrier = false>
void BM_Hierarchy_Traversal(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree<UseParent>(w, entities, n);
	if constexpr (WithDisabledBarrier)
		disable_hierarchy_barrier(w, entities);

	uint64_t visited = 0;
	auto relation = UseParent ? ecs::Parent : ecs::ChildOf;

	w.sources_bfs(relation, entities[0], [&](ecs::Entity) {
		++visited;
	});
	visited = 0;

	for (auto _: state) {
		(void)_;

		w.sources_bfs(relation, entities[0], [&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

template <bool UseParent, bool WithDisabledBarrier = false>
void BM_Query_Traversal(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<UseParent>(w, entities, n);
	if constexpr (WithDisabledBarrier)
		disable_hierarchy_barrier(w, entities);

	const auto relation = UseParent ? ecs::Parent : ecs::ChildOf;
	auto q = w.query().all<Position>();
	uint64_t visited = 0;

	q.order_by(relation, ecs::TravOrder::Down).each([&](ecs::Entity) {
		++visited;
	});
	visited = 0;

	for (auto _: state) {
		(void)_;

		q.order_by(relation, ecs::TravOrder::Down).each([&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

void BM_Query_Plain_ChildOf(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>();
	uint64_t visited = 0;

	q.each([&](ecs::Entity) {
		++visited;
	});
	visited = 0;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

void BM_Query_Cascade_ChildOf(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>().depth_order(ecs::ChildOf);
	uint64_t visited = 0;

	q.each([&](ecs::Entity) {
		++visited;
	});
	visited = 0;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

void BM_Query_Cascade_ChildOf_Disabled(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);
	disable_hierarchy_barrier(w, entities);

	auto q = w.query().all<Position>().depth_order(ecs::ChildOf);
	uint64_t visited = 0;

	q.each([&](ecs::Entity) {
		++visited;
	});
	visited = 0;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

template <uint32_t BranchingFactor>
void BM_Query_DepthOrder_ChildOf_Fanout(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n, BranchingFactor);

	auto q = w.query().all<Position>().depth_order(ecs::ChildOf);
	uint64_t visited = 0;

	q.each([&](ecs::Entity) {
		++visited;
	});
	visited = 0;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

template <uint32_t BranchingFactor>
void BM_Query_Traversal_DependsOn(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_dependency_tree_with_position(w, entities, n, BranchingFactor);

	auto q = w.query().all<Position>();
	uint64_t visited = 0;

	q.order_by(ecs::DependsOn, ecs::TravOrder::Down).each([&](ecs::Entity) {
		++visited;
	});
	visited = 0;

	for (auto _: state) {
		(void)_;

		q.order_by(ecs::DependsOn, ecs::TravOrder::Down).each([&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

template <uint32_t BranchingFactor>
void BM_Query_DepthOrder_DependsOn(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_dependency_tree_with_position(w, entities, n, BranchingFactor);

	auto q = w.query().all<Position>().depth_order(ecs::DependsOn);
	uint64_t visited = 0;

	q.each([&](ecs::Entity) {
		++visited;
	});
	visited = 0;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

template <uint32_t BranchingFactor>
void BM_Query_DepthOrder_ChildOf_IterEmpty(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n, BranchingFactor);

	auto q = w.query().all<Position>().depth_order(ecs::ChildOf);
	uint64_t chunkVisits = 0;

	q.each([&](ecs::Iter& it) {
		chunkVisits += it.size() != 0 ? 1U : 0U;
	});
	chunkVisits = 0;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Iter& it) {
			chunkVisits += it.size() != 0 ? 1U : 0U;
		});
	}

	dont_optimize(chunkVisits);
}

template <uint32_t BranchingFactor>
void BM_Query_DepthOrder_DependsOn_IterEmpty(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_dependency_tree_with_position(w, entities, n, BranchingFactor);

	auto q = w.query().all<Position>().depth_order(ecs::DependsOn);
	uint64_t chunkVisits = 0;

	q.each([&](ecs::Iter& it) {
		chunkVisits += it.size() != 0 ? 1U : 0U;
	});
	chunkVisits = 0;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Iter& it) {
			chunkVisits += it.size() != 0 ? 1U : 0U;
		});
	}

	dont_optimize(chunkVisits);
}

void BM_Query_Plain_ChildOf_Component(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>();
	float sum = 0.0f;

	q.each([&](ecs::Entity entity) {
		sum += w.get<Position>(entity).x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity entity) {
			sum += w.get<Position>(entity).x;
		});
	}

	dont_optimize(sum);
}

void BM_Query_Traversal_ChildOf_Component(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>();
	float sum = 0.0f;

	q.order_by(ecs::ChildOf, ecs::TravOrder::Down).each([&](ecs::Entity entity) {
		sum += w.get<Position>(entity).x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.order_by(ecs::ChildOf, ecs::TravOrder::Down).each([&](ecs::Entity entity) {
			sum += w.get<Position>(entity).x;
		});
	}

	dont_optimize(sum);
}

void BM_Query_Cascade_ChildOf_Component(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>().depth_order(ecs::ChildOf);
	float sum = 0.0f;

	q.each([&](ecs::Entity entity) {
		sum += w.get<Position>(entity).x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity entity) {
			sum += w.get<Position>(entity).x;
		});
	}

	dont_optimize(sum);
}

void BM_Query_Plain_ChildOf_EachComponent(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>();
	float sum = 0.0f;

	q.each([&](ecs::Entity, const Position& pos) {
		sum += pos.x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity, const Position& pos) {
			sum += pos.x;
		});
	}

	dont_optimize(sum);
}

void BM_Query_Traversal_ChildOf_EachComponent(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>();
	float sum = 0.0f;

	q.order_by(ecs::ChildOf, ecs::TravOrder::Down).each([&](ecs::Entity, const Position& pos) {
		sum += pos.x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.order_by(ecs::ChildOf, ecs::TravOrder::Down).each([&](ecs::Entity, const Position& pos) {
			sum += pos.x;
		});
	}

	dont_optimize(sum);
}

void BM_Query_Cascade_ChildOf_EachComponent(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>().depth_order(ecs::ChildOf);
	float sum = 0.0f;

	q.each([&](ecs::Entity, const Position& pos) {
		sum += pos.x;
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Entity, const Position& pos) {
			sum += pos.x;
		});
	}

	dont_optimize(sum);
}

void BM_Query_Plain_ChildOf_Iter(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>();
	float sum = 0.0f;

	q.each([&](ecs::Iter& it) {
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			sum += posView[i].x;
		}
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Iter& it) {
			auto posView = it.view<Position>();
			GAIA_EACH(it) {
				sum += posView[i].x;
			}
		});
	}

	dont_optimize(sum);
}

void BM_Query_Traversal_ChildOf_Iter(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>();
	float sum = 0.0f;

	q.order_by(ecs::ChildOf, ecs::TravOrder::Down).each([&](ecs::Iter& it) {
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			sum += posView[i].x;
		}
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.order_by(ecs::ChildOf, ecs::TravOrder::Down).each([&](ecs::Iter& it) {
			auto posView = it.view<Position>();
			GAIA_EACH(it) {
				sum += posView[i].x;
			}
		});
	}

	dont_optimize(sum);
}

void BM_Query_Cascade_ChildOf_Iter(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree_with_position<false>(w, entities, n);

	auto q = w.query().all<Position>().depth_order(ecs::ChildOf);
	float sum = 0.0f;

	q.each([&](ecs::Iter& it) {
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			sum += posView[i].x;
		}
	});
	sum = 0.0f;

	for (auto _: state) {
		(void)_;

		q.each([&](ecs::Iter& it) {
			auto posView = it.view<Position>();
			GAIA_EACH(it) {
				sum += posView[i].x;
			}
		});
	}

	dont_optimize(sum);
}

void BM_Hierarchy_Traversal_ChildOf_Disabled(picobench::state& state) {
	BM_Hierarchy_Traversal<false, true>(state);
}

void BM_Hierarchy_Traversal_Parent_Disabled(picobench::state& state) {
	BM_Hierarchy_Traversal<true, true>(state);
}

void BM_Query_Traversal_ChildOf_Disabled(picobench::state& state) {
	BM_Query_Traversal<false, true>(state);
}

void BM_Query_Traversal_Parent_Disabled(picobench::state& state) {
	BM_Query_Traversal<true, true>(state);
}

template <bool UseParent>
void BM_Hierarchy_TargetWalk(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree<UseParent>(w, entities, n);

	const auto relation = UseParent ? ecs::Parent : ecs::ChildOf;
	const auto leaf = entities.back();
	uint64_t steps = 0;

	for (auto _: state) {
		(void)_;

		auto curr = leaf;
		while (true) {
			const auto parent = w.target(curr, relation);
			if (parent == ecs::EntityBad)
				break;

			curr = parent;
			++steps;
		}
	}

	dont_optimize(steps);
}

template <bool UseParent>
void BM_Hierarchy_Sources(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_hierarchy_tree<UseParent>(w, entities, n);

	const auto relation = UseParent ? ecs::Parent : ecs::ChildOf;
	uint64_t visited = 0;

	for (auto _: state) {
		(void)_;

		w.sources(relation, entities[0], [&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

void BM_Relationship_SourcesWildcard(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	cnt::darray<ecs::Entity> entities;
	create_linear_entities<false, false, false, false, false>(w, entities, n);

	const auto root = w.add();
	const auto rel0 = w.add();
	const auto rel1 = w.add();
	const auto rel2 = w.add();
	const ecs::Entity rels[] = {rel0, rel1, rel2};

	const auto cnt = (uint32_t)entities.size();
	GAIA_FOR(cnt) {
		w.add(entities[i], ecs::Pair(rels[i % 3U], root));
	}

	uint64_t visited = 0;
	for (auto _: state) {
		(void)_;

		w.sources(ecs::All, root, [&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

void BM_Relationship_TargetsWildcard(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	const auto source = w.add();

	cnt::darray<ecs::Entity> targets;
	targets.reserve(n);
	GAIA_FOR(n) {
		// A single entity can't legally fragment into 10K pair ids because archetypes cap the
		// number of stored terms. Use Exclusive+DontFragment relations so the wildcard helper still
		// traverses 10K direct targets without inflating the source archetype.
		const auto rel = w.add();
		w.add(rel, ecs::Exclusive);
		w.add(rel, ecs::DontFragment);
		const auto target = w.add();
		targets.push_back(target);
		w.add(source, ecs::Pair(rel, target));
	}

	uint64_t visited = 0;
	for (auto _: state) {
		(void)_;

		w.targets(source, ecs::All, [&](ecs::Entity) {
			++visited;
		});
	}

	dont_optimize(visited);
}

template <bool UseParent>
void BM_Hierarchy_DeleteTarget(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		cnt::darray<ecs::Entity> entities;
		entities.reserve(n);

		const auto root = w.add();
		GAIA_FOR(n) {
			const auto e = w.add();
			entities.push_back(e);
			add_hierarchy_edge<UseParent>(w, e, root);
		}

		state.start_timer();
		w.del(root);
		w.update();
		state.stop_timer();
	}
}

template <bool UseParent>
void BM_Query_DirectHierarchy_All(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	const auto rootA = w.add();
	const auto rootB = w.add();

	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	GAIA_FOR(n) {
		const auto e = w.add();
		entities.push_back(e);
		w.add<Position>(e);
		add_hierarchy_edge<UseParent>(w, e, (i & 1U) == 0 ? rootA : rootB);
	}

	auto q = w.query().all<Position>().all(ecs::Pair(UseParent ? ecs::Parent : ecs::ChildOf, rootA));
	uint32_t total = 0;

	for (auto _: state) {
		(void)_;
		total += q.count();
	}

	dont_optimize(total);
}

template <bool UseParent>
void BM_Query_DirectHierarchy_Each(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	const auto rootA = w.add();
	const auto rootB = w.add();

	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	GAIA_FOR(n) {
		const auto e = w.add();
		entities.push_back(e);
		w.add<Position>(e, {(float)i, (float)(i + 1U), (float)(i + 2U)});
		add_hierarchy_edge<UseParent>(w, e, (i & 1U) == 0 ? rootA : rootB);
	}

	auto q = w.query().all<Position>().all(ecs::Pair(UseParent ? ecs::Parent : ecs::ChildOf, rootA));
	uint64_t total = 0;

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0;
		q.each([&](const Position& p) {
			sum += (uint64_t)(p.x + p.y + p.z);
		});
		total += sum;
	}

	dont_optimize(total);
}

template <bool UseParent>
void BM_Query_DirectHierarchy_Or(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	const auto rootA = w.add();
	const auto rootB = w.add();

	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	GAIA_FOR(n) {
		const auto e = w.add();
		entities.push_back(e);
		if ((i & 1U) == 0)
			add_hierarchy_edge<UseParent>(w, e, rootA);
		else
			add_hierarchy_edge<UseParent>(w, e, rootB);

		if ((i % 4U) == 1U)
			w.add<Acceleration>(e);
	}

	auto q = w.query().or_(ecs::Pair(UseParent ? ecs::Parent : ecs::ChildOf, rootA)).or_<Acceleration>();
	uint32_t total = 0;

	for (auto _: state) {
		(void)_;
		total += q.count();
	}

	dont_optimize(total);
}

template <bool UseParent>
void BM_Query_DirectHierarchy_Or_Each(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	ecs::World w;
	const auto rootA = w.add();
	const auto rootB = w.add();

	cnt::darray<ecs::Entity> entities;
	entities.reserve(n);

	GAIA_FOR(n) {
		const auto e = w.add();
		entities.push_back(e);
		w.add<Position>(e, {(float)i, (float)(i + 1U), (float)(i + 2U)});
		if ((i & 1U) == 0)
			add_hierarchy_edge<UseParent>(w, e, rootA);
		else
			add_hierarchy_edge<UseParent>(w, e, rootB);

		if ((i % 4U) == 1U)
			w.add<Acceleration>(e);
	}

	auto q = w.query().all<Position>().or_(ecs::Pair(UseParent ? ecs::Parent : ecs::ChildOf, rootA)).or_<Acceleration>();
	uint64_t total = 0;

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0;
		q.each([&](const Position& p) {
			sum += (uint64_t)(p.x + p.y + p.z);
		});
		total += sum;
	}

	dont_optimize(total);
}

//! Benchmarks transitive target traversal over `Is` targets.
template <uint32_t ChainDepth>
void BM_World_AsTargetsTrav(picobench::state& state) {
	ecs::World w;

	cnt::sarray<ecs::Entity, ChainDepth> chain{};
	GAIA_FOR(ChainDepth) {
		chain[i] = w.add();
		if (i == 0)
			continue;
		w.add(chain[i], ecs::Pair(ecs::Is, chain[i - 1]));
	}

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0;
		w.as_targets_trav(chain[ChainDepth - 1], [&](ecs::Entity target) {
			sum += target.id();
		});
		dont_optimize(sum);
	}
}

void BM_World_AsTargetsTrav_2(picobench::state& state) {
	BM_World_AsTargetsTrav<2>(state);
}

void BM_World_AsTargetsTrav_4(picobench::state& state) {
	BM_World_AsTargetsTrav<4>(state);
}

void BM_World_AsTargetsTrav_8(picobench::state& state) {
	BM_World_AsTargetsTrav<8>(state);
}

void BM_World_AsTargetsTrav_32(picobench::state& state) {
	BM_World_AsTargetsTrav<32>(state);
}

//! Benchmarks deleting wildcard pairs matching (*, target) across many relations.
//! The delete loop should avoid copying the full relation set before removing pairs.
void BM_World_Delete_Wildcard_Target(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		ecs::World w;
		const auto target = w.add();
		cnt::darray<ecs::Entity> rels;
		rels.reserve(n);

		for (uint32_t i = 0; i < n; ++i) {
			auto rel = w.add();
			rels.push_back(rel);
			auto src = w.add();
			w.add(src, ecs::Pair(rel, target));
		}

		w.del(ecs::Pair(ecs::All, target));
		w.update();
		dont_optimize(rels.size());
	}
}

//! Benchmarks cached wildcard-pair query maintenance while building a pair-heavy archetype.
//! This exercises incremental query registration against archetypes that repeat the same relation
//! across many pair ids, which would otherwise create duplicate wildcard lookup keys.
void BM_QueryCache_RegisterPairHeavy_RelWildcard(picobench::state& state) {
	const uint32_t pairCnt = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;

		state.stop_timer();
		ecs::World w;
		const auto rel = w.add();
		cnt::darray<ecs::Entity> targets;
		targets.reserve(pairCnt);
		GAIA_FOR(pairCnt) {
			targets.push_back(w.add());
		}

		auto q = w.query().all(ecs::Pair(rel, ecs::All));
		dont_optimize(q.count());

		const auto e = w.add();
		state.start_timer();

		GAIA_FOR(pairCnt) {
			w.add(e, ecs::Pair(rel, targets[i]));
		}

		dont_optimize(e);
	}
}

////////////////////////////////////////////////////////////////////////////////

void BM_Hierarchy_Traversal_ChildOf_Disabled(picobench::state& state);
void BM_Hierarchy_Traversal_Parent_Disabled(picobench::state& state);
void BM_Hierarchy_DeleteTarget(picobench::state& state);
void BM_Hierarchy_Set(picobench::state& state);
void BM_Hierarchy_Sources(picobench::state& state);
void BM_Hierarchy_TargetWalk(picobench::state& state);
void BM_Query_Traversal(picobench::state& state);
void BM_Query_Traversal_ChildOf_Component(picobench::state& state);
void BM_Query_Traversal_ChildOf_Disabled(picobench::state& state);
void BM_Query_Traversal_ChildOf_EachComponent(picobench::state& state);
void BM_Query_Traversal_ChildOf_Iter(picobench::state& state);
void BM_Query_Traversal_Parent_Disabled(picobench::state& state);
void BM_Query_Cascade_ChildOf(picobench::state& state);
void BM_Query_Cascade_ChildOf_Component(picobench::state& state);
void BM_Query_Cascade_ChildOf_Disabled(picobench::state& state);
void BM_Query_Cascade_ChildOf_EachComponent(picobench::state& state);
void BM_Query_Cascade_ChildOf_Iter(picobench::state& state);
void BM_Query_DepthOrder_ChildOf_Fanout(picobench::state& state);
void BM_Query_DepthOrder_ChildOf_IterEmpty(picobench::state& state);
void BM_Query_DepthOrder_DependsOn(picobench::state& state);
void BM_Query_DepthOrder_DependsOn_IterEmpty(picobench::state& state);
void BM_Query_DirectHierarchy_All(picobench::state& state);
void BM_Query_DirectHierarchy_Each(picobench::state& state);
void BM_Query_DirectHierarchy_Or(picobench::state& state);
void BM_Query_DirectHierarchy_Or_Each(picobench::state& state);
void BM_Query_Plain_ChildOf(picobench::state& state);
void BM_Query_Plain_ChildOf_Component(picobench::state& state);
void BM_Query_Plain_ChildOf_EachComponent(picobench::state& state);
void BM_Query_Plain_ChildOf_Iter(picobench::state& state);
void BM_Query_Traversal_DependsOn(picobench::state& state);
void BM_Relationship_SourcesWildcard(picobench::state& state);
void BM_Relationship_TargetsWildcard(picobench::state& state);

void register_parent(PerfRunMode mode) {
	if (mode != PerfRunMode::Normal)
		return;

	PICOBENCH_SUITE_REG("Structural changes");
	PICOBENCH_REG(BM_Hierarchy_Set<false>).PICO_SETTINGS_FOCUS().user_data(NEntitiesFew).label("childof set 10K");
	PICOBENCH_REG(BM_Hierarchy_Set<true>).PICO_SETTINGS_FOCUS().user_data(NEntitiesFew).label("parent set 10K");

	PICOBENCH_SUITE_REG("Hierarchy batch relations");
	PICOBENCH_REG(BM_HierarchyBatch_SpawnManual<false>)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch childof spawn 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_SpawnManual<true>)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent spawn 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_SpawnFlatPositions)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch flat position spawn 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_EdgeOnly<false>)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch childof edge only 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_EdgeOnly<true>)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent edge only 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_ParentEdgeOnlyExistingTargets)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent existing-target edge only 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_ParentTargetPrepare)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent target prep 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_SpawnParentPrefab)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent prefab spawn 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_SpawnParentRootOnly)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent root spawn 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_SpawnParentPrefabRootOnly)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent prefab root spawn 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_PrefabChildAttachOnly<true>)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent prefab child attach only 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_PrefabChildAttachOnly<false>)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch childof prefab child attach only 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_PrefabChildCopyOnly<true>)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent prefab child copy only 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_PrefabChildCopyOnly<false>)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch childof prefab child copy only 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_SpawnParentPrefabSingle)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent prefab single spawn 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_SpawnParentPrefabCopyIter)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent prefab copyiter spawn 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_SpawnChildOfPrefab)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch childof prefab spawn 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_SpawnChildOfPrefabCopyIter)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch childof prefab copyiter spawn 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_QueryPlain<false>)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch childof plain query 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_QueryPlain<true>)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent plain query 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_QueryChildOfDepthOrder)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch childof depth_order query 40K roots");
	PICOBENCH_REG((BM_HierarchyBatch_QueryMulti<false, false>))
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch childof multi query 40K roots");
	PICOBENCH_REG((BM_HierarchyBatch_QueryMulti<false, true>))
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch childof depth_order multi query 40K roots");
	PICOBENCH_REG((BM_HierarchyBatch_QueryMulti<true, false>))
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent multi query 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_QueryParentOrderBy)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent order_by query 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_QueryIterBatchesPlain<false>)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch childof iter plain 40K roots");
	PICOBENCH_REG(BM_HierarchyBatch_QueryIterBatchesPlain<true>)
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent iter plain 40K roots");
	PICOBENCH_REG((BM_HierarchyBatch_QueryChunkRaw<false, false, false>))
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch childof chunk raw query 40K roots");
	PICOBENCH_REG((BM_HierarchyBatch_QueryChunkRaw<false, false, true>))
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch childof chunk ptr raw query 40K roots");
	PICOBENCH_REG((BM_HierarchyBatch_QueryChunkRaw<false, true, true>))
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch childof depth_order chunk ptr raw query 40K roots");
	PICOBENCH_REG((BM_HierarchyBatch_QueryChunkRaw<true, false, true>))
			.PICO_SETTINGS_HEAVY()
			.user_data(40'000)
			.label("hierarchy batch parent chunk ptr raw query 40K roots");

	PICOBENCH_SUITE_REG("Structural changes");
	PICOBENCH_REG(BM_Query_DirectHierarchy_All<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof query all 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_All<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent query all 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_Each<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof query each 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_Each<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent query each 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_Or<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof query or 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_Or<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent query or 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_Or_Each<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof query or each 10K");
	PICOBENCH_REG(BM_Query_DirectHierarchy_Or_Each<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent query or each 10K");

	PICOBENCH_SUITE_REG("Traversal helpers");
	PICOBENCH_REG(BM_Hierarchy_TargetWalk<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof up traversal 10K");
	PICOBENCH_REG(BM_Hierarchy_TargetWalk<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent up traversal 10K");
	PICOBENCH_REG(BM_Hierarchy_Sources<false>).PICO_SETTINGS_FOCUS().user_data(NEntitiesFew).label("childof sources 10K");
	PICOBENCH_REG(BM_Hierarchy_Sources<true>).PICO_SETTINGS_FOCUS().user_data(NEntitiesFew).label("parent sources 10K");
	PICOBENCH_REG(BM_Relationship_SourcesWildcard)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("wildcard sources 10K");
	PICOBENCH_REG(BM_Relationship_TargetsWildcard)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("wildcard targets 10K");
	PICOBENCH_REG(BM_Hierarchy_Traversal<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof traversal 10K");
	PICOBENCH_REG(BM_Hierarchy_Traversal<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent traversal 10K");
	PICOBENCH_REG(BM_Hierarchy_Traversal_ChildOf_Disabled)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof traversal disabled barrier 10K");
	PICOBENCH_REG(BM_Hierarchy_Traversal_Parent_Disabled)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent traversal disabled barrier 10K");
	PICOBENCH_REG(BM_Query_Plain_ChildOf).PICO_SETTINGS_FOCUS().user_data(NEntitiesFew).label("query childof plain 10K");
	PICOBENCH_REG(BM_Query_Plain_ChildOf_Component)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof plain get 10K");
	PICOBENCH_REG(BM_Query_Traversal<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof traversal 10K");
	PICOBENCH_REG(BM_Query_Traversal_ChildOf_Component)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof traversal get 10K");
	PICOBENCH_REG(BM_Query_Traversal<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query parent traversal 10K");
	PICOBENCH_REG(BM_Query_Cascade_ChildOf)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof depth_order 10K");
	PICOBENCH_REG(BM_Query_DepthOrder_ChildOf_Fanout<1U>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof depth_order chain 10K");
	PICOBENCH_REG(BM_Query_DepthOrder_ChildOf_Fanout<64U>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof depth_order wide 10K");
	PICOBENCH_REG(BM_Query_DepthOrder_ChildOf_IterEmpty<4U>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof depth_order iter empty 10K");
	PICOBENCH_REG(BM_Query_DepthOrder_ChildOf_IterEmpty<1U>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof depth_order iter empty chain 10K");
	PICOBENCH_REG(BM_Query_Cascade_ChildOf_Disabled)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof depth_order disabled barrier 10K");
	PICOBENCH_REG(BM_Query_Traversal_DependsOn<4U>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query dependson traversal 10K");
	PICOBENCH_REG(BM_Query_DepthOrder_DependsOn<4U>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query dependson depth_order 10K");
	PICOBENCH_REG(BM_Query_Traversal_DependsOn<1U>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query dependson traversal chain 10K");
	PICOBENCH_REG(BM_Query_DepthOrder_DependsOn<1U>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query dependson depth_order chain 10K");
	PICOBENCH_REG(BM_Query_DepthOrder_DependsOn_IterEmpty<4U>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query dependson depth_order iter empty 10K");
	PICOBENCH_REG(BM_Query_DepthOrder_DependsOn_IterEmpty<1U>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query dependson depth_order iter empty chain 10K");
	PICOBENCH_REG(BM_Query_Cascade_ChildOf_Component)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof depth_order get 10K");
	PICOBENCH_REG(BM_Query_Plain_ChildOf_EachComponent)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof plain each pos 10K");
	PICOBENCH_REG(BM_Query_Traversal_ChildOf_EachComponent)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof traversal each pos 10K");
	PICOBENCH_REG(BM_Query_Cascade_ChildOf_EachComponent)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof depth_order each pos 10K");
	PICOBENCH_REG(BM_Query_Plain_ChildOf_Iter)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof plain iter pos 10K");
	PICOBENCH_REG(BM_Query_Traversal_ChildOf_Iter)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof traversal iter pos 10K");
	PICOBENCH_REG(BM_Query_Cascade_ChildOf_Iter)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof depth_order iter pos 10K");
	PICOBENCH_REG(BM_Query_Traversal_ChildOf_Disabled)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query childof traversal disabled barrier 10K");
	PICOBENCH_REG(BM_Query_Traversal_Parent_Disabled)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("query parent traversal disabled barrier 10K");
	PICOBENCH_SUITE_REG("Query cache maintenance");
	PICOBENCH_REG(BM_Hierarchy_DeleteTarget<false>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("childof delete target 10K");
	PICOBENCH_REG(BM_Hierarchy_DeleteTarget<true>)
			.PICO_SETTINGS_FOCUS()
			.user_data(NEntitiesFew)
			.label("parent delete target 10K");
}

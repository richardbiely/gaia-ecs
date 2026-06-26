#include "test_common.h"

#define TestWorld SparseTestWorld
#include <iterator>

#if GAIA_OBSERVERS_ENABLED

TEST_CASE("Observer - direct source term changes trigger OnAdd and OnDel") {
	TestWorld twld;

	const auto source = wld.add();
	const auto matched = wld.add();
	const auto unmatched = wld.add();
	wld.add<Position>(matched, {1.0f, 2.0f, 3.0f});
	wld.add<Rotation>(unmatched, {4.0f, 5.0f, 6.0f, 7.0f});

	uint32_t addHits = 0;
	uint32_t delHits = 0;
	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.all<Acceleration>(ecs::QueryTermOptions{}.src(source))
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++addHits;
					added.push_back(entities[i]);
				}
			});

	wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.all<Position>()
			.all<Acceleration>(ecs::QueryTermOptions{}.src(source))
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++delHits;
					removed.push_back(entities[i]);
				}
			});

	wld.add<Acceleration>(source, {8.0f, 9.0f, 10.0f});
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	if (added.size() == 1)
		CHECK(added[0] == matched);

	wld.del<Acceleration>(source);
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	if (removed.size() == 1)
		CHECK(removed[0] == matched);
}

TEST_CASE("Observer - deleting a direct source emits OnDel") {
	TestWorld twld;

	const auto source = wld.add();
	wld.add<Acceleration>(source, {1.0f, 2.0f, 3.0f});

	const auto matched = wld.add();
	wld.add<Position>(matched, {4.0f, 5.0f, 6.0f});

	uint32_t hits = 0;
	cnt::darr<ecs::Entity> removed;
	wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.all<Position>()
			.all<Acceleration>(ecs::QueryTermOptions{}.src(source))
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++hits;
					removed.push_back(entities[i]);
				}
			});

	wld.del(source);
	CHECK(hits == 1);
	CHECK(removed.size() == 1);
	if (removed.size() == 1)
		CHECK(removed[0] == matched);
}

TEST_CASE("Observer - direct source ignores recycled source ids") {
	TestWorld twld;

	const auto source = wld.add();
	const auto matched = wld.add();
	wld.add<Position>(matched, {1.0f, 2.0f, 3.0f});

	uint32_t hits = 0;
	cnt::darr<ecs::Entity> added;
	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.all<Acceleration>(ecs::QueryTermOptions{}.src(source))
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++hits;
					added.push_back(entities[i]);
				}
			});

	wld.add<Acceleration>(source, {4.0f, 5.0f, 6.0f});
	CHECK(hits == 1);
	CHECK(added.size() == 1);
	if (added.size() == 1)
		CHECK(added[0] == matched);

	hits = 0;
	added.clear();

	wld.del(source);
	wld.update();

	ecs::Entity recycled = ecs::EntityBad;
	for (uint32_t i = 0; i < 256 && recycled == ecs::EntityBad; ++i) {
		const auto candidate = wld.add();
		if (candidate.id() == source.id())
			recycled = candidate;
	}
	CHECK(recycled != ecs::EntityBad);
	if (recycled != ecs::EntityBad) {
		CHECK(recycled.id() == source.id());
		CHECK(recycled.gen() != source.gen());
		wld.add<Acceleration>(recycled, {7.0f, 8.0f, 9.0f});
	}

	CHECK(hits == 0);
	CHECK(added.empty());
}

TEST_CASE("Observer - direct source or term changes trigger OnAdd and OnDel") {
	TestWorld twld;

	const auto source = wld.add();
	const auto matched = wld.add();
	wld.add<Position>(matched, {1.0f, 2.0f, 3.0f});

	uint32_t addHits = 0;
	uint32_t delHits = 0;
	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.or_<Acceleration>(ecs::QueryTermOptions{}.src(source))
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++addHits;
					added.push_back(entities[i]);
				}
			});

	wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.all<Position>()
			.or_<Acceleration>(ecs::QueryTermOptions{}.src(source))
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++delHits;
					removed.push_back(entities[i]);
				}
			});

	wld.add<Acceleration>(source, {4.0f, 5.0f, 6.0f});
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	if (added.size() == 1)
		CHECK(added[0] == matched);

	wld.del<Acceleration>(source);
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	if (removed.size() == 1)
		CHECK(removed[0] == matched);
}

TEST_CASE("Observer - identical traversed observers keep local cached query state by default") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));

	cnt::darray<ecs::Entity> observers;
	observers.reserve(200);

	const auto makeObserver = [&twld, connectedTo] {
		return wld.observer()
				.event(ecs::ObserverEvent::OnAdd)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([](ecs::Iter&) {})
				.entity();
	};

	const auto observer0 = makeObserver();
	const auto expectedHandle = ecs::QueryInfo::handle(wld.observers().data(observer0).query.fetch());
	observers.push_back(observer0);

	for (uint32_t i = 1; i < 200; ++i) {
		const auto observer = makeObserver();
		const auto handle = ecs::QueryInfo::handle(wld.observers().data(observer).query.fetch());
		CHECK(handle != expectedHandle);
		observers.push_back(observer);
	}

	CHECK(observers.size() == 200);
}

TEST_CASE("Observer - identical traversed observers delete cleanly") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);

	cnt::darr<ecs::Entity> observers;
	const auto makeObserver = [&twld, connectedTo](ecs::ObserverEvent event) {
		return wld.observer()
				.event(event)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([](ecs::Iter&) {})
				.entity();
	};

	observers.push_back(makeObserver(ecs::ObserverEvent::OnAdd));
	observers.push_back(makeObserver(ecs::ObserverEvent::OnDel));
	observers.push_back(makeObserver(ecs::ObserverEvent::OnAdd));
	observers.push_back(makeObserver(ecs::ObserverEvent::OnDel));

	const auto positionTerm = wld.add<Position>().entity;
	const auto accelerationTerm = wld.add<Acceleration>().entity;
	CHECK(wld.observers().has_observers(positionTerm));
	CHECK(wld.observers().has_observers(accelerationTerm));

	wld.del(observers[0]);
	wld.del(observers[1]);
	CHECK(wld.observers().has_observers(positionTerm));
	CHECK(wld.observers().has_observers(accelerationTerm));

	wld.del(observers[2]);
	wld.del(observers[3]);
	CHECK_FALSE(wld.observers().has_observers(positionTerm));
	CHECK_FALSE(wld.observers().has_observers(accelerationTerm));
}

TEST_CASE("Observer - duplicate traversed observer cleanup leaves no stale observed terms") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);

	const auto makeObserver = [&twld, connectedTo] {
		return wld.observer()
				.event(ecs::ObserverEvent::OnAdd)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([](ecs::Iter&) {})
				.entity();
	};

	const auto observer0 = makeObserver();
	const auto observer1 = makeObserver();
	const auto positionTerm = wld.add<Position>().entity;
	const auto accelerationTerm = wld.add<Acceleration>().entity;

	CHECK(wld.observers().has_observers(positionTerm));
	CHECK(wld.observers().has_observers(accelerationTerm));

	wld.del(observer0);
	CHECK(wld.observers().has_observers(positionTerm));
	CHECK(wld.observers().has_observers(accelerationTerm));

	wld.del(observer1);
	CHECK_FALSE(wld.observers().has_observers(positionTerm));
	CHECK_FALSE(wld.observers().has_observers(accelerationTerm));
}

namespace {
	template <typename TQuery>
	cnt::darr<ecs::Entity> collect_sorted_entities(TQuery query) {
		cnt::darr<ecs::Entity> out;
		query.each([&](ecs::Entity entity) {
			out.push_back(entity);
		});
		std::sort(out.begin(), out.end(), [](ecs::Entity left, ecs::Entity right) {
			if (left.id() != right.id())
				return left.id() < right.id();
			return left.gen() < right.gen();
		});
		return out;
	}

	cnt::darr<ecs::Entity> sorted_entity_diff(const cnt::darr<ecs::Entity>& lhs, const cnt::darr<ecs::Entity>& rhs) {
		cnt::darr<ecs::Entity> out;
		out.reserve(lhs.size());
		std::set_difference(
				lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::back_inserter(out),
				[](ecs::Entity left, ecs::Entity right) {
					if (left.id() != right.id())
						return left.id() < right.id();
					return left.gen() < right.gen();
				});
		return out;
	}

	std::string entity_list_string(const cnt::darr<ecs::Entity>& entities) {
		std::string out = "[";
		for (uint32_t i = 0; i < entities.size(); ++i) {
			if (i != 0)
				out += ", ";
			out += std::to_string(entities[i].id());
			out += ":";
			out += std::to_string(entities[i].gen());
		}
		out += "]";
		return out;
	}

	ecs::Entity recycle_same_id(ecs::World& world, ecs::Entity stale, uint32_t maxAttempts = 256) {
		for (uint32_t i = 0; i < maxAttempts; ++i) {
			const auto candidate = world.add();
			if (candidate.id() == stale.id())
				return candidate;
		}

		return ecs::EntityBad;
	}

	template <typename MutateFunc>
	void expect_traversed_observer_changes(
			ecs::World& world, ecs::Entity bindingRelation, ecs::QueryTermOptions traversalOptions, MutateFunc&& mutate,
			ecs::ObserverPlan::ExecKind expectedExecKind = ecs::ObserverPlan::ExecKind::DiffPropagated) {
		auto termOptions = traversalOptions;
		termOptions.src(ecs::Var0);

		auto buildQuery = [&world, bindingRelation, termOptions]() {
			return world.uquery()
					.template all<Position>()
					.all(ecs::Pair(bindingRelation, ecs::Var0))
					.template all<Acceleration>(termOptions);
		};

		auto added = std::make_shared<cnt::darr<ecs::Entity>>();
		auto removed = std::make_shared<cnt::darr<ecs::Entity>>();

		const auto addObserver = world.observer()
																 .event(ecs::ObserverEvent::OnAdd)
																 .template all<Position>()
																 .all(ecs::Pair(bindingRelation, ecs::Var0))
																 .template all<Acceleration>(termOptions)
																 .on_each([added](ecs::Iter& it) {
																	 auto entities = it.view<ecs::Entity>();
																	 GAIA_EACH(it) {
																		 added->push_back(entities[i]);
																	 }
																 })
																 .entity();
		const auto delObserver = world.observer()
																 .event(ecs::ObserverEvent::OnDel)
																 .template all<Position>()
																 .all(ecs::Pair(bindingRelation, ecs::Var0))
																 .template all<Acceleration>(termOptions)
																 .on_each([removed](ecs::Iter& it) {
																	 auto entities = it.view<ecs::Entity>();
																	 GAIA_EACH(it) {
																		 removed->push_back(entities[i]);
																	 }
																 })
																 .entity();

		const auto& addData = world.observers().data(addObserver);
		const auto& delData = world.observers().data(delObserver);
		CHECK(addData.plan.exec_kind() == expectedExecKind);
		CHECK(delData.plan.exec_kind() == expectedExecKind);

		const auto before = collect_sorted_entities(buildQuery());
		mutate();
		const auto after = collect_sorted_entities(buildQuery());

		std::sort(added->begin(), added->end(), [](ecs::Entity left, ecs::Entity right) {
			if (left.id() != right.id())
				return left.id() < right.id();
			return left.gen() < right.gen();
		});
		std::sort(removed->begin(), removed->end(), [](ecs::Entity left, ecs::Entity right) {
			if (left.id() != right.id())
				return left.id() < right.id();
			return left.gen() < right.gen();
		});

		const auto expectedAdded = sorted_entity_diff(after, before);
		const auto expectedRemoved = sorted_entity_diff(before, after);

		INFO("before=" << entity_list_string(before));
		INFO("after=" << entity_list_string(after));
		INFO("added=" << entity_list_string(*added));
		INFO("expectedAdded=" << entity_list_string(expectedAdded));
		INFO("removed=" << entity_list_string(*removed));
		INFO("expectedRemoved=" << entity_list_string(expectedRemoved));

		CHECK(*added == expectedAdded);
		CHECK(*removed == expectedRemoved);
		(void)addObserver;
		(void)delObserver;
	}
} // namespace

TEST_CASE("Observer - traversed source propagation on ancestor term changes") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));

	int addHits = 0;
	int delHits = 0;
	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;

	const auto makeObserver = [&twld, connectedTo](ecs::ObserverEvent event, int& hits, cnt::darr<ecs::Entity>& out) {
		return wld.observer()
				.event(event)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter& it) {
					auto entities = it.view<ecs::Entity>();
					GAIA_EACH(it) {
						++hits;
						out.push_back(entities[i]);
					}
				})
				.entity();
	};

	(void)makeObserver(ecs::ObserverEvent::OnAdd, addHits, added);
	(void)makeObserver(ecs::ObserverEvent::OnDel, delHits, removed);

	wld.add<Acceleration>(root);
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	CHECK(added[0] == cable);

	wld.del<Acceleration>(root);
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	CHECK(removed[0] == cable);
}

TEST_CASE("Observer - traversed source propagation on ancestor term changes and unrelated existing matches") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto rootHot = wld.add();
	const auto childHot = wld.add();
	wld.child(childHot, rootHot);

	const auto rootCold = wld.add();
	const auto childCold = wld.add();
	wld.child(childCold, rootCold);
	wld.add<Acceleration>(rootCold);

	const auto cableCold = wld.add();
	wld.add<Position>(cableCold);
	wld.add(cableCold, ecs::Pair(connectedTo, childCold));

	const auto cableHot = wld.add();
	wld.add<Position>(cableHot);
	wld.add(cableHot, ecs::Pair(connectedTo, childHot));

	int addHits = 0;
	cnt::darr<ecs::Entity> added;

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++addHits;
					added.push_back(entities[i]);
				}
			});

	wld.add<Acceleration>(rootHot);
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	CHECK(added[0] == cableHot);
}

TEST_CASE("Observer - traversed source propagation on unrelated ancestor term changes emits nothing") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto rootMatching = wld.add();
	const auto childMatching = wld.add();
	wld.child(childMatching, rootMatching);
	wld.add<Acceleration>(rootMatching);

	const auto cableMatching = wld.add();
	wld.add<Position>(cableMatching);
	wld.add(cableMatching, ecs::Pair(connectedTo, childMatching));

	const auto rootUnrelated = wld.add();
	const auto childUnrelated = wld.add();
	wld.child(childUnrelated, rootUnrelated);

	int addHits = 0;
	cnt::darr<ecs::Entity> added;

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++addHits;
					added.push_back(entities[i]);
				}
			});

	wld.add<Acceleration>(rootUnrelated);
	CHECK(addHits == 0);
	CHECK(added.empty());
}

TEST_CASE("Observer - traversed source propagation on source binding pair changes") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);

	int addHits = 0;
	int delHits = 0;
	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;

	const auto makeObserver = [&twld, connectedTo](ecs::ObserverEvent event, int& hits, cnt::darr<ecs::Entity>& out) {
		return wld.observer()
				.event(event)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter& it) {
					auto entities = it.view<ecs::Entity>();
					GAIA_EACH(it) {
						++hits;
						out.push_back(entities[i]);
					}
				})
				.entity();
	};

	(void)makeObserver(ecs::ObserverEvent::OnAdd, addHits, added);
	(void)makeObserver(ecs::ObserverEvent::OnDel, delHits, removed);

	wld.add(cable, ecs::Pair(connectedTo, child));
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	CHECK(added[0] == cable);

	wld.del(cable, ecs::Pair(connectedTo, child));
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	CHECK(removed[0] == cable);
}

TEST_CASE("Observer - traversed source local pair change only diffs touched entity") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cableExisting = wld.add();
	wld.add<Position>(cableExisting);
	wld.add(cableExisting, ecs::Pair(connectedTo, child));

	const auto cableAdded = wld.add();
	wld.add<Position>(cableAdded);

	int addHits = 0;
	cnt::darr<ecs::Entity> added;

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++addHits;
					added.push_back(entities[i]);
				}
			});

	wld.add(cableAdded, ecs::Pair(connectedTo, child));
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	CHECK(added[0] == cableAdded);

	int delHits = 0;
	cnt::darr<ecs::Entity> removed;
	wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++delHits;
					removed.push_back(entities[i]);
				}
			});

	wld.del(cableAdded, ecs::Pair(connectedTo, child));
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	CHECK(removed[0] == cableAdded);
}

TEST_CASE("Observer - traversed source propagation on relation edge changes") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));
	auto q = wld.query()
							 .template all<Position>()
							 .all(ecs::Pair(connectedTo, ecs::Var0))
							 .template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav());
	CHECK(q.count() == 0);

	int addHits = 0;
	int delHits = 0;
	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;

	const auto makeObserver = [&twld, connectedTo](ecs::ObserverEvent event, int& hits, cnt::darr<ecs::Entity>& out) {
		return wld.observer()
				.event(event)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter& it) {
					auto entities = it.view<ecs::Entity>();
					GAIA_EACH(it) {
						++hits;
						out.push_back(entities[i]);
					}
				})
				.entity();
	};
	const auto buildQuery = [&twld, connectedTo] {
		return wld.uquery()
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav());
	};

	(void)makeObserver(ecs::ObserverEvent::OnAdd, addHits, added);
	(void)makeObserver(ecs::ObserverEvent::OnDel, delHits, removed);

	wld.child(child, root);
	CHECK(wld.target(child, ecs::ChildOf) == root);
	CHECK(buildQuery().count() == 1);
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	if (!added.empty())
		CHECK(added[0] == cable);

	wld.del(child, ecs::Pair(ecs::ChildOf, root));
	CHECK(wld.target(child, ecs::ChildOf) == ecs::EntityBad);
	CHECK(buildQuery().count() == 0);
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	if (!removed.empty())
		CHECK(removed[0] == cable);
}

TEST_CASE("Observer - traversed source propagation and unrelated pair relation changes") {
	TestWorld twld;

	const auto connectedToA = wld.add();
	const auto connectedToB = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);

	int hitsA = 0;
	int hitsB = 0;

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedToA, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter&) {
				++hitsA;
			});

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedToB, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter&) {
				++hitsB;
			});

	wld.add(cable, ecs::Pair(connectedToA, child));
	CHECK(hitsA == 1);
	CHECK(hitsB == 0);
}

TEST_CASE("Observer - ancestor component diffs with many descendants") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto childA = wld.add();
	const auto childB = wld.add();
	const auto grandChild = wld.add();
	wld.child(childA, root);
	wld.child(childB, root);
	wld.child(grandChild, childA);

	const auto cableA = wld.add();
	const auto cableB = wld.add();
	const auto cableC = wld.add();
	wld.add<Position>(cableA);
	wld.add<Position>(cableB);
	wld.add<Position>(cableC);
	wld.add(cableA, ecs::Pair(connectedTo, childA));
	wld.add(cableB, ecs::Pair(connectedTo, childB));
	wld.add(cableC, ecs::Pair(connectedTo, grandChild));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.add<Acceleration>(root);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del<Acceleration>(root);
	});
}

TEST_CASE("Observer - self-up closer-match transitions") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.add<Acceleration>(child);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del<Acceleration>(root);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del<Acceleration>(child);
	});
}

TEST_CASE("Observer - bounded parent traversal") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto parent = wld.add();
	const auto leaf = wld.add();
	wld.child(parent, root);
	wld.child(leaf, parent);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, leaf));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav_parent(), [&] {
		wld.add<Acceleration>(root);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav_parent(), [&] {
		wld.add<Acceleration>(parent);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav_parent(), [&] {
		wld.del<Acceleration>(parent);
	});
}

TEST_CASE("Observer - bounded self-parent transitions") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto parent = wld.add();
	const auto leaf = wld.add();
	wld.child(parent, root);
	wld.child(leaf, parent);
	wld.add<Acceleration>(leaf);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, leaf));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav_self_parent(), [&] {
		wld.add<Acceleration>(parent);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav_self_parent(), [&] {
		wld.del<Acceleration>(leaf);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav_self_parent(), [&] {
		wld.del<Acceleration>(parent);
	});
}

TEST_CASE("Observer - repeated reachability paths") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto linkedTo = wld.add();
	const auto root = wld.add();
	const auto midA = wld.add();
	const auto midB = wld.add();
	const auto leaf = wld.add();

	wld.add(midA, ecs::Pair(linkedTo, root));
	wld.add(midB, ecs::Pair(linkedTo, root));
	wld.add(leaf, ecs::Pair(linkedTo, midA));
	wld.add(leaf, ecs::Pair(linkedTo, midB));

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, leaf));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(linkedTo), [&] {
		wld.add<Acceleration>(root);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(linkedTo), [&] {
		wld.del<Acceleration>(root);
	});
}

TEST_CASE("Observer - duplicate-path relation edge additions") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto linkedTo = wld.add();
	const auto root = wld.add();
	const auto midA = wld.add();
	const auto midB = wld.add();
	const auto leaf = wld.add();

	wld.add<Acceleration>(root);
	wld.add(midA, ecs::Pair(linkedTo, root));
	wld.add(midB, ecs::Pair(linkedTo, root));
	wld.add(leaf, ecs::Pair(linkedTo, midA));

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, leaf));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(linkedTo), [&] {
		wld.add(leaf, ecs::Pair(linkedTo, midB));
	});
}

TEST_CASE("Observer - duplicate-path relation edge removals with remaining path") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto linkedTo = wld.add();
	const auto root = wld.add();
	const auto midA = wld.add();
	const auto midB = wld.add();
	const auto leaf = wld.add();

	wld.add<Acceleration>(root);
	wld.add(midA, ecs::Pair(linkedTo, root));
	wld.add(midB, ecs::Pair(linkedTo, root));
	wld.add(leaf, ecs::Pair(linkedTo, midA));
	wld.add(leaf, ecs::Pair(linkedTo, midB));

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, leaf));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(linkedTo), [&] {
		wld.del(leaf, ecs::Pair(linkedTo, midB));
	});
}

TEST_CASE("Observer - deleting one repeated-path intermediate keeps the match") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto linkedTo = wld.add();
	const auto root = wld.add();
	const auto midA = wld.add();
	const auto midB = wld.add();
	const auto leaf = wld.add();

	wld.add<Acceleration>(root);
	wld.add(midA, ecs::Pair(linkedTo, root));
	wld.add(midB, ecs::Pair(linkedTo, root));
	wld.add(leaf, ecs::Pair(linkedTo, midA));
	wld.add(leaf, ecs::Pair(linkedTo, midB));

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, leaf));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(linkedTo), [&] {
		wld.del(midB);
	});
}

TEST_CASE("Observer - deleting the last repeated-path intermediate removes the match") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto linkedTo = wld.add();
	const auto root = wld.add();
	const auto midA = wld.add();
	const auto midB = wld.add();
	const auto leaf = wld.add();

	wld.add<Acceleration>(root);
	wld.add(midA, ecs::Pair(linkedTo, root));
	wld.add(midB, ecs::Pair(linkedTo, root));
	wld.add(leaf, ecs::Pair(linkedTo, midA));
	wld.add(leaf, ecs::Pair(linkedTo, midB));

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, leaf));

	wld.del(midB);
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(linkedTo), [&] {
		wld.del(midA);
	});
}

TEST_CASE("Observer - removing one of multiple matching binding pairs") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto childA = wld.add();
	const auto childB = wld.add();
	wld.child(childA, root);
	wld.child(childB, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, childA));
	wld.add(cable, ecs::Pair(connectedTo, childB));

	expect_traversed_observer_changes(
			wld, connectedTo, ecs::QueryTermOptions{}.trav(),
			[&] {
				wld.del(cable, ecs::Pair(connectedTo, childB));
			},
			ecs::ObserverPlan::ExecKind::DiffPropagated);

	expect_traversed_observer_changes(
			wld, connectedTo, ecs::QueryTermOptions{}.trav(),
			[&] {
				wld.del(cable, ecs::Pair(connectedTo, childA));
			},
			ecs::ObserverPlan::ExecKind::DiffPropagated);
}

TEST_CASE("Observer - another bound source still matches") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto childA = wld.add();
	const auto childB = wld.add();
	wld.child(childA, rootA);
	wld.child(childB, rootB);
	wld.add<Acceleration>(rootA);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, childA));
	wld.add(cable, ecs::Pair(connectedTo, childB));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.add<Acceleration>(rootB);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del<Acceleration>(rootA);
	});
}

TEST_CASE("Observer - deleting a matching ancestor updates all descendants") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto childA = wld.add();
	const auto childB = wld.add();
	const auto grandChild = wld.add();
	wld.child(childA, root);
	wld.child(childB, root);
	wld.child(grandChild, childA);
	wld.add<Acceleration>(root);

	const auto cableA = wld.add();
	const auto cableB = wld.add();
	const auto cableC = wld.add();
	wld.add<Position>(cableA);
	wld.add<Position>(cableB);
	wld.add<Position>(cableC);
	wld.add(cableA, ecs::Pair(connectedTo, childA));
	wld.add(cableB, ecs::Pair(connectedTo, childB));
	wld.add(cableC, ecs::Pair(connectedTo, grandChild));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del(root);
	});
}

TEST_CASE("Observer - deleting one bound source with another still matching") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto childA = wld.add();
	const auto childB = wld.add();
	wld.child(childA, rootA);
	wld.child(childB, rootB);
	wld.add<Acceleration>(rootA);
	wld.add<Acceleration>(rootB);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, childA));
	wld.add(cable, ecs::Pair(connectedTo, childB));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del(childB);
	});
}

TEST_CASE("Observer - deleting the only bound source emits removal") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del(child);
	});
}

TEST_CASE("Observer - recycled bound source does not resurrect traversed matches") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del(child);
		wld.update();

		const auto recycled = recycle_same_id(wld, child);
		CHECK(recycled != ecs::EntityBad);
		if (recycled != ecs::EntityBad) {
			CHECK(recycled.id() == child.id());
			CHECK(recycled.gen() != child.gen());
			wld.child(recycled, root);
		}
	});
}

TEST_CASE("Observer - recycled ancestor does not resurrect traversed matches") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del(root);
		wld.update();

		const auto recycled = recycle_same_id(wld, root);
		CHECK(recycled != ecs::EntityBad);
		if (recycled != ecs::EntityBad) {
			CHECK(recycled.id() == root.id());
			CHECK(recycled.gen() != root.gen());
			wld.add<Acceleration>(recycled);
		}
	});
}

TEST_CASE("Observer - unsupported traversal diff shape") {
	TestWorld twld;

	struct Shield {};

	const auto connectedTo = wld.add();
	const auto guardedBy = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	const auto guardian = wld.add();
	wld.child(child, root);
	wld.child(guardian, root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));
	wld.add(cable, ecs::Pair(guardedBy, guardian));

	auto buildQuery = [&twld, connectedTo, guardedBy]() {
		return wld.uquery()
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.all(ecs::Pair(guardedBy, ecs::Var1))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.template all<Shield>(ecs::QueryTermOptions{}.src(ecs::Var1).trav_parent());
	};

	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;
	const auto addObserver = wld.observer()
															 .event(ecs::ObserverEvent::OnAdd)
															 .template all<Position>()
															 .all(ecs::Pair(connectedTo, ecs::Var0))
															 .all(ecs::Pair(guardedBy, ecs::Var1))
															 .template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
															 .template all<Shield>(ecs::QueryTermOptions{}.src(ecs::Var1).trav_parent())
															 .on_each([&](ecs::Iter& it) {
																 auto entities = it.view<ecs::Entity>();
																 GAIA_EACH(it) {
																	 added.push_back(entities[i]);
																 }
															 })
															 .entity();
	const auto delObserver = wld.observer()
															 .event(ecs::ObserverEvent::OnDel)
															 .template all<Position>()
															 .all(ecs::Pair(connectedTo, ecs::Var0))
															 .all(ecs::Pair(guardedBy, ecs::Var1))
															 .template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
															 .template all<Shield>(ecs::QueryTermOptions{}.src(ecs::Var1).trav_parent())
															 .on_each([&](ecs::Iter& it) {
																 auto entities = it.view<ecs::Entity>();
																 GAIA_EACH(it) {
																	 removed.push_back(entities[i]);
																 }
															 })
															 .entity();

	CHECK(wld.observers().data(addObserver).plan.uses_fallback_diff_dispatch());
	CHECK(wld.observers().data(delObserver).plan.uses_fallback_diff_dispatch());

	const auto before = collect_sorted_entities(buildQuery());
	wld.add<Acceleration>(root);
	const auto middle = collect_sorted_entities(buildQuery());
	wld.add<Shield>(root);
	const auto after = collect_sorted_entities(buildQuery());

	std::sort(added.begin(), added.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});
	std::sort(removed.begin(), removed.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	const auto expectedAddedFirst = sorted_entity_diff(middle, before);
	const auto expectedAddedSecond = sorted_entity_diff(after, middle);
	cnt::darr<ecs::Entity> expectedAdded = expectedAddedFirst;
	for (auto entity: expectedAddedSecond)
		expectedAdded.push_back(entity);
	std::sort(expectedAdded.begin(), expectedAdded.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	CHECK(added == expectedAdded);
	CHECK(removed.empty());
}

TEST_CASE("Observer - traversed source copy_ext_n fires for all new entities") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto src = wld.add();
	wld.add<Position>(src);
	wld.add(src, ecs::Pair(connectedTo, child));

	auto buildQuery = [&twld, connectedTo]() {
		return wld.uquery()
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav());
	};

	cnt::darr<ecs::Entity> added;
	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					added.push_back(entities[i]);
				}
			});

	const auto before = collect_sorted_entities(buildQuery());

	cnt::darr<ecs::Entity> copied;
	wld.copy_ext_n(src, 257, [&](ecs::Entity entity) {
		copied.push_back(entity);
	});

	const auto after = collect_sorted_entities(buildQuery());
	std::sort(added.begin(), added.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});
	std::sort(copied.begin(), copied.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	const auto expectedAdded = sorted_entity_diff(after, before);
	CHECK(added == expectedAdded);
	CHECK(added == copied);
}

TEST_CASE("Observer - traversed source copy_ext_n with multiple matching binding pairs") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto childA = wld.add();
	const auto childB = wld.add();
	wld.child(childA, rootA);
	wld.child(childB, rootB);
	wld.add<Acceleration>(rootA);
	wld.add<Acceleration>(rootB);

	const auto src = wld.add();
	wld.add<Position>(src);
	wld.add(src, ecs::Pair(connectedTo, childA));
	wld.add(src, ecs::Pair(connectedTo, childB));

	cnt::darr<ecs::Entity> added;
	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					added.push_back(entities[i]);
				}
			});

	cnt::darr<ecs::Entity> copied;
	wld.copy_ext_n(src, 193, [&](ecs::Entity entity) {
		copied.push_back(entity);
	});

	std::sort(added.begin(), added.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});
	std::sort(copied.begin(), copied.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	CHECK(added == copied);
}

TEST_CASE("Observer - traversed source add_n fires for all new entities") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto archetypeSeed = wld.add();
	wld.add<Position>(archetypeSeed);
	wld.add(archetypeSeed, ecs::Pair(connectedTo, child));

	auto buildQuery = [&twld, connectedTo]() {
		return wld.uquery()
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav());
	};

	cnt::darr<ecs::Entity> added;
	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					added.push_back(entities[i]);
				}
			});

	const auto before = collect_sorted_entities(buildQuery());

	cnt::darr<ecs::Entity> created;
	wld.add_n(archetypeSeed, 257, [&](ecs::Entity entity) {
		created.push_back(entity);
	});

	const auto after = collect_sorted_entities(buildQuery());
	std::sort(added.begin(), added.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});
	std::sort(created.begin(), created.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	const auto expectedAdded = sorted_entity_diff(after, before);
	CHECK(added == expectedAdded);
	CHECK(added == created);
}

TEST_CASE("Observer - traversed source add_n with multiple matching binding pairs") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto childA = wld.add();
	const auto childB = wld.add();
	wld.child(childA, rootA);
	wld.child(childB, rootB);
	wld.add<Acceleration>(rootA);
	wld.add<Acceleration>(rootB);

	const auto archetypeSeed = wld.add();
	wld.add<Position>(archetypeSeed);
	wld.add(archetypeSeed, ecs::Pair(connectedTo, childA));
	wld.add(archetypeSeed, ecs::Pair(connectedTo, childB));

	cnt::darr<ecs::Entity> added;
	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					added.push_back(entities[i]);
				}
			});

	cnt::darr<ecs::Entity> created;
	wld.add_n(archetypeSeed, 193, [&](ecs::Entity entity) {
		created.push_back(entity);
	});

	std::sort(added.begin(), added.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});
	std::sort(created.begin(), created.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	CHECK(added == created);
}

TEST_CASE("Observer - parented instantiate_n traversed source fires for all new roots") {
	TestWorld twld;

	const auto scene = wld.add();
	wld.add<Acceleration>(scene);

	const auto animal = wld.add();
	wld.add<Position>(animal);

	auto buildQuery = [&twld]() {
		return wld.uquery()
				.template all<Position>()
				.all(ecs::Pair(ecs::Parent, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_self_parent());
	};

	cnt::darr<ecs::Entity> added;
	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(ecs::Parent, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_self_parent())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					added.push_back(entities[i]);
				}
			});

	const auto before = collect_sorted_entities(buildQuery());

	cnt::darr<ecs::Entity> created;
	wld.instantiate_n(animal, scene, 129, [&](ecs::Entity entity) {
		created.push_back(entity);
	});

	const auto after = collect_sorted_entities(buildQuery());
	std::sort(added.begin(), added.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});
	std::sort(created.begin(), created.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	const auto expectedAdded = sorted_entity_diff(after, before);
	CHECK(added == expectedAdded);
	CHECK(created.size() == 129);
}

TEST_CASE("Observer - many identical traversed observers each fire once for batched copy_ext_n") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto src = wld.add();
	wld.add<Position>(src);
	wld.add(src, ecs::Pair(connectedTo, child));

	struct ObsCapture {
		cnt::darr<ecs::Entity> hits;
	};

	cnt::darr<ObsCapture> captures(24);
	for (auto& capture: captures) {
		wld.observer()
				.event(ecs::ObserverEvent::OnAdd)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&capture](ecs::Iter& it) {
					auto entities = it.view<ecs::Entity>();
					GAIA_EACH(it) {
						capture.hits.push_back(entities[i]);
					}
				});
	}

	cnt::darr<ecs::Entity> copied;
	wld.copy_ext_n(src, 97, [&](ecs::Entity entity) {
		copied.push_back(entity);
	});
	std::sort(copied.begin(), copied.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	for (auto& capture: captures) {
		std::sort(capture.hits.begin(), capture.hits.end(), [](ecs::Entity left, ecs::Entity right) {
			if (left.id() != right.id())
				return left.id() < right.id();
			return left.gen() < right.gen();
		});
		CHECK(capture.hits == copied);
	}
}

#endif

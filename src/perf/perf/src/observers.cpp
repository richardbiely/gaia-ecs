#include "common.h"
#include "registry.h"

template <uint32_t ObserverCount, uint32_t TermCount>
void BM_Observer_OnAdd(picobench::state& state) {
	static_assert(TermCount >= 1 && TermCount <= 8);

	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		entities.clear();
		entities.reserve(n);
		uint64_t hits = 0;

		GAIA_FOR(ObserverCount) {
			auto observer = w.observer().event(ecs::ObserverEvent::OnAdd);
			observer_query_all<TermCount>(observer);
			observer.on_each([&hits](ecs::Iter& it) {
				(void)it;
				++hits;
			});
		}

		GAIA_FOR(n) {
			auto e = w.add();
			entities.push_back(e);
			add_observer_terms_except_last<TermCount>(w, e);
		}

		hits = 0;
		state.start_timer();

		for (auto e: entities)
			add_observer_last_term<TermCount>(w, e);

		state.stop_timer();
		dont_optimize(hits);
	}
}

template <uint32_t ObserverCount, uint32_t TermCount>
void BM_Observer_OnDel(picobench::state& state) {
	static_assert(TermCount >= 1 && TermCount <= 8);

	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		entities.clear();
		entities.reserve(n);
		uint64_t hits = 0;

		GAIA_FOR(ObserverCount) {
			auto observer = w.observer().event(ecs::ObserverEvent::OnDel);
			observer_query_no<TermCount>(observer);
			observer.on_each([&hits](ecs::Iter& it) {
				(void)it;
				++hits;
			});
		}

		GAIA_FOR(n) {
			auto e = w.add();
			entities.push_back(e);
			add_observer_all_terms<TermCount>(w, e);
		}

		hits = 0;
		state.start_timer();

		for (auto e: entities) {
			remove_observer_terms_before_last<TermCount>(w, e);
			remove_observer_last_term<TermCount>(w, e);
		}

		state.stop_timer();
		dont_optimize(hits);
	}
}

void BM_Observer_OnAdd_0_1(picobench::state& state) {
	BM_Observer_OnAdd<0, 1>(state);
}
void BM_Observer_OnAdd_1_1(picobench::state& state) {
	BM_Observer_OnAdd<1, 1>(state);
}
void BM_Observer_OnAdd_10_1(picobench::state& state) {
	BM_Observer_OnAdd<10, 1>(state);
}
void BM_Observer_OnAdd_50_1(picobench::state& state) {
	BM_Observer_OnAdd<50, 1>(state);
}
void BM_Observer_OnAdd_50_2(picobench::state& state) {
	BM_Observer_OnAdd<50, 2>(state);
}
void BM_Observer_OnAdd_50_4(picobench::state& state) {
	BM_Observer_OnAdd<50, 4>(state);
}
void BM_Observer_OnAdd_50_8(picobench::state& state) {
	BM_Observer_OnAdd<50, 8>(state);
}

void BM_Observer_OnDel_0_1(picobench::state& state) {
	BM_Observer_OnDel<0, 1>(state);
}
void BM_Observer_OnDel_1_1(picobench::state& state) {
	BM_Observer_OnDel<1, 1>(state);
}
void BM_Observer_OnDel_10_1(picobench::state& state) {
	BM_Observer_OnDel<10, 1>(state);
}
void BM_Observer_OnDel_50_1(picobench::state& state) {
	BM_Observer_OnDel<50, 1>(state);
}
void BM_Observer_OnDel_50_2(picobench::state& state) {
	BM_Observer_OnDel<50, 2>(state);
}
void BM_Observer_OnDel_50_4(picobench::state& state) {
	BM_Observer_OnDel<50, 4>(state);
}
void BM_Observer_OnDel_50_8(picobench::state& state) {
	BM_Observer_OnDel<50, 8>(state);
}

void BM_Observer_DiffPairRelFiltered_OnAdd(picobench::state& state) {
	const uint32_t observerCount = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		uint64_t hits = 0;

		const auto relationHot = w.add();
		const auto root = w.add();
		const auto child = w.add();
		const auto cable = w.add();

		w.child(child, root);
		w.add<Acceleration>(root);
		w.add<Position>(cable);

		auto makeObserver = [&](ecs::Entity relation) {
			w.observer()
					.event(ecs::ObserverEvent::OnAdd)
					.all<Position>()
					.all(ecs::Pair(relation, ecs::Var0))
					.all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
					.on_each([&](ecs::Iter&) {
						++hits;
					});
		};

		if (observerCount != 0U)
			makeObserver(relationHot);
		GAIA_FOR2_(1, observerCount, i) {
			(void)i;
			makeObserver(w.add());
		}

		hits = 0;
		state.start_timer();
		w.add(cable, ecs::Pair(relationHot, child));
		state.stop_timer();

		dont_optimize(hits);
	}
}

void BM_Observer_DiffPairRelExistingMatches_OnAdd(picobench::state& state) {
	const uint32_t existingMatchCount = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		uint64_t hits = 0;

		const auto relationHot = w.add();
		const auto root = w.add();
		const auto child = w.add();
		w.child(child, root);
		w.add<Acceleration>(root);

		GAIA_FOR(existingMatchCount) {
			const auto cableExisting = w.add();
			w.add<Position>(cableExisting);
			w.add(cableExisting, ecs::Pair(relationHot, child));
		}

		const auto cableAdded = w.add();
		w.add<Position>(cableAdded);

		w.observer()
				.event(ecs::ObserverEvent::OnAdd)
				.all<Position>()
				.all(ecs::Pair(relationHot, ecs::Var0))
				.all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter&) {
					++hits;
				});

		hits = 0;
		state.start_timer();
		w.add(cableAdded, ecs::Pair(relationHot, child));
		state.stop_timer();

		dont_optimize(hits);
	}
}

void BM_Observer_DiffCopyExtFiltered_OnAdd(picobench::state& state) {
	const uint32_t observerCount = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		uint64_t hits = 0;

		const auto relationHot = w.add();
		const auto root = w.add();
		const auto child = w.add();
		const auto src = w.add();

		w.child(child, root);
		w.add<Acceleration>(root);
		w.add<Position>(src);
		w.add(src, ecs::Pair(relationHot, child));

		auto makeObserver = [&](ecs::Entity relation) {
			w.observer()
					.event(ecs::ObserverEvent::OnAdd)
					.all<Position>()
					.all(ecs::Pair(relation, ecs::Var0))
					.all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
					.on_each([&](ecs::Iter&) {
						++hits;
					});
		};

		if (observerCount != 0U)
			makeObserver(relationHot);
		GAIA_FOR2_(1, observerCount, i) {
			(void)i;
			makeObserver(w.add());
		}

		hits = 0;
		state.start_timer();
		const auto dst = w.copy_ext(src);
		state.stop_timer();

		dont_optimize(dst);
		dont_optimize(hits);
	}
}

void BM_Observer_DiffCopyExtExistingMatches_OnAdd(picobench::state& state) {
	const uint32_t existingMatchCount = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		uint64_t hits = 0;

		const auto relationHot = w.add();
		const auto root = w.add();
		const auto child = w.add();
		w.child(child, root);
		w.add<Acceleration>(root);

		GAIA_FOR(existingMatchCount) {
			const auto cableExisting = w.add();
			w.add<Position>(cableExisting);
			w.add(cableExisting, ecs::Pair(relationHot, child));
		}

		const auto src = w.add();
		w.add<Position>(src);
		w.add(src, ecs::Pair(relationHot, child));

		w.observer()
				.event(ecs::ObserverEvent::OnAdd)
				.all<Position>()
				.all(ecs::Pair(relationHot, ecs::Var0))
				.all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter&) {
					++hits;
				});

		hits = 0;
		state.start_timer();
		const auto dst = w.copy_ext(src);
		state.stop_timer();

		dont_optimize(dst);
		dont_optimize(hits);
	}
}

void BM_Observer_DiffAncestorExistingMatches_OnAdd(picobench::state& state) {
	const uint32_t existingMatchCount = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		uint64_t hits = 0;

		const auto relationHot = w.add();
		const auto rootHot = w.add();
		const auto childHot = w.add();
		w.child(childHot, rootHot);

		const auto rootCold = w.add();
		const auto childCold = w.add();
		w.child(childCold, rootCold);
		w.add<Acceleration>(rootCold);

		GAIA_FOR(existingMatchCount) {
			const auto cableExisting = w.add();
			w.add<Position>(cableExisting);
			w.add(cableExisting, ecs::Pair(relationHot, childCold));
		}

		const auto cableHot = w.add();
		w.add<Position>(cableHot);
		w.add(cableHot, ecs::Pair(relationHot, childHot));

		w.observer()
				.event(ecs::ObserverEvent::OnAdd)
				.all<Position>()
				.all(ecs::Pair(relationHot, ecs::Var0))
				.all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter&) {
					++hits;
				});

		hits = 0;
		state.start_timer();
		w.add<Acceleration>(rootHot);
		state.stop_timer();

		dont_optimize(hits);
	}
}

void BM_Observer_DiffRelationEdgeExistingMatches_OnAdd(picobench::state& state) {
	const uint32_t existingMatchCount = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		uint64_t hits = 0;

		const auto relationHot = w.add();
		const auto rootHot = w.add();
		w.add<Acceleration>(rootHot);
		const auto childHot = w.add();

		const auto rootCold = w.add();
		const auto childCold = w.add();
		w.child(childCold, rootCold);
		w.add<Acceleration>(rootCold);

		GAIA_FOR(existingMatchCount) {
			const auto cableExisting = w.add();
			w.add<Position>(cableExisting);
			w.add(cableExisting, ecs::Pair(relationHot, childCold));
		}

		const auto cableHot = w.add();
		w.add<Position>(cableHot);
		w.add(cableHot, ecs::Pair(relationHot, childHot));

		w.observer()
				.event(ecs::ObserverEvent::OnAdd)
				.all<Position>()
				.all(ecs::Pair(relationHot, ecs::Var0))
				.all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter&) {
					++hits;
				});

		hits = 0;
		state.start_timer();
		w.child(childHot, rootHot);
		state.stop_timer();

		dont_optimize(hits);
	}
}

void BM_Observer_DiffAncestorFilteredObservers_OnAdd(picobench::state& state) {
	const uint32_t observerCount = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		uint64_t hits = 0;

		const auto relationHot = w.add();
		const auto root = w.add();
		const auto child = w.add();
		w.child(child, root);

		const auto cable = w.add();
		w.add<Position>(cable);
		w.add(cable, ecs::Pair(relationHot, child));

		GAIA_FOR(observerCount) {
			w.observer()
					.event(ecs::ObserverEvent::OnAdd)
					.all<Position>()
					.all(ecs::Pair(relationHot, ecs::Var0))
					.all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
					.on_each([&](ecs::Iter&) {
						++hits;
					});
		}

		hits = 0;
		state.start_timer();
		w.add<Acceleration>(root);
		state.stop_timer();

		dont_optimize(hits);
	}
}

void BM_Observer_DiffAncestorCascadeDelete_OnDel(picobench::state& state) {
	const uint32_t existingMatchCount = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		uint64_t hits = 0;

		const auto relationHot = w.add();

		const auto rootCold = w.add();
		const auto childCold = w.add();
		w.child(childCold, rootCold);
		w.add<Acceleration>(rootCold);

		GAIA_FOR(existingMatchCount) {
			const auto cableExisting = w.add();
			w.add<Position>(cableExisting);
			w.add(cableExisting, ecs::Pair(relationHot, childCold));
		}

		const auto rootHot = w.add();
		const auto childAHot = w.add();
		const auto childBHot = w.add();
		const auto grandChildHot = w.add();
		w.child(childAHot, rootHot);
		w.child(childBHot, rootHot);
		w.child(grandChildHot, childAHot);
		w.add<Acceleration>(rootHot);

		const auto cableA = w.add();
		const auto cableB = w.add();
		const auto cableC = w.add();
		w.add<Position>(cableA);
		w.add<Position>(cableB);
		w.add<Position>(cableC);
		w.add(cableA, ecs::Pair(relationHot, childAHot));
		w.add(cableB, ecs::Pair(relationHot, childBHot));
		w.add(cableC, ecs::Pair(relationHot, grandChildHot));

		w.observer()
				.event(ecs::ObserverEvent::OnDel)
				.all<Position>()
				.all(ecs::Pair(relationHot, ecs::Var0))
				.all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter&) {
					++hits;
				});

		hits = 0;
		state.start_timer();
		w.del(rootHot);
		state.stop_timer();

		dont_optimize(hits);
	}
}

void BM_Observer_DiffAncestorUnrelatedExistingMatches_OnAdd(picobench::state& state) {
	const uint32_t existingMatchCount = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		uint64_t hits = 0;

		const auto relationHot = w.add();
		const auto rootMatching = w.add();
		const auto childMatching = w.add();
		w.child(childMatching, rootMatching);
		w.add<Acceleration>(rootMatching);

		GAIA_FOR(existingMatchCount) {
			const auto cableExisting = w.add();
			w.add<Position>(cableExisting);
			w.add(cableExisting, ecs::Pair(relationHot, childMatching));
		}

		const auto rootUnrelated = w.add();
		const auto childUnrelated = w.add();
		w.child(childUnrelated, rootUnrelated);

		w.observer()
				.event(ecs::ObserverEvent::OnAdd)
				.all<Position>()
				.all(ecs::Pair(relationHot, ecs::Var0))
				.all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter&) {
					++hits;
				});

		hits = 0;
		state.start_timer();
		w.add<Acceleration>(rootUnrelated);
		state.stop_timer();

		dont_optimize(hits);
	}
}

void BM_Observer_DiffCopyExtDirectFiltered_OnAdd(picobench::state& state) {
	const uint32_t directObserverCount = (uint32_t)state.user_data();

	for (auto _: state) {
		(void)_;
		state.stop_timer();

		ecs::World w;
		uint64_t hits = 0;

		const auto relationHot = w.add();
		const auto root = w.add();
		const auto child = w.add();
		const auto src = w.add();

		w.child(child, root);
		w.add<Acceleration>(root);
		w.add<Position>(src);
		w.add(src, ecs::Pair(relationHot, child));

		w.observer()
				.event(ecs::ObserverEvent::OnAdd)
				.all<Position>()
				.all(ecs::Pair(relationHot, ecs::Var0))
				.all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter&) {
					++hits;
				});

		GAIA_FOR(directObserverCount) {
			w.observer().event(ecs::ObserverEvent::OnAdd).all<Position>().on_each([](ecs::Iter&) {});
		}

		hits = 0;
		state.start_timer();
		const auto dst = w.copy_ext(src);
		state.stop_timer();

		dont_optimize(dst);
		dont_optimize(hits);
	}
}

////////////////////////////////////////////////////////////////////////////////

void register_observers(PerfRunMode mode) {
	switch (mode) {
		case PerfRunMode::Profiling:
			PICOBENCH_SUITE_REG("Profile picks");
			PICOBENCH_REG(BM_Observer_OnAdd_50_4)
					.PICO_SETTINGS_OBS()
					.user_data(NObserverEntities)
					.label("on_add, 50 obs, 4 terms");
			return;
		case PerfRunMode::Sanitizer:
			PICOBENCH_SUITE_REG("Sanitizer picks");
			PICOBENCH_REG(BM_Observer_OnAdd_10_1).PICO_SETTINGS_SANI().user_data(1000).label("on_add, 10 obs");
			return;
		case PerfRunMode::Normal:
			PICOBENCH_SUITE_REG("Observers");
			PICOBENCH_REG(BM_Observer_OnAdd_0_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_add, 0 obs");
			PICOBENCH_REG(BM_Observer_OnAdd_1_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_add, 1 obs");
			PICOBENCH_REG(BM_Observer_OnAdd_10_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_add, 10 obs");
			PICOBENCH_REG(BM_Observer_OnAdd_50_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_add, 50 obs");
			PICOBENCH_REG(BM_Observer_OnAdd_50_2)
					.PICO_SETTINGS_OBS()
					.user_data(NObserverEntities)
					.label("on_add, 50 obs, 2 terms");
			PICOBENCH_REG(BM_Observer_OnAdd_50_4)
					.PICO_SETTINGS_OBS()
					.user_data(NObserverEntities)
					.label("on_add, 50 obs, 4 terms");
			PICOBENCH_REG(BM_Observer_OnAdd_50_8)
					.PICO_SETTINGS_OBS()
					.user_data(NObserverEntities)
					.label("on_add, 50 obs, 8 terms");
			PICOBENCH_REG(BM_Observer_OnDel_0_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_del, 0 obs");
			PICOBENCH_REG(BM_Observer_OnDel_1_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_del, 1 obs");
			PICOBENCH_REG(BM_Observer_OnDel_10_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_del, 10 obs");
			PICOBENCH_REG(BM_Observer_OnDel_50_1).PICO_SETTINGS_OBS().user_data(NObserverEntities).label("on_del, 50 obs");
			PICOBENCH_REG(BM_Observer_OnDel_50_2)
					.PICO_SETTINGS_OBS()
					.user_data(NObserverEntities)
					.label("on_del, 50 obs, 2 terms");
			PICOBENCH_REG(BM_Observer_OnDel_50_4)
					.PICO_SETTINGS_OBS()
					.user_data(NObserverEntities)
					.label("on_del, 50 obs, 4 terms");
			PICOBENCH_REG(BM_Observer_OnDel_50_8)
					.PICO_SETTINGS_OBS()
					.user_data(NObserverEntities)
					.label("on_del, 50 obs, 8 terms");
			PICOBENCH_REG(BM_Observer_DiffPairRelFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff pair-rel on_add filtered 1");
			PICOBENCH_REG(BM_Observer_DiffPairRelFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(50)
					.label("observer diff pair-rel on_add filtered 50");
			PICOBENCH_REG(BM_Observer_DiffPairRelFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(200)
					.label("observer diff pair-rel on_add filtered 200");
			PICOBENCH_REG(BM_Observer_DiffPairRelExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff pair-rel local existing 1");
			PICOBENCH_REG(BM_Observer_DiffPairRelExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(100)
					.label("observer diff pair-rel local existing 100");
			PICOBENCH_REG(BM_Observer_DiffPairRelExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1000)
					.label("observer diff pair-rel local existing 1000");
			PICOBENCH_REG(BM_Observer_DiffCopyExtFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff copy_ext on_add filtered 1");
			PICOBENCH_REG(BM_Observer_DiffCopyExtFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(50)
					.label("observer diff copy_ext on_add filtered 50");
			PICOBENCH_REG(BM_Observer_DiffCopyExtFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(200)
					.label("observer diff copy_ext on_add filtered 200");
			PICOBENCH_REG(BM_Observer_DiffCopyExtExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff copy_ext local existing 1");
			PICOBENCH_REG(BM_Observer_DiffCopyExtExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(100)
					.label("observer diff copy_ext local existing 100");
			PICOBENCH_REG(BM_Observer_DiffCopyExtExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1000)
					.label("observer diff copy_ext local existing 1000");
			PICOBENCH_REG(BM_Observer_DiffAncestorExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff ancestor local existing 1");
			PICOBENCH_REG(BM_Observer_DiffAncestorExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(100)
					.label("observer diff ancestor local existing 100");
			PICOBENCH_REG(BM_Observer_DiffAncestorExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1000)
					.label("observer diff ancestor local existing 1000");
			PICOBENCH_REG(BM_Observer_DiffRelationEdgeExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff relation-edge local existing 1");
			PICOBENCH_REG(BM_Observer_DiffRelationEdgeExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(100)
					.label("observer diff relation-edge local existing 100");
			PICOBENCH_REG(BM_Observer_DiffRelationEdgeExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1000)
					.label("observer diff relation-edge local existing 1000");
			PICOBENCH_REG(BM_Observer_DiffAncestorFilteredObservers_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff ancestor filtered observers 1");
			PICOBENCH_REG(BM_Observer_DiffAncestorFilteredObservers_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(50)
					.label("observer diff ancestor filtered observers 50");
			PICOBENCH_REG(BM_Observer_DiffAncestorFilteredObservers_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(200)
					.label("observer diff ancestor filtered observers 200");
			PICOBENCH_REG(BM_Observer_DiffAncestorCascadeDelete_OnDel)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff ancestor delete 1");
			PICOBENCH_REG(BM_Observer_DiffAncestorCascadeDelete_OnDel)
					.PICO_SETTINGS_OBS()
					.user_data(100)
					.label("observer diff ancestor delete 100");
			PICOBENCH_REG(BM_Observer_DiffAncestorCascadeDelete_OnDel)
					.PICO_SETTINGS_OBS()
					.user_data(1000)
					.label("observer diff ancestor delete 1000");
			PICOBENCH_REG(BM_Observer_DiffAncestorUnrelatedExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff ancestor unrelated existing 1");
			PICOBENCH_REG(BM_Observer_DiffAncestorUnrelatedExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(100)
					.label("observer diff ancestor unrelated existing 100");
			PICOBENCH_REG(BM_Observer_DiffAncestorUnrelatedExistingMatches_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1000)
					.label("observer diff ancestor unrelated existing 1000");
			PICOBENCH_REG(BM_Observer_DiffCopyExtDirectFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(1)
					.label("observer diff copy_ext direct-filtered 1");
			PICOBENCH_REG(BM_Observer_DiffCopyExtDirectFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(50)
					.label("observer diff copy_ext direct-filtered 50");
			PICOBENCH_REG(BM_Observer_DiffCopyExtDirectFiltered_OnAdd)
					.PICO_SETTINGS_OBS()
					.user_data(200)
					.label("observer diff copy_ext direct-filtered 200");
			return;
		default:
			return;
	}
}

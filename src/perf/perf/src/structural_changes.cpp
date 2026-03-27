#include "common.h"
#include "registry.h"

void BM_ComponentAdd_Velocity(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		create_linear_entities<false, false, false, false, false>(w, entities, n);

		// warm graph edge
		{
			auto warm = w.add();
			w.add<Position>(warm, {0.0f, 0.0f, 0.0f});
			w.add<Velocity>(warm, {0.0f, 0.0f, 0.0f});
			w.del<Velocity>(warm);
		}

		state.start_timer();

		for (auto e: entities)
			w.add<Velocity>(e, {1.0f, 0.0f, 0.0f});

		state.stop_timer();
	}
}

void BM_ComponentRemove_Velocity(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;

	for (auto _: state) {
		(void)_;
		state.stop_timer();
		ecs::World w;
		create_linear_entities<true, false, false, false, false>(w, entities, n);
		state.start_timer();

		for (auto e: entities)
			w.del<Velocity>(e);

		state.stop_timer();
	}
}

void BM_ComponentToggle_Frozen(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;
	ecs::World w;
	create_linear_entities<true, true, false, false, false>(w, entities, n);

	bool addPhase = true;
	for (auto _: state) {
		(void)_;
		if (addPhase) {
			for (uint32_t idx = 0U; idx < entities.size(); idx += 2U)
				w.add<Frozen>(entities[idx], {true});
		} else {
			for (uint32_t idx = 0U; idx < entities.size(); idx += 2U)
				w.del<Frozen>(entities[idx]);
		}
		addPhase = !addPhase;
	}
}

void BM_ComponentSetGet_ByEntity(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;
	ecs::World w;
	create_linear_entities<false, false, true, false, false>(w, entities, n);

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0ULL;

		for (auto e: entities) {
			auto& p = w.set<Position>(e);
			p.x += 0.125f;
			p.y += 0.250f;
			const auto& h = w.get<Health>(e);
			sum += (uint64_t)(uint32_t)h.value;
		}

		dont_optimize(sum);
	}
}

void BM_ComponentHasExact_ByEntity(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;
	ecs::World w;
	create_linear_entities<false, false, true, false, false>(w, entities, n);

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0ULL;

		for (auto e: entities) {
			if (w.has<Position>(e))
				++sum;
		}

		dont_optimize(sum);
	}
}

void BM_ComponentGetExact_ByEntity(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;
	ecs::World w;
	create_linear_entities<false, false, true, false, false>(w, entities, n);

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0ULL;

		for (auto e: entities) {
			const auto& p = w.get<Position>(e);
			sum += (uint64_t)(uint32_t)(p.x + p.y + p.z);
		}

		dont_optimize(sum);
	}
}

void BM_ComponentAccessorGet_Reused(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;
	ecs::World w;
	create_linear_entities<true, true, true, false, false>(w, entities, n);

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0ULL;

		for (auto e: entities) {
			const auto acc = w.acc(e);
			const auto& p0 = acc.get<Position>();
			const auto& v = acc.get<Velocity>();
			const auto& p1 = acc.get<Position>();
			const auto& a = acc.get<Acceleration>();
			const auto& p2 = acc.get<Position>();
			const auto& h = acc.get<Health>();
			sum += (uint64_t)(uint32_t)(p0.x + p1.y + p2.z + v.x + a.y + (float)h.value);
		}

		dont_optimize(sum);
	}
}

void BM_ComponentAccessorMut_Reused(picobench::state& state) {
	const uint32_t n = (uint32_t)state.user_data();
	cnt::darray<ecs::Entity> entities;
	ecs::World w;
	create_linear_entities<true, true, false, false, false>(w, entities, n);

	for (auto _: state) {
		(void)_;
		uint64_t sum = 0ULL;

		for (auto e: entities) {
			auto acc = w.acc_mut(e);
			auto& p = acc.mut<Position>();
			const auto& v = acc.get<Velocity>();
			const auto& a = acc.get<Acceleration>();
			p.x += v.x;
			p.y += a.y;
			auto& pAgain = acc.smut<Position>();
			pAgain.z += 1.0f;
			sum += (uint64_t)(uint32_t)(p.x + pAgain.y + pAgain.z);
		}

		dont_optimize(sum);
	}
}

////////////////////////////////////////////////////////////////////////////////

void register_structural_changes(PerfRunMode mode) {
	switch (mode) {
		case PerfRunMode::Sanitizer:
			PICOBENCH_SUITE_REG("Sanitizer picks");
			PICOBENCH_REG(BM_ComponentAdd_Velocity).PICO_SETTINGS_SANI().user_data(NEntitiesFew).label("add velocity");
			return;
		case PerfRunMode::Normal:
			PICOBENCH_SUITE_REG("Structural changes");
			PICOBENCH_REG(BM_ComponentAdd_Velocity).PICO_SETTINGS().user_data(NEntitiesMedium).label("add velocity");
			PICOBENCH_REG(BM_ComponentRemove_Velocity).PICO_SETTINGS().user_data(NEntitiesMedium).label("remove velocity");
			PICOBENCH_REG(BM_ComponentToggle_Frozen).PICO_SETTINGS().user_data(NEntitiesMedium).label("toggle frozen");
			PICOBENCH_REG(BM_ComponentSetGet_ByEntity).PICO_SETTINGS().user_data(NEntitiesMedium).label("set/get by entity");
			PICOBENCH_REG(BM_ComponentHasExact_ByEntity)
					.PICO_SETTINGS()
					.user_data(NEntitiesMedium)
					.label("has exact by entity");
			PICOBENCH_REG(BM_ComponentGetExact_ByEntity)
					.PICO_SETTINGS()
					.user_data(NEntitiesMedium)
					.label("get exact by entity");
			PICOBENCH_REG(BM_ComponentAccessorGet_Reused)
					.PICO_SETTINGS_FOCUS()
					.user_data(NEntitiesFew)
					.label("accessor get reused 10K");
			PICOBENCH_REG(BM_ComponentAccessorMut_Reused)
					.PICO_SETTINGS_FOCUS()
					.user_data(NEntitiesFew)
					.label("accessor mut reused 10K");
			return;
		case PerfRunMode::Profiling:
		default:
			return;
	}
}

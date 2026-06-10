#define PICOBENCH_IMPLEMENT
#include <gaia.h>
#include <picobench/picobench.hpp>
#include <string_view>

#define ECS_ITER_COMPIDX_CACHING 1

using namespace gaia;

float dt;

struct Position {
	float x, y, z;
};
struct PositionSoA {
	GAIA_LAYOUT(SoA);
	float x, y, z;
};
struct Velocity {
	float x, y, z;
};
struct VelocitySoA {
	GAIA_LAYOUT(SoA);
	float x, y, z;
};
struct Rotation {
	float x, y, z, w;
};
struct Scale {
	float x, y, z;
};
struct Direction {
	float x, y, z;
};
struct Health {
	int value;
	int max;
};
struct IsEnemy {
	bool value;
};
struct Dummy {
	int value[24];
};

constexpr uint32_t NFew = 100'000; // kept a multiple of 32 to keep it simple even for SIMD code
constexpr uint32_t NMany = 1'000'000; // kept a multiple of 32 to keep it simple even for SIMD code

constexpr float MinDelta = 0.01f;
constexpr float MaxDelta = 0.033f;

float CalculateDelta(picobench::state& state) {
	state.stop_timer();
	const float d = static_cast<float>((double)RAND_MAX / (MaxDelta - MinDelta));
	float delta = MinDelta + (static_cast<float>(rand()) / d);
	state.start_timer();
	return delta;
}

template <bool SoA>
void Register_ESC_Components(ecs::World& w) {
	if constexpr (SoA) {
		(void)w.add<PositionSoA>();
		(void)w.add<VelocitySoA>();
	} else {
		(void)w.add<Position>();
		(void)w.add<Velocity>();
	}
	(void)w.add<Rotation>();
	(void)w.add<Scale>();
	(void)w.add<Direction>();
	(void)w.add<Health>();
	(void)w.add<IsEnemy>();
}

template <bool SoA>
void CreateECSEntities_Static(ecs::World& w, uint32_t N) {
	{
		auto e = w.add();
		if constexpr (SoA)
			w.add<PositionSoA>(e, {0, 100, 0});
		else
			w.add<Position>(e, {0, 100, 0});
		w.add<Rotation>(e, {1, 2, 3, 4});
		w.add<Scale>(e, {1, 1, 1});
		w.copy_n(e, N - 1);
	}
}

template <bool SoA>
void CreateECSEntities_Dynamic(ecs::World& w, uint32_t N) {
	{
		auto e = w.add();
		if constexpr (SoA)
			w.add<PositionSoA>(e, {0, 100, 0});
		else
			w.add<Position>(e, {0, 100, 0});
		w.add<Rotation>(e, {1, 2, 3, 4});
		w.add<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.add<VelocitySoA>(e, {0, 0, 1});
		else
			w.add<Velocity>(e, {0, 0, 1});
		w.copy_n(e, (N / 4) - 1);
	}
	{
		auto e = w.add();
		if constexpr (SoA)
			w.add<PositionSoA>(e, {0, 100, 0});
		else
			w.add<Position>(e, {0, 100, 0});
		w.add<Rotation>(e, {1, 2, 3, 4});
		w.add<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.add<VelocitySoA>(e, {0, 0, 1});
		else
			w.add<Velocity>(e, {0, 0, 1});
		w.add<Direction>(e, {0, 0, 1});
		w.copy_n(e, (N / 4) - 1);
	}
	{
		auto e = w.add();
		if constexpr (SoA)
			w.add<PositionSoA>(e, {0, 100, 0});
		else
			w.add<Position>(e, {0, 100, 0});
		w.add<Rotation>(e, {1, 2, 3, 4});
		w.add<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.add<VelocitySoA>(e, {0, 0, 1});
		else
			w.add<Velocity>(e, {0, 0, 1});
		w.add<Direction>(e, {0, 0, 1});
		w.add<Health>(e, {100, 100});
		w.copy_n(e, (N / 4) - 1);
	}
	{
		auto e = w.add();
		if constexpr (SoA)
			w.add<PositionSoA>(e, {0, 100, 0});
		else
			w.add<Position>(e, {0, 100, 0});
		w.add<Rotation>(e, {1, 2, 3, 4});
		w.add<Scale>(e, {1, 1, 1});
		if constexpr (SoA)
			w.add<VelocitySoA>(e, {0, 0, 1});
		else
			w.add<Velocity>(e, {0, 0, 1});
		w.add<Direction>(e, {0, 0, 1});
		w.add<Health>(e, {100, 100});
		w.add<IsEnemy>(e, {false});
		w.copy_n(e, (N / 4) - 1);
	}
}

void BM_ECS(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS);

	ecs::World w;

	auto queryPosCVel = w.query().all<Position&>().all<Velocity>();
	auto queryPosVel = w.query().all<Position&>().all<Velocity&>();
	auto queryVel = w.query().all<Velocity&>();
	auto queryCHealth = w.query().all<Health>();

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);

		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
		gaia::dont_optimize(queryPosCVel.empty());
		gaia::dont_optimize(queryPosVel.empty());
		gaia::dont_optimize(queryVel.empty());
		gaia::dont_optimize(queryCHealth.empty());
	}

	srand(0);
	for (auto _: state) {
		(void)_;
		const float cdt = CalculateDelta(state);

		// Update position
		{
			GAIA_PROF_SCOPE(update_pos);
			queryPosCVel.each([&](Position& p, const Velocity& v) {
				p.x += v.x * cdt;
				p.y += v.y * cdt;
				p.z += v.z * cdt;
			});
		}
		// Handle ground collision
		{
			GAIA_PROF_SCOPE(handle_collision);
			queryPosVel.each([&](Position& p, Velocity& v) {
				if (p.y < 0.0f) {
					p.y = 0.0f;
					v.y = 0.0f;
				}
			});
		}
		// Apply gravity
		{
			GAIA_PROF_SCOPE(apply_gravity);
			queryVel.each([&](Velocity& v) {
				v.y += 9.81f * cdt;
			});
		}
		// Calculate the number of units alive
		{
			GAIA_PROF_SCOPE(calc_alive);
			uint32_t aliveUnits = 0;
			queryCHealth.each([&](const Health& h) {
				if (h.value > 0)
					++aliveUnits;
			});
			gaia::dont_optimize(aliveUnits);
		}

		GAIA_PROF_FRAME();
	}
}

void BM_ECS_ReadPositionOnly(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_ReadPositionOnly);

	ecs::World w;
	auto queryPos = w.query().all<Position>();

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);
		gaia::dont_optimize(queryPos.empty());
	}

	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		queryPos.each([&](const Position& p) {
			sum += p.x + p.y + p.z;
		});
		gaia::dont_optimize(sum);
	}
}

void BM_ECS_UpdatePositionOnly(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_UpdatePositionOnly);

	ecs::World w;
	auto queryPosCVel = w.query().all<Position&>().all<Velocity>();

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);
		gaia::dont_optimize(queryPosCVel.empty());
	}

	for (auto _: state) {
		(void)_;
		const float cdt = CalculateDelta(state);
		queryPosCVel.each([&](Position& p, const Velocity& v) {
			p.x += v.x * cdt;
			p.y += v.y * cdt;
			p.z += v.z * cdt;
		});
	}
}

void BM_ECS_ReadPositionVelocityOnly(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_ReadPositionVelocityOnly);

	ecs::World w;
	auto queryPosCVel = w.query().all<Position>().all<Velocity>();

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);
		gaia::dont_optimize(queryPosCVel.empty());
	}

	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		queryPosCVel.each([&](const Position& p, const Velocity& v) {
			sum += p.x + p.y + p.z + v.x + v.y + v.z;
		});
		gaia::dont_optimize(sum);
	}
}

void BM_ECS_TouchPositionWithVelocity(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_TouchPositionWithVelocity);

	ecs::World w;
	auto queryPosCVel = w.query().all<Position&>().all<Velocity>();

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);
		gaia::dont_optimize(queryPosCVel.empty());
	}

	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		queryPosCVel.each([&](Position& p, const Velocity& v) {
			sum += p.x + p.y + p.z + v.x + v.y + v.z;
		});
		gaia::dont_optimize(sum);
	}
}

void BM_ECS_UpdatePositionFixedDeltaWithReadback(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_UpdatePositionFixedDeltaWithReadback);

	ecs::World w;
	auto queryPosCVel = w.query().all<Position&>().all<Velocity>();

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);
		gaia::dont_optimize(queryPosCVel.empty());
	}

	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		constexpr float cdt = 0.016f;
		queryPosCVel.each([&](Position& p, const Velocity& v) {
			p.x += v.x * cdt;
			p.y += v.y * cdt;
			p.z += v.z * cdt;
			sum += p.x + p.y + p.z;
		});
		gaia::dont_optimize(sum);
	}
}

template <bool WritePosition, typename Func>
void BM_ECS_PositionVelocityChunkRaw(picobench::state& state, Func func) {
	ecs::World w;
	auto queryPosCVel = w.query().all<Position>().all<Velocity>();

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);
		auto& queryInfo = queryPosCVel.fetch();
		queryPosCVel.match_all(queryInfo);
		gaia::dont_optimize(queryInfo.cache_archetype_view().size());
	}

	auto& queryInfo = queryPosCVel.fetch();
	const auto posComp = w.get<Position>();
	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		if constexpr (WritePosition)
			::gaia::ecs::update_version(w.world_version());

		for (const auto* pArchetype: queryInfo.cache_archetype_view()) {
			const auto& chunks = pArchetype->chunks();
			for (auto* pChunk: chunks) {
				const auto from = ecs::Iter::start_index(pChunk);
				const auto to = ecs::Iter::end_index(pChunk);
				if (from == to)
					continue;

				uint32_t posIdx = 0;
				if constexpr (WritePosition)
					posIdx = pChunk->comp_idx(posComp);
				func(*pChunk, from, to, posIdx, sum);
			}
		}
		gaia::dont_optimize(sum);
	}
}

void BM_ECS_ReadPositionVelocityChunkRaw(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_ReadPositionVelocityChunkRaw);

	BM_ECS_PositionVelocityChunkRaw<false>(
			state, [](ecs::Chunk& chunk, uint16_t from, uint16_t to, uint32_t, float& sum) {
				auto p = chunk.view<Position>(from, to);
				auto v = chunk.view<Velocity>(from, to);
				const auto cnt = (uint32_t)(to - from);
				GAIA_FOR(cnt) {
					sum += p[i].x + p[i].y + p[i].z + v[i].x + v[i].y + v[i].z;
				}
			});
}

void BM_ECS_TouchPositionWithVelocityChunkRaw(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_TouchPositionWithVelocityChunkRaw);

	BM_ECS_PositionVelocityChunkRaw<true>(
			state, [](ecs::Chunk& chunk, uint16_t from, uint16_t to, uint32_t posIdx, float& sum) {
				auto p = chunk.sview_mut<Position>(from, to);
				auto v = chunk.view<Velocity>(from, to);
				const auto cnt = (uint32_t)(to - from);
				GAIA_FOR(cnt) {
					sum += p[i].x + p[i].y + p[i].z + v[i].x + v[i].y + v[i].z;
				}
				chunk.finish_write(posIdx, from, to);
			});
}

void BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkRaw(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkRaw);

	BM_ECS_PositionVelocityChunkRaw<true>(
			state, [](ecs::Chunk& chunk, uint16_t from, uint16_t to, uint32_t posIdx, float& sum) {
				auto p = chunk.sview_mut<Position>(from, to);
				auto v = chunk.view<Velocity>(from, to);
				constexpr float cdt = 0.016f;
				const auto cnt = (uint32_t)(to - from);
				GAIA_FOR(cnt) {
					p[i].x += v[i].x * cdt;
					p[i].y += v[i].y * cdt;
					p[i].z += v[i].z * cdt;
					sum += p[i].x + p[i].y + p[i].z;
				}
				chunk.finish_write(posIdx, from, to);
			});
}

void BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkRawSilent(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkRawSilent);

	BM_ECS_PositionVelocityChunkRaw<false>(
			state, [](ecs::Chunk& chunk, uint16_t from, uint16_t to, uint32_t, float& sum) {
				auto p = chunk.sview_mut<Position>(from, to);
				auto v = chunk.view<Velocity>(from, to);
				constexpr float cdt = 0.016f;
				const auto cnt = (uint32_t)(to - from);
				GAIA_FOR(cnt) {
					p[i].x += v[i].x * cdt;
					p[i].y += v[i].y * cdt;
					p[i].z += v[i].z * cdt;
					sum += p[i].x + p[i].y + p[i].z;
				}
			});
}

template <typename Func>
void BM_ECS_PositionVelocityChunkPtrRaw(picobench::state& state, Func func) {
	ecs::World w;
	auto queryPosCVel = w.query().all<Position>().all<Velocity>();

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);
		auto& queryInfo = queryPosCVel.fetch();
		queryPosCVel.match_all(queryInfo);
		gaia::dont_optimize(queryInfo.cache_archetype_view().size());
	}

	auto& queryInfo = queryPosCVel.fetch();
	const auto posComp = w.get<Position>();
	const auto velComp = w.get<Velocity>();
	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		for (const auto* pArchetype: queryInfo.cache_archetype_view()) {
			const auto& chunks = pArchetype->chunks();
			for (auto* pChunk: chunks) {
				const auto from = ecs::Iter::start_index(pChunk);
				const auto to = ecs::Iter::end_index(pChunk);
				if (from == to)
					continue;

				const auto recs = pChunk->comp_rec_view();
				auto* p = (Position*)recs[pChunk->comp_idx(posComp)].pData + from;
				const auto* v = (const Velocity*)recs[pChunk->comp_idx(velComp)].pData + from;
				func(p, v, (uint32_t)(to - from), sum);
			}
		}
		gaia::dont_optimize(sum);
	}
}

void BM_ECS_ReadPositionVelocityChunkPtrRaw(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_ReadPositionVelocityChunkPtrRaw);

	BM_ECS_PositionVelocityChunkPtrRaw(state, [](const Position* p, const Velocity* v, uint32_t cnt, float& sum) {
		GAIA_FOR(cnt) {
			sum += p[i].x + p[i].y + p[i].z + v[i].x + v[i].y + v[i].z;
		}
	});
}

void BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawSilent(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawSilent);

	BM_ECS_PositionVelocityChunkPtrRaw(state, [](Position* p, const Velocity* v, uint32_t cnt, float& sum) {
		constexpr float cdt = 0.016f;
		GAIA_FOR(cnt) {
			p[i].x += v[i].x * cdt;
			p[i].y += v[i].y * cdt;
			p[i].z += v[i].z * cdt;
			sum += p[i].x + p[i].y + p[i].z;
		}
	});
}

void BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawLocalSumSilent(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawLocalSumSilent);

	BM_ECS_PositionVelocityChunkPtrRaw(state, [](Position* p, const Velocity* v, uint32_t cnt, float& sum) {
		float localSum = 0.0f;
		constexpr float cdt = 0.016f;
		GAIA_FOR(cnt) {
			p[i].x += v[i].x * cdt;
			p[i].y += v[i].y * cdt;
			p[i].z += v[i].z * cdt;
			localSum += p[i].x + p[i].y + p[i].z;
		}
		sum += localSum;
	});
}

GAIA_NOINLINE float UpdatePositionFixedDeltaWithReadbackRestrictLocalSum(
		Position* GAIA_RESTRICT p, const Velocity* GAIA_RESTRICT v, uint32_t cnt) {
	float sum = 0.0f;
	constexpr float cdt = 0.016f;
	GAIA_FOR(cnt) {
		p[i].x += v[i].x * cdt;
		p[i].y += v[i].y * cdt;
		p[i].z += v[i].z * cdt;
		sum += p[i].x + p[i].y + p[i].z;
	}
	return sum;
}

template <bool FinishWrites>
void BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawRestrictLocalSumBase(picobench::state& state) {
	ecs::World w;
	auto queryPosCVel = w.query().all<Position>().all<Velocity>();

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);
		auto& queryInfo = queryPosCVel.fetch();
		queryPosCVel.match_all(queryInfo);
		gaia::dont_optimize(queryInfo.cache_archetype_view().size());
	}

	auto& queryInfo = queryPosCVel.fetch();
	const auto posComp = w.get<Position>();
	const auto velComp = w.get<Velocity>();
	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		if constexpr (FinishWrites)
			::gaia::ecs::update_version(w.world_version());

		for (const auto* pArchetype: queryInfo.cache_archetype_view()) {
			const auto& chunks = pArchetype->chunks();
			for (auto* pChunk: chunks) {
				const auto from = ecs::Iter::start_index(pChunk);
				const auto to = ecs::Iter::end_index(pChunk);
				if (from == to)
					continue;

				const auto posIdx = pChunk->comp_idx(posComp);
				const auto recs = pChunk->comp_rec_view();
				auto* p = (Position*)recs[posIdx].pData + from;
				const auto* v = (const Velocity*)recs[pChunk->comp_idx(velComp)].pData + from;
				sum += UpdatePositionFixedDeltaWithReadbackRestrictLocalSum(p, v, (uint32_t)(to - from));
				if constexpr (FinishWrites)
					pChunk->finish_write(posIdx, from, to);
			}
		}
		gaia::dont_optimize(sum);
	}
}

void BM_ECS_UpdatePositionFixedDeltaWithReadbackIter(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_UpdatePositionFixedDeltaWithReadbackIter);

	ecs::World w;
	auto queryPosCVel = w.query().all<Position&>().all<Velocity>();

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);
		gaia::dont_optimize(queryPosCVel.empty());
	}

	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		queryPosCVel.each([&](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
			auto p = it.view_mut<Position>(0);
			auto v = it.view<Velocity>(1);
#else
			auto p = it.view_mut<Position>();
			auto v = it.view<Velocity>();
#endif
			constexpr float cdt = 0.016f;
			const auto cnt = it.size();
			GAIA_FOR(cnt) {
				p[i].x += v[i].x * cdt;
				p[i].y += v[i].y * cdt;
				p[i].z += v[i].z * cdt;
				sum += p[i].x + p[i].y + p[i].z;
			}
		});
		gaia::dont_optimize(sum);
	}
}

void BM_ECS_UpdatePositionFixedDeltaWithReadbackIterLocalAccum(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_UpdatePositionFixedDeltaWithReadbackIterLocalAccum);

	ecs::World w;
	auto queryPosCVel = w.query().all<Position&>().all<Velocity>();

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);
		gaia::dont_optimize(queryPosCVel.empty());
	}

	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		queryPosCVel.each([&](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
			auto p = it.view_mut<Position>(0);
			auto v = it.view<Velocity>(1);
#else
			auto p = it.view_mut<Position>();
			auto v = it.view<Velocity>();
#endif
			float localSum = 0.0f;
			constexpr float cdt = 0.016f;
			const auto cnt = it.size();
			GAIA_FOR(cnt) {
				p[i].x += v[i].x * cdt;
				p[i].y += v[i].y * cdt;
				p[i].z += v[i].z * cdt;
				localSum += p[i].x + p[i].y + p[i].z;
			}
			sum += localSum;
		});
		gaia::dont_optimize(sum);
	}
}

void BM_ECS_UpdatePositionFixedDeltaWithReadbackIterLocalSum(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_UpdatePositionFixedDeltaWithReadbackIterLocalSum);

	ecs::World w;
	auto queryPosCVel = w.query().all<Position&>().all<Velocity>();

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);
		gaia::dont_optimize(queryPosCVel.empty());
	}

	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		queryPosCVel.each([&](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
			auto p = it.view_mut<Position>(0);
			auto v = it.view<Velocity>(1);
#else
			auto p = it.view_mut<Position>();
			auto v = it.view<Velocity>();
#endif
			sum += UpdatePositionFixedDeltaWithReadbackRestrictLocalSum(p.data(), v.data(), (uint32_t)it.size());
		});
		gaia::dont_optimize(sum);
	}
}

void BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawRestrictLocalSumSilent(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawRestrictLocalSumSilent);

	BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawRestrictLocalSumBase<false>(state);
}

void BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawRestrictLocalSum(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawRestrictLocalSum);

	BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawRestrictLocalSumBase<true>(state);
}

template <uint32_t Groups>
void BM_NonECS_DOD_ReadPositionVelocityChunked(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_NonECS_DOD_ReadPositionVelocityChunked);

	const auto N = (uint32_t)state.user_data() / 2;
	const uint32_t NGroup = N / Groups;
	struct Group {
		cnt::darray<Position> positions;
		cnt::darray<Velocity> velocities;
	} groups[Groups];

	{
		GAIA_PROF_SCOPE(setup);
		GAIA_FOR_(Groups, groupIdx) {
			auto& group = groups[groupIdx];
			group.positions.resize(NGroup);
			group.velocities.resize(NGroup);
			GAIA_FOR(NGroup) {
				group.positions[i] = {0, 100, 0};
				group.velocities[i] = {0, 0, 1};
			}
		}
	}

	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		GAIA_FOR_(Groups, groupIdx) {
			const auto& group = groups[groupIdx];
			const auto* positions = group.positions.data();
			const auto* velocities = group.velocities.data();
			GAIA_FOR(NGroup) {
				sum += positions[i].x + positions[i].y + positions[i].z + velocities[i].x + velocities[i].y + velocities[i].z;
			}
		}
		gaia::dont_optimize(sum);
	}
}

void BM_NonECS_DOD_ReadPositionVelocityOnly(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_NonECS_DOD_ReadPositionVelocityOnly);

	const auto N = (uint32_t)state.user_data() / 2;
	cnt::darray<Position> positions(N);
	cnt::darray<Velocity> velocities(N);

	{
		GAIA_PROF_SCOPE(setup);
		GAIA_FOR(N) {
			positions[i] = {0, 100, 0};
			velocities[i] = {0, 0, 1};
		}
	}

	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		GAIA_FOR(N) {
			sum += positions[i].x + positions[i].y + positions[i].z + velocities[i].x + velocities[i].y + velocities[i].z;
		}
		gaia::dont_optimize(sum);
	}
}

template <uint32_t Groups>
void BM_NonECS_DOD_UpdatePositionFixedDeltaWithReadbackChunked(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_NonECS_DOD_UpdatePositionFixedDeltaWithReadbackChunked);

	const auto N = (uint32_t)state.user_data() / 2;
	const uint32_t NGroup = N / Groups;
	struct Group {
		cnt::darray<Position> positions;
		cnt::darray<Velocity> velocities;
	} groups[Groups];

	{
		GAIA_PROF_SCOPE(setup);
		GAIA_FOR_(Groups, groupIdx) {
			auto& group = groups[groupIdx];
			group.positions.resize(NGroup);
			group.velocities.resize(NGroup);
			GAIA_FOR(NGroup) {
				group.positions[i] = {0, 100, 0};
				group.velocities[i] = {0, 0, 1};
			}
		}
	}

	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		constexpr float cdt = 0.016f;
		GAIA_FOR_(Groups, groupIdx) {
			auto& group = groups[groupIdx];
			auto* positions = group.positions.data();
			const auto* velocities = group.velocities.data();
			GAIA_FOR(NGroup) {
				positions[i].x += velocities[i].x * cdt;
				positions[i].y += velocities[i].y * cdt;
				positions[i].z += velocities[i].z * cdt;
				sum += positions[i].x + positions[i].y + positions[i].z;
			}
		}
		gaia::dont_optimize(sum);
	}
}

void BM_NonECS_DOD_UpdatePositionFixedDeltaWithReadback(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_NonECS_DOD_UpdatePositionFixedDeltaWithReadback);

	const auto N = (uint32_t)state.user_data() / 2;
	cnt::darray<Position> positions(N);
	cnt::darray<Velocity> velocities(N);

	{
		GAIA_PROF_SCOPE(setup);
		GAIA_FOR(N) {
			positions[i] = {0, 100, 0};
			velocities[i] = {0, 0, 1};
		}
	}

	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		constexpr float cdt = 0.016f;
		GAIA_FOR(N) {
			positions[i].x += velocities[i].x * cdt;
			positions[i].y += velocities[i].y * cdt;
			positions[i].z += velocities[i].z * cdt;
			sum += positions[i].x + positions[i].y + positions[i].z;
		}
		gaia::dont_optimize(sum);
	}
}

void BM_ECS_ReadVelocityOnly(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_ReadVelocityOnly);

	ecs::World w;
	auto queryVel = w.query().all<Velocity>();

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);
		gaia::dont_optimize(queryVel.empty());
	}

	for (auto _: state) {
		(void)_;
		float sum = 0.0f;
		queryVel.each([&](const Velocity& v) {
			sum += v.x + v.y + v.z;
		});
		gaia::dont_optimize(sum);
	}
}

void BM_ECS_WriteVelocityOnly(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_WriteVelocityOnly);

	ecs::World w;
	auto queryVel = w.query().all<Velocity&>();

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);
		gaia::dont_optimize(queryVel.empty());
	}

	for (auto _: state) {
		(void)_;
		const float cdt = CalculateDelta(state);
		queryVel.each([&](Velocity& v) {
			v.y += 9.81f * cdt;
		});
	}
}

void BM_ECS_Iter(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_WithSystems);

	ecs::World w;

	w.system().name("update_pos").all<Position&>().all<Velocity>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto p = it.view_any_mut<Position>(0);
		auto v = it.view_any<Velocity>(1);
#else
		auto p = it.view_any_mut<Position>();
		auto v = it.view_any<Velocity>();
#endif
		const float cdt = dt;

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			p[i].x += v[i].x * cdt;
			p[i].y += v[i].y * cdt;
			p[i].z += v[i].z * cdt;
		}
	});

	w.system().name("handle_collision").all<Position&>().all<Velocity>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto p = it.view_any_mut<Position>(0);
		auto v = it.view_any_mut<Velocity>(1);
#else
		auto p = it.view_any_mut<Position>();
		auto v = it.view_any_mut<Velocity>();
#endif

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			if (p[i].y < 0.0f) {
				p[i].y = 0.0f;
				v[i].y = 0.0f;
			}
		}
	});

	w.system().name("apply_gravity").all<Velocity>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto v = it.view_any_mut<Velocity>(0);
#else
		auto v = it.view_any_mut<Velocity>();
#endif
		const float cdt = dt;

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			v[i].y += 9.81f * cdt;
		}
	});

	w.system().name("calc_alive").all<Health>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto h = it.view_any<Health>(0);
#else
		auto h = it.view_any<Health>();
#endif
		uint32_t aliveUnits = 0;

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			if (h[i].value > 0)
				++aliveUnits;
		}
		gaia::dont_optimize(aliveUnits);
	});

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);

		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
		for (uint32_t i = 0; i < 10; ++i)
			w.systems_run();
	}

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);
		w.systems_run();
	}
}

void BM_ECS_Iter_Dir(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_WithSystems);

	ecs::World w;

	w.system().name("update_pos").all<Position&>().all<Velocity>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto p = it.view_mut<Position>(0);
		auto v = it.view<Velocity>(1);
#else
		auto p = it.view_mut<Position>();
		auto v = it.view<Velocity>();
#endif
		const float cdt = dt;

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			p[i].x += v[i].x * cdt;
			p[i].y += v[i].y * cdt;
			p[i].z += v[i].z * cdt;
		}
	});

	w.system().name("handle_collision").all<Position&>().all<Velocity>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto p = it.view_mut<Position>(0);
		auto v = it.view_mut<Velocity>(1);
#else
		auto p = it.view_mut<Position>();
		auto v = it.view_mut<Velocity>();
#endif

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			if (p[i].y < 0.0f) {
				p[i].y = 0.0f;
				v[i].y = 0.0f;
			}
		}
	});

	w.system().name("apply_gravity").all<Velocity>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto v = it.view_mut<Velocity>(0);
#else
		auto v = it.view_mut<Velocity>();
#endif
		const float cdt = dt;

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			v[i].y += 9.81f * cdt;
		}
	});

	w.system().name("calc_alive").all<Health>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto h = it.view<Health>(0);
#else
		auto h = it.view<Health>();
#endif
		uint32_t aliveUnits = 0;

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			if (h[i].value > 0)
				++aliveUnits;
		}
		gaia::dont_optimize(aliveUnits);
	});

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<false>(w);
		CreateECSEntities_Static<false>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<false>(w, (uint32_t)state.user_data() / 2);

		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
		for (uint32_t i = 0; i < 10; ++i)
			w.systems_run();
	}

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);
		w.systems_run();
	}
}

void BM_ECS_Iter_SoA(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_Iter_SoA);

	ecs::World w;

	w.system().name("update_pos").all<PositionSoA&>().all<VelocitySoA>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto p = it.view_any_mut<PositionSoA>(0);
		auto v = it.view_any<VelocitySoA>(1);
#else
		auto p = it.view_any_mut<PositionSoA>();
		auto v = it.view_any<VelocitySoA>();
#endif
		const float cdt = dt;

		auto ppx = p.set<0>();
		auto ppy = p.set<1>();
		auto ppz = p.set<2>();

		auto vvx = v.get<0>();
		auto vvy = v.get<1>();
		auto vvz = v.get<2>();

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			ppx[i] += vvx[i] * cdt;
			ppy[i] += vvy[i] * cdt;
			ppz[i] += vvz[i] * cdt;
		}
	});

	w.system().name("handle_collision").all<PositionSoA&>().all<VelocitySoA>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto p = it.view_any_mut<PositionSoA>(0);
		auto v = it.view_any_mut<VelocitySoA>(1);
#else
		auto p = it.view_any_mut<PositionSoA>();
		auto v = it.view_any_mut<VelocitySoA>();
#endif
		auto ppy = p.set<1>();
		auto vvy = v.set<1>();

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			if (ppy[i] < 0.0f) {
				ppy[i] = 0.0f;
				vvy[i] = 0.0f;
			}
		}
	});

	w.system().name("apply_gravity").all<VelocitySoA>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto v = it.view_any_mut<VelocitySoA>(0);
#else
		auto v = it.view_any_mut<VelocitySoA>();
#endif
		const float cdt = dt;

		auto vvy = v.set<1>();

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			vvy[i] += 9.81f * cdt;
		}
	});

	w.system().name("calc_alive").all<Health>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto h = it.view_any<Health>(0);
#else
		auto h = it.view_any<Health>();
#endif
		uint32_t aliveUnits = 0;

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			if (h[i].value > 0)
				++aliveUnits;
		}
		gaia::dont_optimize(aliveUnits);
	});

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<true>(w);
		CreateECSEntities_Static<true>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<true>(w, (uint32_t)state.user_data() / 2);

		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
		for (uint32_t i = 0; i < 10; ++i)
			w.systems_run();
	}

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);
		w.systems_run();
	}
}

void BM_ECS_Iter_SoA_Dir(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_Iter_SoA);

	ecs::World w;

	w.system().name("update_pos").all<PositionSoA&>().all<VelocitySoA>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto p = it.view_mut<PositionSoA>(0);
		auto v = it.view<VelocitySoA>(1);
#else
		auto p = it.view_mut<PositionSoA>();
		auto v = it.view<VelocitySoA>();
#endif
		const float cdt = dt;

		auto ppx = p.set<0>();
		auto ppy = p.set<1>();
		auto ppz = p.set<2>();

		auto vvx = v.get<0>();
		auto vvy = v.get<1>();
		auto vvz = v.get<2>();

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			ppx[i] += vvx[i] * cdt;
			ppy[i] += vvy[i] * cdt;
			ppz[i] += vvz[i] * cdt;
		}
	});

	w.system().name("handle_collision").all<PositionSoA&>().all<VelocitySoA>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto p = it.view_mut<PositionSoA>(0);
		auto v = it.view_mut<VelocitySoA>(1);
#else
		auto p = it.view_mut<PositionSoA>();
		auto v = it.view_mut<VelocitySoA>();
#endif
		auto ppy = p.set<1>();
		auto vvy = v.set<1>();

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			if (ppy[i] < 0.0f) {
				ppy[i] = 0.0f;
				vvy[i] = 0.0f;
			}
		}
	});

	w.system().name("apply_gravity").all<VelocitySoA>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto v = it.view_mut<VelocitySoA>(0);
#else
		auto v = it.view_mut<VelocitySoA>();
#endif
		const float cdt = dt;

		auto vvy = v.set<1>();

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			vvy[i] += 9.81f * cdt;
		}
	});

	w.system().name("calc_alive").all<Health>().on_each([](ecs::Iter& it) {
#if ECS_ITER_COMPIDX_CACHING
		auto h = it.view<Health>(0);
#else
		auto h = it.view<Health>();
#endif
		uint32_t aliveUnits = 0;

		const auto cnt = it.size();
		GAIA_FOR(cnt) {
			if (h[i].value > 0)
				++aliveUnits;
		}
		gaia::dont_optimize(aliveUnits);
	});

	{
		GAIA_PROF_SCOPE(setup);
		Register_ESC_Components<true>(w);
		CreateECSEntities_Static<true>(w, (uint32_t)state.user_data() / 2);
		CreateECSEntities_Dynamic<true>(w, (uint32_t)state.user_data() / 2);

		/* We want to benchmark the hot-path. In real-world scenarios queries are cached so cache them now */
		for (uint32_t i = 0; i < 10; ++i)
			w.systems_run();
	}

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);
		w.systems_run();
	}
}

namespace NonECS {
	struct IUnit {
		Position p;
		Rotation r;
		Scale s;
		//! This is a bunch of generic data that keeps getting added
		//! to the base class over its lifetime and is rarely used ever.
		Dummy dummy[4];

		IUnit() noexcept = default;
		virtual ~IUnit() = default;

		IUnit(const IUnit&) = default;
		IUnit(IUnit&&) noexcept = default;
		IUnit& operator=(const IUnit&) = default;
		IUnit& operator=(IUnit&&) noexcept = default;

		virtual void update_pos(float deltaTime) = 0;
		virtual void updatePosition_verify() {}

		virtual void handle_collision(float deltaTime) = 0;
		virtual void handleGroundCollision_verify() {}

		virtual void apply_gravity(float deltaTime) = 0;
		virtual void applyGravity_verify() {}

		virtual bool isAlive() const = 0;
		virtual void isAlive_verify() {}
	};

	struct UnitStatic: public IUnit {
		void update_pos([[maybe_unused]] float deltaTime) override {}
		void handle_collision([[maybe_unused]] float deltaTime) override {}
		void apply_gravity([[maybe_unused]] float deltaTime) override {}
		bool isAlive() const override {
			return true;
		}
	};

	struct UnitDynamic1: public IUnit {
		Velocity v;

		void update_pos(float deltaTime) override {
			p.x += v.x * deltaTime;
			p.y += v.y * deltaTime;
			p.z += v.z * deltaTime;
		}
		void updatePosition_verify() override {
			gaia::dont_optimize(p.x);
		}

		void handle_collision([[maybe_unused]] float deltaTime) override {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() override {
			gaia::dont_optimize(v.y);
		}

		void apply_gravity(float deltaTime) override {
			v.y += 9.81f * deltaTime;
		}
		void applyGravity_verify() override {
			gaia::dont_optimize(v.y);
		}

		bool isAlive() const override {
			return true;
		}
	};

	struct UnitDynamic2: public IUnit {
		Velocity v;
		Direction d;

		void update_pos(float deltaTime) override {
			p.x += v.x * deltaTime;
			p.y += v.y * deltaTime;
			p.z += v.z * deltaTime;
		}
		void updatePosition_verify() override {
			gaia::dont_optimize(p.x);
		}

		void handle_collision([[maybe_unused]] float deltaTime) override {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() override {
			gaia::dont_optimize(v.y);
		}

		void apply_gravity(float deltaTime) override {
			v.y += 9.81f * deltaTime;
		}
		void applyGravity_verify() override {
			gaia::dont_optimize(v.y);
		}

		bool isAlive() const override {
			return true;
		}
	};

	struct UnitDynamic3: public IUnit {
		Velocity v;
		Direction d;
		Health h;

		void update_pos(float deltaTime) override {
			p.x += v.x * deltaTime;
			p.y += v.y * deltaTime;
			p.z += v.z * deltaTime;
		}
		void updatePosition_verify() override {
			gaia::dont_optimize(p.x);
		}

		void handle_collision([[maybe_unused]] float deltaTime) override {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() override {
			gaia::dont_optimize(v.x);
		}

		void apply_gravity(float deltaTime) override {
			v.y += 9.81f * deltaTime;
		}
		void applyGravity_verify() override {
			gaia::dont_optimize(v.y);
		}

		bool isAlive() const override {
			return h.value > 0;
		}
		void isAlive_verify() override {
			gaia::dont_optimize(h.value);
		}
	};

	struct UnitDynamic4: public IUnit {
		Velocity v;
		Direction d;
		Health h;
		IsEnemy e;

		void update_pos(float deltaTime) override {
			p.x += v.x * deltaTime;
			p.y += v.y * deltaTime;
			p.z += v.z * deltaTime;
		}
		void updatePosition_verify() override {
			gaia::dont_optimize(p.x);
		}

		void handle_collision([[maybe_unused]] float deltaTime) override {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() override {
			gaia::dont_optimize(v.x);
		}

		void apply_gravity(float deltaTime) override {
			v.y += 9.81f * deltaTime;
		}
		void applyGravity_verify() override {
			gaia::dont_optimize(v.y);
		}

		bool isAlive() const override {
			return h.value > 0;
		}
		void isAlive_verify() override {
			gaia::dont_optimize(h.value);
		}
	};
} // namespace NonECS

template <bool AlternativeExecOrder>
void BM_NonECS(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_NonECS);

	using namespace NonECS;

	// Create entities.
	// We allocate via new to simulate the usual kind of behavior in games
	const auto N = (uint32_t)state.user_data() / 2;
	cnt::darray<IUnit*> units(N * 2);
	{
		GAIA_PROF_SCOPE(setup);

		GAIA_FOR(N) {
			auto* u = new UnitStatic();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			units[i] = u;
		}
		uint32_t j = N;
		GAIA_FOR(N / 4) {
			auto* u = new UnitDynamic1();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		GAIA_FOR(N / 4) {
			auto* u = new UnitDynamic2();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		GAIA_FOR(N / 4) {
			auto* u = new UnitDynamic3();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
		j += N / 4;
		GAIA_FOR(N / 4) {
			auto* u = new UnitDynamic4();
			u->p = {0, 100, 0};
			u->r = {1, 2, 3, 4};
			u->s = {1, 1, 1};
			u->v = {0, 0, 1};
			units[j + i] = u;
		}
	}

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		uint32_t aliveUnits = 0;

		// Process entities
		if constexpr (AlternativeExecOrder) {
			{
				GAIA_PROF_SCOPE(update_pos);
				for (auto& u: units)
					u->update_pos(dt);
			}
			{
				GAIA_PROF_SCOPE(handle_collision);
				for (auto& u: units)
					u->handle_collision(dt);
			}
			{
				GAIA_PROF_SCOPE(apply_gravity);
				for (auto& u: units)
					u->apply_gravity(dt);
			}
			{
				GAIA_PROF_SCOPE(calc_alive);
				for (auto& u: units) {
					if (u->isAlive())
						++aliveUnits;
				}
			}
		} else {
			{
				GAIA_PROF_SCOPE(calc_main);
				for (auto& u: units) {
					u->update_pos(dt);
					u->handle_collision(dt);
					u->apply_gravity(dt);
				}
			}
			{
				GAIA_PROF_SCOPE(calc_alive);
				for (auto& u: units) {
					if (u->isAlive())
						++aliveUnits;
				}
			}
		}

		(void)aliveUnits;

		units[0]->updatePosition_verify();
		units[0]->handleGroundCollision_verify();
		units[0]->applyGravity_verify();
		units[0]->isAlive_verify();

		GAIA_PROF_FRAME();
	}

	for (const auto& u: units) {
		delete u;
	}
}

namespace NonECS_BetterMemoryLayout {
	struct UnitData {
		Position p;
		Rotation r;
		Scale s;
		//! This is a bunch of generic data that keeps getting added
		//! to the base class over its lifetime and is rarely used ever.
		Dummy dummy[4];
	};

	struct UnitStatic: UnitData {
		void update_pos([[maybe_unused]] float deltaTime) {}
		void updatePosition_verify() {}

		void handle_collision([[maybe_unused]] float deltaTime) {}
		void handleGroundCollision_verify() {}

		void apply_gravity([[maybe_unused]] float deltaTime) {}
		void applyGravity_verify() {}

		bool isAlive() const {
			return true;
		}
		void isAlive_verify() {}
	};

	struct UnitDynamic1: UnitData {
		Velocity v;

		void update_pos(float deltaTime) {
			p.x += v.x * deltaTime;
			p.y += v.y * deltaTime;
			p.z += v.z * deltaTime;
		}
		void updatePosition_verify() {
			gaia::dont_optimize(p.x);
		}

		void handle_collision([[maybe_unused]] float deltaTime) {
			if (p.y < 0.0f) {
				p.y = 0.0f;
				v.y = 0.0f;
			}
		}
		void handleGroundCollision_verify() {
			gaia::dont_optimize(v.y);
		}

		void apply_gravity(float deltaTime) {
			v.y += 9.81f * deltaTime;
		}
		void applyGravity_verify() {
			gaia::dont_optimize(v.y);
		}

		bool isAlive() const {
			return true;
		}
		void isAlive_verify() {}
	};

	struct UnitDynamic2: public UnitDynamic1 {
		Direction d;
	};

	struct UnitDynamic3: public UnitDynamic2 {
		Health h;

		using UnitDynamic2::isAlive;
		using UnitDynamic2 ::isAlive_verify;
		bool isAlive() const {
			return h.value > 0;
		}
		void isAlive_verify() {
			gaia::dont_optimize(h.value);
		}
	};

	struct UnitDynamic4: public UnitDynamic3 {
		IsEnemy e;
	};
} // namespace NonECS_BetterMemoryLayout

template <bool AlternativeExecOrder>
void BM_NonECS_BetterMemoryLayout(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_NonECS_BetterMemoryLayout);

	using namespace NonECS_BetterMemoryLayout;

	const auto N = (uint32_t)state.user_data() / 2;
	cnt::darray<UnitStatic> units_static(N);
	cnt::darray<UnitDynamic1> units_dynamic1(N / 4);
	cnt::darray<UnitDynamic2> units_dynamic2(N / 4);
	cnt::darray<UnitDynamic3> units_dynamic3(N / 4);
	cnt::darray<UnitDynamic4> units_dynamic4(N / 4);

	// Create entities
	{
		GAIA_PROF_SCOPE(setup);

		GAIA_FOR(N) {
			UnitStatic u;
			u.p = {0, 100, 0};
			u.r = {1, 2, 3, 4};
			u.s = {1, 1, 1};
			units_static[i] = GAIA_MOV(u);
		}
		GAIA_FOR(N / 4) {
			UnitDynamic1 u;
			u.p = {0, 100, 0};
			u.r = {1, 2, 3, 4};
			u.s = {1, 1, 1};
			u.v = {0, 0, 1};
			units_dynamic1[i] = GAIA_MOV(u);
		}
		GAIA_FOR(N / 4) {
			UnitDynamic2 u;
			u.p = {0, 100, 0};
			u.r = {1, 2, 3, 4};
			u.s = {1, 1, 1};
			u.v = {0, 0, 1};
			u.d = {0, 0, 1};
			units_dynamic2[i] = GAIA_MOV(u);
		}
		GAIA_FOR(N / 4) {
			UnitDynamic3 u;
			u.p = {0, 100, 0};
			u.r = {1, 2, 3, 4};
			u.v = {0, 0, 1};
			u.s = {1, 1, 1};
			u.d = {0, 0, 1};
			u.h = {100, 100};
			units_dynamic3[i] = GAIA_MOV(u);
		}
		GAIA_FOR(N / 4) {
			UnitDynamic4 u;
			u.p = {0, 100, 0};
			u.r = {1, 2, 3, 4};
			u.s = {1, 1, 1};
			u.v = {0, 0, 1};
			u.d = {0, 0, 1};
			u.h = {100, 100};
			u.e = {false};
			units_dynamic4[i] = GAIA_MOV(u);
		}
	}

	auto exec = [](auto& arr) {
		if constexpr (AlternativeExecOrder) {
			{
				GAIA_PROF_SCOPE(update_pos);
				for (auto& u: arr)
					u.update_pos(dt);
			}
			{
				GAIA_PROF_SCOPE(handle_collision);
				for (auto& u: arr)
					u.handle_collision(dt);
			}
			{
				GAIA_PROF_SCOPE(apply_gravity);
				for (auto& u: arr)
					u.apply_gravity(dt);
			}
		} else {
			GAIA_PROF_SCOPE(calc_main);
			for (auto& u: arr) {
				u.update_pos(dt);
				u.handle_collision(dt);
				u.apply_gravity(dt);
			}
		}

		arr[0].updatePosition_verify();
		arr[0].handleGroundCollision_verify();
		arr[0].applyGravity_verify();
	};

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		exec(units_static);
		exec(units_dynamic1);
		exec(units_dynamic2);
		exec(units_dynamic3);
		exec(units_dynamic4);

		{
			GAIA_PROF_SCOPE(calc_alive);
			uint32_t aliveUnits = 0;
			for (auto& u: units_dynamic3) {
				if (u.isAlive())
					++aliveUnits;
			}
			for (auto& u: units_dynamic4) {
				if (u.isAlive())
					++aliveUnits;
			}
			units_dynamic3[0].isAlive_verify();
			units_dynamic4[0].isAlive_verify();
			gaia::dont_optimize(aliveUnits);
		}

		GAIA_PROF_FRAME();
	}
}

template <uint32_t Groups>
void BM_NonECS_DOD(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_NonECS_DOD);

	struct UnitDynamic {
		static void update_pos(cnt::darray<Position>& p, const cnt::darray<Velocity>& v, const float deltaTime) {
			const auto cnt = p.size();
			GAIA_FOR(cnt) {
				p[i].x += v[i].x * deltaTime;
				p[i].y += v[i].y * deltaTime;
				p[i].z += v[i].z * deltaTime;
			}

			gaia::dont_optimize(p[0].x);
			gaia::dont_optimize(p[0].y);
			gaia::dont_optimize(p[0].z);
		}
		static void handle_collision(cnt::darray<Position>& p, cnt::darray<Velocity>& v) {
			const auto cnt = p.size();
			GAIA_FOR(cnt) {
				if (p[i].y < 0.0f) {
					p[i].y = 0.0f;
					v[i].y = 0.0f;
				}
			}

			gaia::dont_optimize(p[0].y);
			gaia::dont_optimize(v[0].y);
		}

		static void apply_gravity(cnt::darray<Velocity>& v, const float deltaTime) {
			const auto cnt = v.size();
			GAIA_FOR(cnt) {
				v[i].y += 9.81f * deltaTime;
			}

			gaia::dont_optimize(v[0].y);
		}

		static uint32_t calc_alive_units(const cnt::darray<Health>& h) {
			uint32_t aliveUnits = 0;

			const auto cnt = h.size();
			GAIA_FOR(cnt) {
				if (h[i].value > 0)
					++aliveUnits;
			}
			return aliveUnits;
		}
	};

	const auto N = (uint32_t)state.user_data() / 2;
	const uint32_t NGroup = N / Groups;

	struct static_units_group {
		cnt::darray<Position> units_p;
		cnt::darray<Rotation> units_r;
		cnt::darray<Scale> units_s;
	} static_groups[Groups];

	struct dyn_units_group {
		cnt::darray<Position> units_p;
		cnt::darray<Rotation> units_r;
		cnt::darray<Scale> units_s;
		cnt::darray<Velocity> units_v;
		cnt::darray<Direction> units_d;
		cnt::darray<Health> units_h;
		cnt::darray<IsEnemy> units_e;
	} dyn_groups[Groups];

	{
		GAIA_PROF_SCOPE(setup);

		// Create static entities
		for (auto& g: static_groups) {
			g.units_p.resize(NGroup);
			g.units_r.resize(NGroup);
			g.units_s.resize(NGroup);

			GAIA_FOR(NGroup) {
				g.units_p[i] = {0, 100, 0};
				g.units_r[i] = {1, 2, 3, 4};
				g.units_s[i] = {1, 1, 1};
			}
		}

		// Create dynamic entities
		for (auto& g: dyn_groups) {
			g.units_p.resize(NGroup);
			g.units_r.resize(NGroup);
			g.units_s.resize(NGroup);
			g.units_v.resize(NGroup);
			g.units_d.resize(NGroup);
			g.units_h.resize(NGroup);
			g.units_e.resize(NGroup);

			GAIA_FOR(NGroup) {
				g.units_p[i] = {0, 100, 0};
				g.units_r[i] = {1, 2, 3, 4};
				g.units_s[i] = {1, 1, 1};
				g.units_v[i] = {0, 0, 1};
				g.units_d[i] = {0, 0, 1};
				g.units_h[i] = {100, 100};
				g.units_e[i] = {false};
			}
		}
	}

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		{
			GAIA_PROF_SCOPE(update_pos);

			for (auto& g: dyn_groups)
				UnitDynamic::update_pos(g.units_p, g.units_v, dt);
		}

		{
			GAIA_PROF_SCOPE(handle_collision);

			for (auto& g: dyn_groups)
				UnitDynamic::handle_collision(g.units_p, g.units_v);
		}

		{
			GAIA_PROF_SCOPE(apply_gravity);

			for (auto& g: dyn_groups)
				UnitDynamic::apply_gravity(g.units_v, dt);
		}

		{
			GAIA_PROF_SCOPE(calc_alive);

			uint32_t aliveUnits = 0;
			for (auto& g: dyn_groups)
				aliveUnits += UnitDynamic::calc_alive_units(g.units_h);
			gaia::dont_optimize(aliveUnits);
		}

		GAIA_PROF_FRAME();
	}
}

void BM_ECS_DepthOrder_Iter_EnabledOnly(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_DepthOrder_Iter_EnabledOnly);

	ecs::World w;
	(void)w.add<Position>();

	auto root = w.add();
	auto child = w.add();
	w.child(child, root);
	w.add<Position>(child, {0, 0, 0});
	w.copy_n(child, (uint32_t)state.user_data() - 1);

	auto q = w.query().all<Position>().depth_order(ecs::ChildOf);
	gaia::dont_optimize(q.count());

	for (auto _: state) {
		(void)_;
		uint32_t sum = 0;
		q.each([&](ecs::Iter& it) {
			auto ents = it.view<ecs::Entity>();
			GAIA_FOR(it.size()) sum += ents[i].id();
		});
		gaia::dont_optimize(sum);
	}
}

void BM_ECS_DepthOrder_Iter_DisabledOnly(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_DepthOrder_Iter_DisabledOnly);

	ecs::World w;
	(void)w.add<Position>();

	auto root = w.add();
	auto child = w.add();
	w.child(child, root);
	w.add<Position>(child, {0, 0, 0});
	w.enable(child, false);
	w.copy_n(child, (uint32_t)state.user_data() - 1, [&](ecs::Entity e) {
		w.enable(e, false);
	});

	auto q = w.query().all<Position>().depth_order(ecs::ChildOf);
	gaia::dont_optimize(q.count(ecs::Constraints::DisabledOnly));

	for (auto _: state) {
		(void)_;
		uint32_t sum = 0;
		q.each(
				[&](ecs::Iter& it) {
					auto ents = it.view<ecs::Entity>();
					GAIA_FOR(it.size()) sum += ents[i].id();
				},
				ecs::Constraints::DisabledOnly);
		gaia::dont_optimize(sum);
	}
}

void BM_ECS_DepthOrder_Typed_EnabledOnly(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_DepthOrder_Typed_EnabledOnly);

	ecs::World w;
	(void)w.add<Position>();

	auto root = w.add();
	auto child = w.add();
	w.child(child, root);
	w.add<Position>(child, {1, 0, 0});
	w.copy_n(child, (uint32_t)state.user_data() - 1);

	auto q = w.query().all<Position>().depth_order(ecs::ChildOf);
	gaia::dont_optimize(q.count());

	for (auto _: state) {
		(void)_;
		uint32_t sum = 0;
		q.each([&](const Position& p) {
			sum += (uint32_t)p.x;
		});
		gaia::dont_optimize(sum);
	}
}

void BM_ECS_DepthOrder_Typed_PrunedEnabledOnly(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_ECS_DepthOrder_Typed_PrunedEnabledOnly);

	ecs::World w;
	(void)w.add<Position>();

	const auto entityCount = (uint32_t)state.user_data();
	const auto prunedCount = entityCount;
	const auto enabledCount = entityCount;

	auto disabledRoot = w.add();
	w.enable(disabledRoot, false);
	auto prunedChild = w.add();
	w.child(prunedChild, disabledRoot);
	w.add<Position>(prunedChild, {2, 0, 0});
	w.copy_n(prunedChild, prunedCount - 1);

	auto enabledRoot = w.add();
	auto enabledChild = w.add();
	w.child(enabledChild, enabledRoot);
	w.add<Position>(enabledChild, {1, 0, 0});
	w.copy_n(enabledChild, enabledCount - 1);

	auto q = w.query().all<Position>().depth_order(ecs::ChildOf);
	gaia::dont_optimize(q.count());

	for (auto _: state) {
		(void)_;
		uint32_t sum = 0;
		q.each([&](const Position& p) {
			sum += (uint32_t)p.x;
		});
		gaia::dont_optimize(sum);
	}
}

template <uint32_t Groups>
void BM_NonECS_DOD_SoA(picobench::state& state) {
	GAIA_PROF_SCOPE(BM_NonECS_DOD_SoA);

	struct UnitDynamic {
		static void update_pos(cnt::darray_soa<PositionSoA>& p, const cnt::darray_soa<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(update_pos);

			gaia::mem::auto_view_policy_set<PositionSoA> pv{p};
			gaia::mem::auto_view_policy_get<VelocitySoA> vv{v};

			auto ppx = pv.set<0>();
			auto ppy = pv.set<1>();
			auto ppz = pv.set<2>();

			auto vvx = vv.get<0>();
			auto vvy = vv.get<1>();
			auto vvz = vv.get<2>();

			const auto cnt = p.size();
			const auto cdt = dt;
			GAIA_FOR(cnt) ppx[i] += vvx[i] * cdt;
			GAIA_FOR(cnt) ppy[i] += vvy[i] * cdt;
			GAIA_FOR(cnt) ppz[i] += vvz[i] * cdt;

			gaia::dont_optimize(ppx[0]);
			gaia::dont_optimize(ppy[0]);
			gaia::dont_optimize(ppz[0]);
		}

		static void handle_collision(cnt::darray_soa<PositionSoA>& p, cnt::darray_soa<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(handle_collision);

			gaia::mem::auto_view_policy_set<PositionSoA> pv{p};
			gaia::mem::auto_view_policy_set<VelocitySoA> vv{v};

			auto ppy = pv.set<1>();
			auto vvy = vv.set<1>();

			const auto cnt = p.size();
			GAIA_FOR(cnt) {
				if (ppy[i] < 0.0f) {
					ppy[i] = 0.0f;
					vvy[i] = 0.0f;
				}
			}

			gaia::dont_optimize(ppy[0]);
			gaia::dont_optimize(vvy[0]);
		}

		static void apply_gravity(cnt::darray_soa<VelocitySoA>& v) {
			GAIA_PROF_SCOPE(apply_gravity);

			gaia::mem::auto_view_policy_set<VelocitySoA> vv{v};

			auto vvy = vv.set<1>();

			const auto cnt = v.size();
			const auto cdt = dt;
			GAIA_FOR(cnt) {
				vvy[i] += 9.81f * cdt;
			}

			gaia::dont_optimize(vvy[0]);
		}

		static uint32_t calc_alive_units(const cnt::darray<Health>& h) {
			GAIA_PROF_SCOPE(calc_alive_units);

			uint32_t aliveUnits = 0;

			const auto cnt = h.size();
			GAIA_FOR(cnt) {
				if (h[i].value > 0)
					++aliveUnits;
			}
			return aliveUnits;
		}
	};

	const auto N = (uint32_t)state.user_data() / 2;
	const uint32_t NGroup = N / Groups;
	struct static_units_group {
		cnt::darray_soa<PositionSoA> units_p;
		cnt::darray<Rotation> units_r;
		cnt::darray<Scale> units_s;
	} static_groups[Groups];
	struct dyn_units_group {
		cnt::darray_soa<PositionSoA> units_p;
		cnt::darray<Rotation> units_r;
		cnt::darray<Scale> units_s;
		cnt::darray_soa<VelocitySoA> units_v;
		cnt::darray<Direction> units_d;
		cnt::darray<Health> units_h;
		cnt::darray<IsEnemy> units_e;
	} dyn_groups[Groups];

	{
		GAIA_PROF_SCOPE(setup);

		// Create static entities
		for (auto& g: static_groups) {
			g.units_p.resize(NGroup);
			g.units_r.resize(NGroup);
			g.units_s.resize(NGroup);

			GAIA_FOR(NGroup) {
				g.units_p[i] = {0, 100, 0};
				g.units_r[i] = {1, 2, 3, 4};
				g.units_s[i] = {1, 1, 1};
			}
		}

		// Create dynamic entities
		for (auto& g: dyn_groups) {
			g.units_p.resize(NGroup);
			g.units_r.resize(NGroup);
			g.units_s.resize(NGroup);
			g.units_v.resize(NGroup);
			g.units_d.resize(NGroup);
			g.units_h.resize(NGroup);
			g.units_e.resize(NGroup);

			GAIA_FOR(NGroup) {
				g.units_p[i] = {0, 100, 0};
				g.units_r[i] = {1, 2, 3, 4};
				g.units_s[i] = {1, 1, 1};
				g.units_v[i] = {0, 0, 1};
				g.units_d[i] = {0, 0, 1};
				g.units_h[i] = {100, 100};
				g.units_e[i] = {false};
			}
		}
	}

	srand(0);
	for (auto _: state) {
		(void)_;
		dt = CalculateDelta(state);

		for (auto& g: dyn_groups)
			UnitDynamic::update_pos(g.units_p, g.units_v);

		for (auto& g: dyn_groups)
			UnitDynamic::handle_collision(g.units_p, g.units_v);

		for (auto& g: dyn_groups)
			UnitDynamic::apply_gravity(g.units_v);

		{
			GAIA_PROF_SCOPE(calc_alive);

			uint32_t aliveUnits = 0;
			for (auto& g: dyn_groups)
				aliveUnits += UnitDynamic::calc_alive_units(g.units_h);
			gaia::dont_optimize(aliveUnits);
		}

		GAIA_PROF_FRAME();
	}
}

#define PICO_SETTINGS() iterations({1024}).samples(3).user_data(NFew)
#define PICO_SETTINGS_1() iterations({1024}).samples(1).user_data(NFew)
#define PICO_SETTINGS_SANI() iterations({8}).samples(1).user_data(NFew)
#define PICOBENCH_SUITE_REG(name) r.current_suite_name() = name;
#define PICOBENCH_REG(func) (void)r.add_benchmark(#func, func)

int main(int argc, char* argv[]) {
	picobench::runner r(true);
	r.parse_cmd_line(argc, argv);

	// If picobench encounters an unknown command line argument it returns false and sets an error.
	// Ignore this behavior.
	// We only need to make sure to provide the custom arguments after the picobench ones.
	if (r.error() == picobench::error_unknown_cmd_line_argument)
		r.set_error(picobench::no_error);

	// With profiling mode enabled we want to be able to pick what benchmark to run so it is easier
	// for us to isolate the results.
	{
		bool profilingMode = false;
		bool sanitizerMode = false;
		bool dodMode = false;

		const gaia::cnt::darray<std::string_view> args(argv + 1, argv + argc);
		for (const auto& arg: args) {
			if (arg == "-p") {
				profilingMode = true;
				continue;
			}
			if (arg == "-s") {
				sanitizerMode = true;
				continue;
			}
			if (arg == "-dod") {
				dodMode = true;
				continue;
			}
		}

		GAIA_LOG_N("Profiling mode = %s", profilingMode ? "ON" : "OFF");
		GAIA_LOG_N("Sanitizer mode = %s", sanitizerMode ? "ON" : "OFF");
		GAIA_LOG_N("DOD mode       = %s", dodMode ? "ON" : "OFF");

		if (profilingMode) {
			if (dodMode) {
				// PICOBENCH_SUITE_REG("NonECS_DOD");
				PICOBENCH_REG(BM_NonECS_DOD<80>).PICO_SETTINGS_1().label("DOD_Chunks_80");
			} else {
				// PICOBENCH_SUITE_REG("ECS");
				PICOBENCH_REG(BM_ECS_Iter).PICO_SETTINGS_1().label("Iter");
			}
			r.run_benchmarks();
			return 0;
		}

		if (sanitizerMode) {
			PICOBENCH_REG(BM_ECS).PICO_SETTINGS().baseline().label("Default");
			PICOBENCH_REG(BM_ECS_Iter).PICO_SETTINGS().label("Iter");
			PICOBENCH_REG(BM_ECS_Iter_SoA).PICO_SETTINGS().label("Iter_SoA");
			r.run_benchmarks();
			return 0;
		}

		{
			// PICOBENCH_SUITE_REG("OOP");
			// // Ordinary coding style.
			// PICOBENCH_REG(BM_NonECS<false>).PICO_SETTINGS().label("Default");
			// PICOBENCH_REG(BM_NonECS<true>).PICO_SETTINGS().label("Default2");

			// // Ordinary coding style with optimized memory layout (imagine using custom allocators
			// // to keep things close and tidy in memory).
			// PICOBENCH_REG(BM_NonECS_BetterMemoryLayout<false>).PICO_SETTINGS().label("OptimizedMemLayout");
			// PICOBENCH_REG(BM_NonECS_BetterMemoryLayout<true>).PICO_SETTINGS().label("OptimizedMemLayout2");

			// Memory organized in DoD style.
			// Performance target BM_ECS_Iter.
			// "Groups" is there to simulate having items split into separate chunks similar to what ECS does.
			PICOBENCH_SUITE_REG("DOD");
			PICOBENCH_REG(BM_NonECS_DOD<1>).PICO_SETTINGS().baseline().label("Default");
			PICOBENCH_REG(BM_NonECS_DOD<20>).PICO_SETTINGS().label("Chunks_20");
			PICOBENCH_REG(BM_NonECS_DOD<40>).PICO_SETTINGS().label("Chunks_40");
			PICOBENCH_REG(BM_NonECS_DOD<80>).PICO_SETTINGS().label("Chunks_80");
			PICOBENCH_REG(BM_NonECS_DOD<160>).PICO_SETTINGS().label("Chunks_160");
			PICOBENCH_REG(BM_NonECS_DOD<200>).PICO_SETTINGS().label("Chunks_200");
			PICOBENCH_REG(BM_NonECS_DOD<320>).PICO_SETTINGS().label("Chunks_320");
			PICOBENCH_REG(BM_NonECS_DOD<320>).PICO_SETTINGS().user_data(NMany).label("Chunks_320 Many");
			PICOBENCH_REG(BM_NonECS_DOD_ReadPositionVelocityOnly)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("ReadPositionVelocityOnly Many");
			PICOBENCH_REG(BM_NonECS_DOD_UpdatePositionFixedDeltaWithReadback)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("UpdatePositionFixedDeltaWithReadback Many");
			PICOBENCH_REG(BM_NonECS_DOD_ReadPositionVelocityChunked<320>)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("ReadPositionVelocityChunks_320 Many");
			PICOBENCH_REG(BM_NonECS_DOD_UpdatePositionFixedDeltaWithReadbackChunked<320>)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("UpdatePositionFixedDeltaWithReadbackChunks_320 Many");

			// Best possible performance with no manual SIMD optimization.
			// Performance target for BM_ECS_Iter_SoA.
			// "Groups" is there to simulate having items split into separate chunks similar to what ECS does.
			PICOBENCH_SUITE_REG("DOD_SoA");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<1>).PICO_SETTINGS().baseline().label("Default");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<20>).PICO_SETTINGS().label("Chunks_20");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<40>).PICO_SETTINGS().label("Chunks_40");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<80>).PICO_SETTINGS().label("Chunks_80");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<160>).PICO_SETTINGS().label("Chunks_160");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<200>).PICO_SETTINGS().label("Chunks_200");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<320>).PICO_SETTINGS().label("Chunks_320");
			PICOBENCH_REG(BM_NonECS_DOD_SoA<320>).PICO_SETTINGS().user_data(NMany).label("Chunks_320 Many");

			// GaiaECS performance.
			PICOBENCH_SUITE_REG("ECS");
			PICOBENCH_REG(BM_ECS).PICO_SETTINGS().baseline().label("Default");
			PICOBENCH_REG(BM_ECS).PICO_SETTINGS().user_data(NMany).label("Default Many");
			PICOBENCH_REG(BM_ECS_ReadPositionOnly).PICO_SETTINGS().user_data(NMany).label("ReadPositionOnly Many");
			PICOBENCH_REG(BM_ECS_UpdatePositionOnly).PICO_SETTINGS().user_data(NMany).label("UpdatePositionOnly Many");
			PICOBENCH_REG(BM_ECS_ReadPositionVelocityOnly)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("ReadPositionVelocityOnly Many");
			PICOBENCH_REG(BM_ECS_TouchPositionWithVelocity)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("TouchPositionWithVelocity Many");
			PICOBENCH_REG(BM_ECS_UpdatePositionFixedDeltaWithReadback)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("UpdatePositionFixedDeltaWithReadback Many");
			PICOBENCH_REG(BM_ECS_ReadPositionVelocityChunkRaw)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("ReadPositionVelocityChunkRaw Many");
			PICOBENCH_REG(BM_ECS_TouchPositionWithVelocityChunkRaw)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("TouchPositionWithVelocityChunkRaw Many");
			PICOBENCH_REG(BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkRaw)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("UpdatePositionFixedDeltaWithReadbackChunkRaw Many");
			PICOBENCH_REG(BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkRawSilent)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("UpdatePositionFixedDeltaWithReadbackChunkRawSilent Many");
			PICOBENCH_REG(BM_ECS_ReadPositionVelocityChunkPtrRaw)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("ReadPositionVelocityChunkPtrRaw Many");
			PICOBENCH_REG(BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawSilent)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("UpdatePositionFixedDeltaWithReadbackChunkPtrRawSilent Many");
			PICOBENCH_REG(BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawLocalSumSilent)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("UpdatePositionFixedDeltaWithReadbackChunkPtrRawLocalSumSilent Many");
			PICOBENCH_REG(BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawRestrictLocalSumSilent)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("UpdatePositionFixedDeltaWithReadbackChunkPtrRawRestrictLocalSumSilent Many");
			PICOBENCH_REG(BM_ECS_UpdatePositionFixedDeltaWithReadbackChunkPtrRawRestrictLocalSum)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("UpdatePositionFixedDeltaWithReadbackChunkPtrRawRestrictLocalSum Many");
			PICOBENCH_REG(BM_ECS_UpdatePositionFixedDeltaWithReadbackIter)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("UpdatePositionFixedDeltaWithReadbackIter Many");
			PICOBENCH_REG(BM_ECS_UpdatePositionFixedDeltaWithReadbackIterLocalAccum)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("UpdatePositionFixedDeltaWithReadbackIterLocalAccum Many");
			PICOBENCH_REG(BM_ECS_UpdatePositionFixedDeltaWithReadbackIterLocalSum)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("UpdatePositionFixedDeltaWithReadbackIterLocalSum Many");
			PICOBENCH_REG(BM_ECS_ReadVelocityOnly).PICO_SETTINGS().user_data(NMany).label("ReadVelocityOnly Many");
			PICOBENCH_REG(BM_ECS_WriteVelocityOnly).PICO_SETTINGS().user_data(NMany).label("WriteVelocityOnly Many");
			PICOBENCH_REG(BM_ECS_Iter).PICO_SETTINGS().label("Iter");
			PICOBENCH_REG(BM_ECS_Iter).PICO_SETTINGS().user_data(NMany).label("Iter Many");
			PICOBENCH_REG(BM_ECS_Iter_Dir).PICO_SETTINGS().label("Iter_Dir");
			PICOBENCH_REG(BM_ECS_Iter_Dir).PICO_SETTINGS().user_data(NMany).label("Iter_Dir Many");
			PICOBENCH_REG(BM_ECS_Iter_SoA).PICO_SETTINGS().label("Iter_SoA");
			PICOBENCH_REG(BM_ECS_Iter_SoA).PICO_SETTINGS().user_data(NMany).label("Iter_SoA Many");
			PICOBENCH_REG(BM_ECS_Iter_SoA_Dir).PICO_SETTINGS().label("Iter_SoA_Dir");
			PICOBENCH_REG(BM_ECS_Iter_SoA_Dir).PICO_SETTINGS().user_data(NMany).label("Iter_SoA_Dir Many");
			PICOBENCH_REG(BM_ECS_DepthOrder_Iter_EnabledOnly)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("DepthOrder_Iter_EnabledOnly Many");
			PICOBENCH_REG(BM_ECS_DepthOrder_Iter_DisabledOnly)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("DepthOrder_Iter_DisabledOnly Many");
			PICOBENCH_REG(BM_ECS_DepthOrder_Typed_EnabledOnly)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("DepthOrder_Typed_EnabledOnly Many");
			PICOBENCH_REG(BM_ECS_DepthOrder_Typed_PrunedEnabledOnly)
					.PICO_SETTINGS()
					.user_data(NMany)
					.label("DepthOrder_Typed_PrunedEnabledOnly Many");
		}
	}

	return r.run(0);
}

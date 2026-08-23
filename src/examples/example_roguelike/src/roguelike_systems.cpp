//! \file
//! \brief Gaia systems for the roguelike example.
//!
//! Input is read outside ECS. `World::update` runs these systems.
//! `w.add(first, {DependsOn, second})` means first runs before second.

#include "roguelike_game.h"
#include "roguelike_render.h"

#include <cstdio>

using namespace gaia;

void register_systems(ecs::World& world, Game& game) {
	const auto phaseInput = world.add();
	const auto phaseSim = world.add();
	const auto phaseView = world.add();
	world.name(phaseInput, "Input");
	world.name(phaseSim, "Simulation");
	world.name(phaseView, "View");
	world.child(phaseInput, game.modPhases);
	world.child(phaseSim, game.modPhases);
	world.child(phaseView, game.modPhases);

	// Child-before-target: A DependsOn B means A runs before B.
	world.add(phaseInput, {ecs::DependsOn, phaseSim});
	world.add(phaseSim, {ecs::DependsOn, phaseView});

	auto sysInput = world.system()
											.name("InputSystem")
											.phase(phaseInput)
											.ctx(&game)
											.all<Velocity&>()
											.all<Player>()
											.on_each([](ecs::Iter& it) {
												auto& gw = game_of(it);
												auto vel = it.view_mut<Velocity>();

												GAIA_EACH(it) {
													const char key = gw.pendingKey;
													gw.playerActed = false;
													gw.pendingShot = false;
													vel[i] = {0, 0};

													if (key == KEY_QUIT) {
														gw.terminate = true;
														return;
													}
													if (gw.escaped)
														return;
													if (key == KEY_UP) {
														vel[i] = {0, -1};
														gw.playerActed = true;
													} else if (key == KEY_DOWN) {
														vel[i] = {0, 1};
														gw.playerActed = true;
													} else if (key == KEY_LEFT) {
														vel[i] = {-1, 0};
														gw.playerActed = true;
													} else if (key == KEY_RIGHT) {
														vel[i] = {1, 0};
														gw.playerActed = true;
													} else if (key == KEY_SHOOT) {
														gw.playerActed = true;
													} else if (key == KEY_WAIT) {
														gw.playerActed = true;
													}
												}
											});

	auto sysAi = world.system()
									 .name("EnemyAISystem")
									 .phase(phaseInput)
									 .ctx(&game)
									 .all<Position>()
									 .all<Velocity&>()
									 .is(game.prefabEnemy)
									 .on_each([](ecs::Iter& it) {
										 auto& gw = game_of(it);
										 auto pos = it.view<Position>();
										 auto vel = it.view_mut<Velocity>();
										 GAIA_EACH(it) {
											 gw.ChasePlayer(pos[i], vel[i]);
										 }
									 });
	world.add(sysInput.entity(), {ecs::DependsOn, sysAi.entity()});

	auto sysReset =
			world.system().name("ResetFrameSystem").phase(phaseInput).ctx(&game).all<Turn>().on_each([](ecs::Iter& it) {
				auto& gw = game_of(it);
				gw.occupancy.Clear();
				gw.colliding.clear();
			});
	world.add(sysAi.entity(), {ecs::DependsOn, sysReset.entity()});

	auto sysMap = world.system()
										.name("UpdateMapSystem")
										.phase(phaseInput)
										.ctx(&game)
										.all<Position>()
										.all<RigidBody>()
										.on_each([](ecs::Iter& it) {
											auto& gw = game_of(it);
											auto ve = it.view<ecs::Entity>();
											auto vp = it.view<Position>();
											GAIA_EACH(it) {
												if (!gw.dungeon.InBounds(vp[i].x, vp[i].y))
													continue;
												gw.occupancy.Add(vp[i].x, vp[i].y, ve[i]);
											}
										});
	world.add(sysReset.entity(), {ecs::DependsOn, sysMap.entity()});

	// Typed callback: no GameWorld access, only the components in the query.
	auto sysOri = world.system()
										.name("OrientationSystem")
										.phase(phaseSim)
										.all<Orientation&>()
										.all<Velocity>()
										.on_each([](Orientation& o, const Velocity& v) {
											if (v.x != 0) {
												o.x = v.x > 0 ? 1 : -1;
												o.y = 0;
											}
											if (v.y != 0) {
												o.x = 0;
												o.y = v.y > 0 ? 1 : -1;
											}
										});

	auto sysCol = world.system()
										.name("CollisionSystem")
										.phase(phaseSim)
										.ctx(&game)
										.all<Turn>()
										.writes<Velocity>()
										.on_each([](ecs::Iter& it) {
											game_of(it).ResolveMovement();
										});
	world.add(sysOri.entity(), {ecs::DependsOn, sysCol.entity()});

	auto sysMove = world.system()
										 .name("MoveSystem")
										 .phase(phaseSim)
										 .all<Position&>()
										 .all<Velocity>()
										 .on_each([](Position& p, const Velocity& v) {
											 p.x += v.x;
											 p.y += v.y;
										 });
	world.add(sysCol.entity(), {ecs::DependsOn, sysMove.entity()});

	auto sysStairs =
			world.system().name("StairsSystem").phase(phaseSim).ctx(&game).all<Turn>().on_each([](ecs::Iter& it) {
				auto& gw = game_of(it);
				if (gw.playerActed)
					gw.TryDescend();
			});
	world.add(sysMove.entity(), {ecs::DependsOn, sysStairs.entity()});

	auto sysDmg = world.system()
										.name("HandleDamageSystem")
										.phase(phaseSim)
										.ctx(&game)
										.all<Turn>()
										.writes<Health>()
										.on_each([](ecs::Iter& it) {
											auto& gw = game_of(it);
											const auto& colliding = gw.colliding;
											GAIA_EACH(colliding) {
												const auto& coll = colliding[i];
												if (coll.e2 == ecs::EntityBad)
													continue;

												auto atk = coll.e1;
												auto def = coll.e2;
												if (gw.world.is(def, gw.prefabArrow) && !gw.world.is(atk, gw.prefabArrow)) {
													atk = coll.e2;
													def = coll.e1;
												}
												if (gw.world.is(atk, gw.prefabArrow) && gw.world.has<Player>(def))
													continue;
												if (gw.world.is(atk, gw.prefabEnemy) && gw.world.is(def, gw.prefabEnemy))
													continue;

												uint32_t idxAtk{}, idxDef{};
												auto* pAtk = gw.world.get_chunk(atk, idxAtk);
												auto* pDef = gw.world.get_chunk(def, idxDef);
												GAIA_ASSERT(pAtk != nullptr);
												GAIA_ASSERT(pDef != nullptr);

												const bool attackerIsArrow = gw.world.is(atk, gw.prefabArrow);
												if (attackerIsArrow && pAtk->has<Health>() && pAtk->view<Health>()[idxAtk].value <= 0)
													continue;
												if (!pDef->has<Health>() || !pAtk->has<BattleStats>())
													continue;

												int armor = 0;
												if (pDef->has<BattleStats>())
													armor = pDef->view<BattleStats>()[idxDef].armor;
												int damage = pAtk->view<BattleStats>()[idxAtk].power - armor;
												if (gw.world.is(atk, gw.prefabGoblin)) {
													const auto pack = gw.world.find_prefab_instance(atk, gw.prefabPack);
													if (pack != ecs::EntityBad && gw.world.has<BattleStats>(pack))
														damage += gw.world.get<BattleStats>(pack).power;
												}
												if (damage < 0)
													continue;

												pDef->view_mut<Health>()[idxDef].value -= damage;
												if (attackerIsArrow && pAtk->has<Health>())
													pAtk->view_mut<Health>()[idxAtk].value = 0;

												const char atkG = pAtk->has<Sprite>() ? pAtk->view<Sprite>()[idxAtk].value : '?';
												const char defG = pDef->has<Sprite>() ? pDef->view<Sprite>()[idxDef].value : '?';
												gw.log_msg("%c hits %c for %d.", atkG, defG, damage);
											}
										});
	world.add(sysCol.entity(), {ecs::DependsOn, sysDmg.entity()});

	auto sysItem = world.system()
										 .name("HandleItemHitSystem")
										 .phase(phaseSim)
										 .ctx(&game)
										 .all<Turn>()
										 .writes<Health>()
										 .on_each([](ecs::Iter& it) {
											 auto& gw = game_of(it);
											 auto& cmd = it.cmd_buffer_st();
											 const auto& colliding = gw.colliding;
											 GAIA_EACH(colliding) {
												 const auto& coll = colliding[i];
												 if (coll.e2 == ecs::EntityBad) {
													 if (gw.world.is(coll.e1, gw.prefabArrow) && gw.world.has<Health>(coll.e1)) {
														 auto h = gw.world.get<Health>(coll.e1);
														 h.value = 0;
														 gw.world.set<Health>(coll.e1) = h;
													 }
													 continue;
												 }

												 uint32_t idx1{}, idx2{};
												 auto* pChunk1 = gw.world.get_chunk(coll.e1, idx1);
												 auto* pChunk2 = gw.world.get_chunk(coll.e2, idx2);
												 GAIA_ASSERT(pChunk1 != nullptr);
												 GAIA_ASSERT(pChunk2 != nullptr);

												 if (pChunk1->has<Player>() && pChunk1->has<Health>() && pChunk2->has<Item>() &&
														 pChunk2->has<BattleStats>()) {
													 auto item2 = pChunk2->view<Item>();
													 auto stats2 = pChunk2->view<BattleStats>();
													 if (item2[idx2].type == ItemType::Gold) {
														 gw.score += (uint32_t)stats2[idx2].power;
														 gw.log_msg("Picked up %d gold.", stats2[idx2].power);
													 } else if (item2[idx2].type == ItemType::Potion) {
														 pChunk1->view_mut<Health>()[idx1].value += stats2[idx2].power;
														 gw.log_msg("The potion mends you (%+d).", stats2[idx2].power);
													 } else if (item2[idx2].type == ItemType::Poison) {
														 pChunk1->view_mut<Health>()[idx1].value += stats2[idx2].power;
														 gw.log_msg("The flask burns (%+d).", stats2[idx2].power);
													 } else {
														 continue;
													 }
													 cmd.del(coll.e2);
												 }

												 if (gw.world.is(coll.e1, gw.prefabArrow) && pChunk1->has<Health>())
													 pChunk1->view_mut<Health>()[idx1].value = 0;
												 if (gw.world.is(coll.e2, gw.prefabArrow) && pChunk2->has<Health>())
													 pChunk2->view_mut<Health>()[idx2].value = 0;
											 }
										 });
	world.add(sysCol.entity(), {ecs::DependsOn, sysItem.entity()});

	auto sysHp =
			world.system().name("HandleHealthSystem").phase(phaseSim).all<Health&>().changed<Health>().on_each([](Health& h) {
				if (h.value > h.valueMax)
					h.value = h.valueMax;
			});
	world.add(sysDmg.entity(), {ecs::DependsOn, sysHp.entity()});
	world.add(sysItem.entity(), {ecs::DependsOn, sysHp.entity()});

	auto sysDeath = world.system()
											.name("HandleDeathSystem")
											.phase(phaseSim)
											.ctx(&game)
											.all<Health>()
											.all<Position>()
											.changed<Health>()
											.on_each([](ecs::Iter& it) {
												auto& gw = game_of(it);
												auto& cmd = it.cmd_buffer_st();
												auto ve = it.view<ecs::Entity>();
												auto vh = it.view<Health>();

												GAIA_EACH(it) {
													if (vh[i].value > 0)
														continue;

													const auto e = ve[i];
													if (gw.world.has<Player>(e)) {
														gw.playerAlive = false;
														gw.log_msg("You collapse.");
													} else if (gw.world.is(e, gw.prefabEnemy)) {
														gw.score += 10;
														gw.log_msg("A foe falls. +10.");
													}

													cmd.del(e);
												}
											});
	world.add(sysHp.entity(), {ecs::DependsOn, sysDeath.entity()});
	world.add(sysStairs.entity(), {ecs::DependsOn, sysDeath.entity()});

	auto sysClear =
			world.system().name("ClearMapSystem").phase(phaseView).ctx(&game).all<Turn>().on_each([](ecs::Iter& it) {
				auto& gw = game_of(it);
				view_begin_frame(gw);
				view_copy_terrain(gw);
			});

	auto sysSpr = world.system()
										.name("WriteSpritesToMapSystem")
										.phase(phaseView)
										.ctx(&game)
										.all<Position>()
										.all<Sprite>()
										.on_each([](ecs::Iter& it) {
											auto& gw = game_of(it);
											auto vp = it.view<Position>();
											auto vs = it.view<Sprite>();
											GAIA_EACH(it) {
												view_stamp(gw, vp[i].x, vp[i].y, vs[i].value);
											}
										});
	world.add(sysClear.entity(), {ecs::DependsOn, sysSpr.entity()});

	auto sysFov = world.system().name("FovSystem").phase(phaseView).ctx(&game).all<Turn>().on_each([](ecs::Iter& it) {
		view_recompute_fov(game_of(it));
	});
	world.add(sysSpr.entity(), {ecs::DependsOn, sysFov.entity()});

	auto sysRen = world.system().name("RenderSystem").phase(phaseView).ctx(&game).all<Turn>().on_each([](ecs::Iter& it) {
		view_draw_map(game_of(it));
	});
	world.add(sysFov.entity(), {ecs::DependsOn, sysRen.entity()});

	auto sysUiP = world.system()
										.name("UISystemPlayer")
										.phase(phaseView)
										.ctx(&game)
										.all<Health>()
										.all<Player>()
										.on_each([](ecs::Iter& it) {
											auto& gw = game_of(it);
											gw.RecountCensus();
											auto vh = it.view<Health>();
											GAIA_EACH(it) {
												printf(
														"Floor %u/%u   HP %d/%d   Gold %u   Foes %u   On floor %u\n", gw.floor, MaxFloors,
														vh[i].value, vh[i].valueMax, gw.score, gw.enemyCount, gw.floorBodies);
											}
										});
	world.add(sysRen.entity(), {ecs::DependsOn, sysUiP.entity()});

	// Semantic `is` query terms recognize instances created with World::instantiate.
	auto sysUiG = world.system()
										.name("UISystemGoblin")
										.phase(phaseView)
										.ctx(&game)
										.all<Health>()
										.all<Position>()
										.is(game.prefabGoblin)
										.on_each([](ecs::Iter& it) {
											auto& gw = game_of(it);
											auto ve = it.view<ecs::Entity>();
											auto vh = it.view<Health>();
											auto vp = it.view<Position>();
											GAIA_EACH(it) {
												if (!gw.dungeon.InBounds(vp[i].x, vp[i].y) || !gw.visible[vp[i].y][vp[i].x])
													continue;
												const auto pack = gw.world.find_prefab_instance(ve[i], gw.prefabPack);
												if (pack != ecs::EntityBad && gw.world.has<BattleStats>(pack))
													printf(
															"  g %u:%u  %d/%d  pack +%d\n", ve[i].id(), ve[i].gen(), vh[i].value, vh[i].valueMax,
															gw.world.get<BattleStats>(pack).power);
												else
													printf("  g %u:%u  %d/%d\n", ve[i].id(), ve[i].gen(), vh[i].value, vh[i].valueMax);
											}
										});
	world.add(sysRen.entity(), {ecs::DependsOn, sysUiG.entity()});

	auto sysUiO = world.system()
										.name("UISystemOrc")
										.phase(phaseView)
										.ctx(&game)
										.all<Health>()
										.all<Position>()
										.is(game.prefabOrc)
										.on_each([](ecs::Iter& it) {
											auto& gw = game_of(it);
											auto ve = it.view<ecs::Entity>();
											auto vh = it.view<Health>();
											auto vp = it.view<Position>();
											GAIA_EACH(it) {
												if (!gw.dungeon.InBounds(vp[i].x, vp[i].y) || !gw.visible[vp[i].y][vp[i].x])
													continue;
												printf("  O %u:%u  %d/%d\n", ve[i].id(), ve[i].gen(), vh[i].value, vh[i].valueMax);
											}
										});
	world.add(sysRen.entity(), {ecs::DependsOn, sysUiO.entity()});

	auto sysState =
			world.system().name("GameStateSystem").phase(phaseView).ctx(&game).all<Turn>().on_each([](ecs::Iter& it) {
				auto& gw = game_of(it);
				if (gw.enemyCount == 0 && !gw.floorCleared && gw.playerAlive && !gw.escaped) {
					gw.floorCleared = true;
					gw.log_msg("The floor is quiet. Find %c.", TILE_STAIRS);
				}

				GAIA_EACH(gw.log) {
					printf("  %s\n", gw.log[i].text);
				}
				printf("Move WASD  Wait space  Shoot Q  Quit P\n");

				if (!gw.playerAlive) {
					printf("You are dead. Score %u.\n", gw.score);
					gw.terminate = true;
					return;
				}
				if (gw.escaped) {
					printf("You escaped the dungeon. Score %u.\n", gw.score);
					gw.terminate = true;
				}
			});
	world.add(sysUiP.entity(), {ecs::DependsOn, sysState.entity()});
	world.add(sysUiG.entity(), {ecs::DependsOn, sysState.entity()});
	world.add(sysUiO.entity(), {ecs::DependsOn, sysState.entity()});
}

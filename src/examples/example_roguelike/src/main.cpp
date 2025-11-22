#ifdef _WIN32
	#include <conio.h>
#else
	#include <fcntl.h>
	#include <termios.h>
	#include <unistd.h>
#endif
#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <queue>
#include <unordered_set>
#include <vector>

#include <gaia.h>

using namespace gaia;

#define USE_PATHFINDING 0

//----------------------------------------------------------------------
// Platform-specific helpers
//----------------------------------------------------------------------

#ifndef _WIN32
void enable_raw_mode(termios* old) {
	if (tcgetattr(0, old) < 0)
		perror("tcgetattr");

	termios raw = *old;
	raw.c_lflag &= ~(ICANON | ECHO);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(0, TCSANOW, &raw) < 0)
		perror("tcsetattr raw");
}

void disable_raw_mode(termios* old) {
	if (tcsetattr(0, TCSADRAIN, old) < 0)
		perror("tcsetattr restore");
}

char get_char() {
	char buf[2] = {};
	termios old;
	enable_raw_mode(&old);

	const auto n = read(0, buf, sizeof(buf) - 1);
	if (n < 0)
		perror("read");

	disable_raw_mode(&old);

	return buf[0];
}

void clear_screen() {
	auto res = system("clear");
	(void)res;
}
#else
char get_char() {
	return (char)_getch();
}
void clear_screen() {
	system("cls");
}
#endif

//----------------------------------------------------------------------
// Components
//----------------------------------------------------------------------

//! Coordinates in world-space
struct Position {
	int x, y;
};
bool operator==(const Position& a, const Position& b) {
	return a.x == b.x && a.y == b.y;
}
namespace std {
	template <>
	struct hash<Position> {
		size_t operator()(const Position& p) const noexcept {
			const uint64_t h1 = core::calculate_hash64(p.x);
			const uint64_t h2 = core::calculate_hash64(p.y);
			return core::hash_combine(h1, h2);
		}
	};
} // namespace std

//! Direction in which an object faces
struct Orientation {
	int x, y;
};
//! Velocity
struct Velocity {
	int x, y;
};
//! Sprite used for rendering an object
struct Sprite {
	char value;
};
//! How much health does an object have
struct Health {
	int value;
	int valueMax;
};
//! Various stats related to battle
struct BattleStats {
	int power;
	int armor;
};
enum class ItemType : uint8_t { Poison, Potion, Arrow };
//! Precise identification of items
struct Item {
	ItemType type;
};
//! Tag representing the player
struct Player {};
//! Tag representing physical objects that can interact with one-another
struct RigidBody {};

//----------------------------------------------------------------------

constexpr uint32_t ScreenX = 40;
constexpr uint32_t ScreenY = 15;

constexpr char KEY_LEFT = 'a';
constexpr char KEY_RIGHT = 'd';
constexpr char KEY_UP = 'w';
constexpr char KEY_DOWN = 's';
constexpr char KEY_SHOOT = 'q';
constexpr char KEY_QUIT = 'p';

constexpr char TILE_WALL = '#';
constexpr char TILE_FREE = ' ';
constexpr char TILE_PLAYER = 'P';
constexpr char TILE_ENEMY_GOBLIN = 'G';
constexpr char TILE_ENEMY_ORC = 'O';
constexpr char TILE_ARROW = '.';
constexpr char TILE_POTION = 'h';
constexpr char TILE_POISON = 'x';
#if USE_PATHFINDING
constexpr char TILE_PATH = 'W';
#endif

//----------------------------------------------------------------------
// Path-finding
//----------------------------------------------------------------------

#if USE_PATHFINDING

template <class T, class Container, class Compare>
class PriorityQueue: public std::priority_queue<T, Container, Compare> {
public:
	using const_iterator = typename std::priority_queue<T, Container, Compare>::container_type::const_iterator;

	template <typename Func>
	bool has_if(Func func) const {
		auto it = this->c.cbegin();
		auto last = this->c.cend();
		while (it != last) {
			if (func(*it))
				break;
			++it;
		}
		return it != this->c.cend();
	}
};

class AStar {
	using MyPriorityPair = std::pair<float, uint32_t>; // f cost / ID

	class MyPriorityCompare {
	public:
		bool operator()(MyPriorityPair a, MyPriorityPair b) const {
			return a.first > b.first;
		}
	};

	using MyPriorityQueue = PriorityQueue<MyPriorityPair, std::vector<MyPriorityPair>, MyPriorityCompare>;

	static constexpr uint32_t MAX_NEIGHBORS = 4; // each node can have up to 4 neighbors

public:
	struct Node {
	private:
		//! ID of the node
		uint32_t id;
		//! Bitmask of neighbors in clockwise direction.
		//! 0 - north, 1 - east, 2 - south, 3 - west
		uint32_t neighbors : 4;
		//! Cost. 2 bits per direction. With 4 directions 8 bits are necessary.
		//! 0 - blocked
		//! 1 - small cost
		//! 2 - medium cost
		//! 3 - big cost
		uint32_t edge_costs : 8;

	public:
		Node() {
			id = 0;
			neighbors = 0;
			edge_cost = 0;
		}
		Node(uint32_t value): id(value) {}

		void InitIndex(uint32_t index, uint32_t cost) {
			SetNeighbor(index, cost != 0);
			SetEdgeCost(index, cost);
		}

		uint32_t comp_id() const {
			return id;
		}
		bool HasNeighbor(uint32_t index) const {
			return (((uint32_t)neighbors >> index) & 1U) != 0U;
		}
		uint32_t GetEdgeCost(uint32_t index) const {
			return ((uint32_t)edge_costs >> (index * 2)) & 3U;
		}

	private:
		void SetNeighbor(uint32_t index, bool value) {
			const uint32_t mask = 1U << index;
			neighbors = (neighbors & ~mask) | ((uint32_t)value << index);
		}
		void SetEdgeCost(uint32_t index, uint32_t cost) {
			const uint32_t mask = (3U << (index * 2));
			edge_costs = (edge_costs & ~mask) | ((cost & 3U) << (index * 2));
		}
	};

	static constexpr uint32_t NodeIdToX(uint32_t id) {
		return id % ScreenX;
	}
	static constexpr uint32_t NodeIdToY(uint32_t id) {
		return id / ScreenX;
	}
	static constexpr uint32_t NodeIdFromXY(uint32_t x, uint32_t y) {
		return y * ScreenX + x;
	}

	// Define a function to estimate the cost from a node to the end node (using Euclidean distance)
	float HeuristicCostEstimate(const Node& current, const Node& goal) const {
		const uint32_t cid = current.comp_id();
		const uint32_t gid = goal.comp_id();
		const uint32_t dx = NodeIdToX(cid) - NodeIdToX(gid);
		const uint32_t dy = NodeIdToY(cid) - NodeIdToY(gid);
		const uint32_t dxx = dx * dx;
		const uint32_t dyy = dy * dy;
		return sqrtf((float)dxx + (float)dyy); // Euclidean distance
	}

	bool OpenSetContains(const MyPriorityQueue& open_set, uint32_t index) const {
		static_assert(sizeof(MyPriorityPair) <= 8); // So long it's small we can pass by value
		return open_set.has_if([index](MyPriorityPair elem) {
			return elem.second == index;
		}); // true if the node is in the open set, false otherwise
	}

	std::vector<uint32_t> FindPath(const std::vector<Node>& graph, uint32_t start_id, uint32_t goal_id) {
		cnt::map<uint32_t, uint32_t> parents; // key: ID, data: parentID
		cnt::map<uint32_t, std::pair<float, float>> scores; // key: ID, data: <g, f>
		std::unordered_set<uint32_t> closed_set;
		MyPriorityQueue open_set;

		// Define the start and goal nodes
		const Node& start_node = graph[start_id];
		const Node& goal_node = graph[goal_id];

		// Add the start node to the open set
		open_set.push(std::make_pair(0, start_id));

		// Initialize the costs
		scores[start_id] = {0, HeuristicCostEstimate(start_node, goal_node)};

		// Run the A* algorithm
		while (!open_set.empty()) {
			// Get the node with the lowest f score from the open set
			const uint32_t current_id = open_set.top().second;
			open_set.pop();

			// Check if we've reached the goal node
			if (current_id == goal_id) {
				std::vector<uint32_t> path;

				// Reconstruct the path
				uint32_t node_id = goal_id;
				while (node_id != start_id) {
					path.push_back(node_id);
					node_id = parents[node_id];
				}
				path.push_back(start_id);
				reverse(path.begin(), path.end());
				return path;
			}

			// Mark the current node as closed
			closed_set.emplace(current_id);

			constexpr int neighborOffsets[MAX_NEIGHBORS] = {-(int)ScreenX, 1, ScreenX, -1};

			// Iterate over the neighbors of the current node
			const Node& current_node = graph[current_id];
			GAIA_FOR(MAX_NEIGHBORS) {
				// We need to have a neighbor
				if (!current_node.HasNeighbor(i))
					continue;

				// The neighbor needs to be accessible
				const auto neighborCost = (float)current_node.GetEdgeCost(i);
				if (neighborCost <= 0.F)
					continue;

				// Skip this neighbor if it's already closed
				const uint32_t neighbor_id = current_id + neighborOffsets[i]; // relative indexing
				if (closed_set.find(neighbor_id) != closed_set.end())
					continue;

				const float tentative_g_score = scores[current_id].first + neighborCost;
				if (tentative_g_score < scores[neighbor_id].first) {
					// This is a better path to the neighbor node
					const float f = tentative_g_score + HeuristicCostEstimate(graph[neighbor_id], goal_node);
					parents[neighbor_id] = current_id;
					scores[neighbor_id] = {tentative_g_score, f};
					if (!OpenSetContains(open_set, neighbor_id))
						open_set.push(std::make_pair(f, neighbor_id));
				} else if (!OpenSetContains(open_set, neighbor_id)) {
					// Unchecked field, try it
					const float f = tentative_g_score + HeuristicCostEstimate(graph[neighbor_id], goal_node);
					parents[neighbor_id] = current_id;
					scores[neighbor_id] = {tentative_g_score, f};
					open_set.push(std::make_pair(f, neighbor_id));
				}
			}
		}

		// If we get here, there's no path from start to goal
		return {};
	}
};

#endif

//----------------------------------------------------------------------
// Systems data
//----------------------------------------------------------------------

struct CollisionData {
	//! Entity coll
	ecs::Entity e1;
	//! Entity being collided with
	ecs::Entity e2;
	//! Position of collision
	Position p;
	//! Velocity at the time of collision
	Velocity v;
};

//----------------------------------------------------------------------
// Game world
//----------------------------------------------------------------------

struct GameWorld {
	ecs::World& w;
	//! map tiles
	char map[ScreenY][ScreenX]{};
	//! tile content
	cnt::map<Position, cnt::darray<ecs::Entity>> content;
	//! collision data
	cnt::darray<CollisionData> m_colliding;
	//! quit the game when true
	bool terminate = false;

	explicit GameWorld(ecs::World& world): w(world) {}

	void init() {
		InitWorldMap();
		CreatePlayer();
		CreateEnemies();
		CreateItems();
	}

	void InitWorldMap() {
		// map
		GAIA_FOR_(ScreenY, y) {
			GAIA_FOR_(ScreenX, x) {
				map[y][x] = TILE_FREE;
			}
		}

		// edges
		GAIA_FOR_(ScreenY, y) {
			map[y][0] = TILE_WALL;
			map[y][ScreenX - 1] = TILE_WALL;
		}
		GAIA_FOR_(ScreenX, x) {
			map[0][x] = TILE_WALL;
			map[ScreenY - 1][x] = TILE_WALL;
		}
		// random obstacles in the upper right part
		srand(0);
		GAIA_FOR2_(3, ScreenY / 2, y) {
			GAIA_FOR2_(ScreenX / 2, ScreenX - 2, x) {
				bool placeWall = (rand() % 4) == 0;
				if (placeWall)
					map[y][x] = TILE_WALL;
			}
		}

#if USE_PATHFINDING
		std::vector<AStar::Node> graph;
		graph.reserve(ScreenX * ScreenY);
		uint32_t index = 0;
		GAIA_FOR_(ScreenY, y) {
			GAIA_FOR_(ScreenX, x) {
				AStar::Node node{index++};

				if (y > 0)
					node.InitIndex(0, map[y - 1][x] != TILE_WALL);
				if (x < ScreenX - 1)
					node.InitIndex(1, map[y][x + 1] != TILE_WALL);
				if (y < ScreenY - 1)
					node.InitIndex(2, map[y + 1][x] != TILE_WALL);
				if (x > 0)
					node.InitIndex(3, map[y][x - 1] != TILE_WALL);

				graph.push_back(GAIA_MOV(node));
			}
		}

		AStar astar;
		auto path = astar.FindPath(graph, AStar::NodeIdFromXY(2, 2), AStar::NodeIdFromXY(30, 10));
		for (auto id: path) {
			const auto nx = AStar::NodeIdToX(id);
			const auto ny = AStar::NodeIdToY(id);
			map[ny][nx] = TILE_PATH;
		}
#endif
	}

	void CreatePlayer() {
		auto player = w.add();
		w.build(player) //
				.add<Position>()
				.add<Velocity>()
				.add<RigidBody>()
				.add<Orientation>()
				.add<Sprite>()
				.add<Health>()
				.add<BattleStats>()
				.add<Player>();
		w.acc_mut(player) //
				.set<Position>({5, 10})
				.set<Velocity>({0, 0})
				.set<Orientation>({1, 0})
				.set<Sprite>({TILE_PLAYER})
				.set<Health>({100, 100})
				.set<BattleStats>({9, 5});
	}

	void CreateEnemies() {
		cnt::sarray<ecs::Entity, 3> enemies;
		GAIA_EACH(enemies) {
			auto& e = enemies[i];
			e = w.add();
			w.build(e) //
					.add<Position>()
					.add<Velocity>()
					.add<RigidBody>()
					.add<Sprite>()
					.add<Health>()
					.add<BattleStats>();

			const bool isOrc = (i % 2) != 0;
			if (isOrc) {
				w.acc_mut(e) //
						.set<Sprite>({TILE_ENEMY_ORC})
						.set<Health>({60, 60})
						.set<BattleStats>({12, 7});
			} else {
				w.acc_mut(e) //
						.set<Sprite>({TILE_ENEMY_GOBLIN})
						.set<Health>({40, 40})
						.set<BattleStats>({10, 5});
			}
		}
		w.set<Position>(enemies[0]) = {8, 8};
		w.set<Position>(enemies[1]) = {10, 10};
		w.set<Position>(enemies[2]) = {12, 12};
	}

	void CreateItems() {
		auto potion = w.add();
		w.build(potion) //
				.add<Position>()
				.add<Sprite>()
				.add<RigidBody>()
				.add<Item>()
				.add<BattleStats>();
		w.acc_mut(potion) //
				.set<Position>({5, 5})
				.set<Sprite>({TILE_POTION})
				.set<Item>({ItemType::Potion})
				.set<BattleStats>({10, 0});

		auto poison = w.add();
		w.build(poison) //
				.add<Position>()
				.add<Sprite>()
				.add<RigidBody>()
				.add<Item>()
				.add<BattleStats>();
		w.acc_mut(poison) //
				.set<Position>({15, 10})
				.set<Sprite>({TILE_POISON})
				.set<Item>({ItemType::Poison})
				.set<BattleStats>({-10, 0});
	}

	void CreateArrow(Position p, Velocity v) {
		auto e = w.add();
		w.build(e) //
				.add<Position>()
				.add<Velocity>()
				.add<Sprite>()
				.add<RigidBody>()
				.add<Item>()
				.add<BattleStats>()
				.add<Health>();
		w.acc_mut(e) //
				.set<Position>(GAIA_MOV(p))
				.set<Velocity>(GAIA_MOV(v))
				.set<Sprite>({TILE_ARROW})
				.set<Item>({ItemType::Arrow})
				.set<BattleStats>({10, 0})
				.set<Health>({1, 1});
	}
};

struct GameWorldComponent {
	GameWorld* pGameWorld;
};

GameWorld* g_pGameWorld = nullptr;

#define PRINT_SYSTEM_NAME 1

int main() {
	clear_screen();
	printf(
			"Welcome to the most rudimentary of rogue-likes driven by Gaia-ECS.\n"
			"\nControls:\n"
			"  %c, %c, %c, %c - movement\n"
			"  %c - shoot an arrow\n"
			"     - for melee, apply movement in the direction of the enemy standing next to you\n"
			"  %c - quit the game\n"
			"\nLegend:\n"
			"  %c - player\n"
			"  %c - goblin, a small but terrifying monster\n"
			"  %c - orc, a big run-on-sight monster\n"
			"  %c - potion, replenishes health\n"
			"  %c - poison, damages health\n"
			"  %c - wall\n"
			"\nPress any key to continue...\n",
			KEY_UP, KEY_LEFT, KEY_DOWN, KEY_RIGHT, KEY_SHOOT, KEY_QUIT, TILE_PLAYER, TILE_ENEMY_GOBLIN, TILE_ENEMY_ORC,
			TILE_POTION, TILE_POISON, TILE_WALL);

	ecs::World w;
	GameWorld g_world(w);
	g_pGameWorld = &g_world;

	auto entGW = w.add();
	w.add<GameWorldComponent>(entGW, {&g_world});
	w.name(entGW, "GameWorld");

	auto groupPreSimulation = w.add();
	auto groupSimulation = w.add();
	auto groupPostSimulation = w.add();
	w.name(groupPreSimulation, "PreSimulation");
	w.name(groupSimulation, "Simulation");
	w.name(groupPostSimulation, "PostSimulation");

	// Pre-simulation step
	{
		// InputSystem
		{
			auto sb = w.system() //
										.name("InputSystem")
										.all<Velocity&>()
										.all<Position>()
										.all<Orientation>()
										.all<Player>()
										.on_each([](Velocity& v, const Position& p, const Orientation& o) {
#if PRINT_SYSTEM_NAME
											GAIA_LOG_D("1 - InputSystem");
#endif
											const char key = get_char();
											g_pGameWorld->terminate = key == KEY_QUIT;

											v = {0, 0};
											if (key == KEY_UP) {
												v = {0, -1};
											} else if (key == KEY_DOWN) {
												v = {0, 1};
											} else if (key == KEY_LEFT) {
												v = {-1, 0};
											} else if (key == KEY_RIGHT) {
												v = {1, 0};
											} else if (key == KEY_SHOOT) {
												g_pGameWorld->CreateArrow({p.x, p.y}, {o.x, o.y});
											}
										});
			w.child(sb.entity(), groupPreSimulation);
		}
		// UpdateMapSystem
		{
			auto sb = w.system() //
										.name("UpdateMapSystem")
										.all<Position>()
										.all<RigidBody>()
										.on_each([](ecs::Iter& it) {
#if PRINT_SYSTEM_NAME
											GAIA_LOG_D("2 - UpdateMapSystem");
#endif
											auto ve = it.view<ecs::Entity>();
											auto vp = it.view<Position>();
											GAIA_EACH(it) {
												g_pGameWorld->content[vp[i]].push_back(ve[i]);
											}
										});
			w.child(sb.entity(), groupPreSimulation);
		}
	}
	// Simulation
	{
		// OrientationSystem
		{
			auto sb = w.system() //
										.name("OrientationSystem")
										.all<Orientation&>()
										.all<Velocity>()
										.on_each([](Orientation& o, const Velocity& v) {
#if PRINT_SYSTEM_NAME
											GAIA_LOG_D("3 - OrientationSystem");
#endif
											if (v.x != 0) {
												o.x = v.x > 0 ? 1 : -1;
												o.y = 0;
											}
											if (v.y != 0) {
												o.x = 0;
												o.y = v.y > 0 ? 1 : -1;
											}
										});
			w.child(sb.entity(), groupSimulation);
		}
		// CollisionSystem
		{
			auto sb = w.system() //
										.name("CollisionSystem")
										.all<Velocity&>()
										.all<Position>()
										.all<RigidBody>()
										.on_each([&](ecs::Iter& iter) {
#if PRINT_SYSTEM_NAME
											GAIA_LOG_D("4 - CollisionSystem");
#endif
											auto getSign = [](int val) {
												return val < 0 ? -1 : 1;
											};

											auto& w = *iter.world();
											auto ent = iter.view<ecs::Entity>();
											auto vel = iter.view_mut<Velocity>();
											auto pos = iter.view<Position>();

											// Drop all previous collision records
											auto& coll = g_pGameWorld->m_colliding;
											coll.clear();

											const auto cnt = iter.size();
											GAIA_FOR(cnt) {
												// Skip stationary objects
												const auto& v = vel[i]; // This is <= 8 bytes so it would be okay even if we did a copy rather
																								// than const reference
												if (v.x == 0 && v.y == 0)
													return;

												auto e = ent[i];
												const auto& p = pos[i]; // This is <= 8 bytes so it would be okay even if we did a copy rather
																								// than const reference

												// Traverse the path along the velocity vector.
												// Normally this would be something like DDA algorithm or anything you like.
												// However, for the sake of simplicity, because we only move horizontally or vertically,
												// we are fine with traversing one axis exclusively.
												const int ii = v.y != 0;
												const int jj = (ii + 1) & 1;
												const int dd[2] = {ii ? 0 : getSign(v.x), ii ? getSign(v.y) : 0};
												int pp[2] = {p.x + dd[0], p.y + dd[1]};
												const int vv[2] = {v.x, v.y};
												const int pp_end = pp[ii] + vv[ii];

												const auto collisionsBefore = coll.size();

												int naa = 0;
												for (; pp[ii] != pp_end; pp[ii] += dd[ii], naa += dd[ii]) {
													// Stop on wall collisions
													if (g_pGameWorld->map[pp[1]][pp[0]] == TILE_WALL) {
														coll.push_back({e, ecs::IdentifierBad, Position{pp[0], pp[1]}, v});
														goto onCollision;
													}

													// Skip when there's no content
													auto it = g_pGameWorld->content.find({pp[0], pp[1]});
													if (it == g_pGameWorld->content.end())
														continue;

													// Generate content collision data
													bool hadCollision = false;
													for (auto e2: it->second) {
														if (e == e2)
															continue;

														// If the content has non-zero velocity we need to determine if we'd hit it
														if (w.has<Velocity>(e2)) {
															auto v2 = w.get<Velocity>(e2);
															if (v2.x != 0 && v2.y != 0) {
																const int vv2[2] = {v2.x, v2.y};

																// No hit possible if moving on different axes
																if (vv[ii] != vv2[jj])
																	continue;

																// No hit possible if moving just as fast or faster than us in the same direction
																if (vv2[ii] > 0 && vv[i] > 0 && vv2[ii] >= vv[ii])
																	continue;
															}
														}

														coll.push_back({e, e2, Position{pp[0], pp[1]}, v});
														hadCollision = true;
													}
													if (hadCollision)
														break;
												}

												// Skip if no collisions were detected
												if (collisionsBefore == coll.size())
													return;

											onCollision:
												// Alter the velocity according to the first contact we made along the way.
												// We make every collision stop the moving object. This is only a question of design.
												// We might as well keep the velocity and handle the collision aftermath in a different system.
												// Or we could introduce collision layers or many other things.
												if (v.x != 0)
													vel[i] = {naa, 0};
												else
													vel[i] = {0, naa};
											}
										});
			w.child(sb.entity(), groupSimulation);
		}
		// MoveSystem
		{
			auto sb = w.system() //
										.name("MoveSystem")
										.all<Position&>()
										.all<Velocity>()
										.on_each([](Position& p, const Velocity& v) {
#if PRINT_SYSTEM_NAME
											GAIA_LOG_D("5 - MoveSystem");
#endif
											p.x += v.x;
											p.y += v.y;
										});
			w.child(sb.entity(), groupSimulation);
		}
		// HandleDamageSystem
		{
			auto sb = w.system() //
										.name("HandleDamageSystem")
										.all<GameWorldComponent>()
										.on_each([](ecs::Iter& iter) {
#if PRINT_SYSTEM_NAME
											GAIA_LOG_D("6 - HandleDamageSystem");
#endif
											const auto& w = *iter.world();
											const auto& gw = iter.view<GameWorldComponent>()[0];
											for (const auto& coll: gw.pGameWorld->m_colliding) {
												// Skip world collisions
												if (coll.e2 == ecs::IdentifierBad)
													continue;

												uint32_t idx1{}, idx2{};
												auto* pChunk1 = w.get_chunk(coll.e1, idx1);
												auto* pChunk2 = w.get_chunk(coll.e2, idx2);
												GAIA_ASSERT(pChunk1 != nullptr);
												GAIA_ASSERT(pChunk2 != nullptr);

												// Skip non-damageable things
												if (!pChunk2->has<Health>())
													continue;
												if (!pChunk1->has<BattleStats>() || !pChunk2->has<BattleStats>())
													continue;

												// Verify if damage can be applied (e.g. power > armor)
												const auto stats1 = pChunk1->view<BattleStats>();
												const auto stats2 = pChunk2->view<BattleStats>();

												const int damage = stats1[idx1].power - stats2[idx2].armor;
												if (damage < 0)
													continue;

												// Apply damage
												auto health2 = pChunk2->view_mut<Health>();
												health2[idx2].value -= damage;
											}
										});
			w.child(sb.entity(), groupSimulation);
			w.add(sb.entity(), {ecs::DependsOn, w.get("CollisionSystem")});
		}
		// HandleItemHitSystem
		{
			auto sb = w.system() //
										.name("HandleItemHitSystem")
										.all<GameWorldComponent>()
										.on_each([](ecs::Iter& iter) {
#if PRINT_SYSTEM_NAME
											GAIA_LOG_D("7 - HandleItemHitSystem");
#endif
											const auto& w = *iter.world();
											const auto& gw = iter.view<GameWorldComponent>()[0];
											for (const auto& coll: gw.pGameWorld->m_colliding) {
												// Entity -> world content collision
												if (coll.e2 == ecs::IdentifierBad) {
													uint32_t idx1{};
													auto* pChunk1 = w.get_chunk(coll.e1, idx1);
													GAIA_ASSERT(pChunk1 != nullptr);

													// An arrow colliding with something. Bring its health to 0 (destroyed).
													// We could have simply called world().del(coll.e1) but doing it
													// this way allows our more control. Who knows what kinds of effect and
													// post-processing we might have in mind for the arrow later in the frame.
													if (pChunk1->has<Item>() && pChunk1->has<Health>()) {
														auto item1 = pChunk1->view<Item>();
														if (item1[idx1].type == ItemType::Arrow) {
															auto health1 = pChunk1->view_mut<Health>();
															health1[idx1].value = 0;
														}
													}
												}
												// Entity -> entity collision
												else {
													uint32_t idx1{}, idx2{};
													auto* pChunk1 = w.get_chunk(coll.e1, idx1);
													auto* pChunk2 = w.get_chunk(coll.e2, idx2);
													GAIA_ASSERT(pChunk1 != nullptr);
													GAIA_ASSERT(pChunk2 != nullptr);

													// TODO: Add ability to get a list of components based on query

													// E.g. a player coll with an item
													if (pChunk1->has<Health>() && pChunk2->has<Item>() && pChunk2->has<BattleStats>()) {
														auto health1 = pChunk1->view_mut<Health>();
														auto stats2 = pChunk2->view<BattleStats>();

														// Apply the item's effect
														health1[idx1].value += stats2[idx2].power;
													}

													// An arrow coll with something. Bring its health to 0 (destroyed).
													if (pChunk1->has<Item>() && pChunk1->has<Health>()) {
														auto item1 = pChunk1->view<Item>();
														if (item1[idx1].type == ItemType::Arrow) {
															auto health1 = pChunk1->view_mut<Health>();
															health1[idx1].value = 0;
														}
													}
												}
											}
										});
			w.child(sb.entity(), groupSimulation);
			w.add(sb.entity(), {ecs::DependsOn, w.get("CollisionSystem")});
		}
		// HandleHealthSystem
		{
			auto sb = w.system() //
										.name("HandleHealthSystem")
										.all<Health&>()
										.changed<Health>()
										.on_each([](Health& h) {
#if PRINT_SYSTEM_NAME
											GAIA_LOG_D("8 - HandleHealthSystem");
#endif
											if (h.value > h.valueMax)
												h.value = h.valueMax;
										});
			w.child(sb.entity(), groupSimulation);
			w.add(sb.entity(), {ecs::DependsOn, w.get("HandleDamageSystem")});
		}
		// HandleDeathSystem
		{
			auto sb = w.system() //
										.name("HandleDeathSystem")
										.all<Health>()
										.all<Position>()
										.changed<Health>()
										.on_each([](ecs::Iter& it) {
#if PRINT_SYSTEM_NAME
											GAIA_LOG_D("9 - HandleDeathSystem");
#endif

											auto& cmdBuffer = it.cmd_buffer_mt();
											auto ve = it.view<ecs::Entity>();
											auto vp = it.view<Position>();
											auto vh = it.view<Health>();

											GAIA_EACH(it) {
												if (vh[i].value > 0)
													return;

												g_pGameWorld->map[vp[i].y][vp[i].x] = TILE_FREE;
												cmdBuffer.del(ve[i]);
											}
										});
			w.child(sb.entity(), groupSimulation);
			w.add(sb.entity(), {ecs::DependsOn, w.get("HandleHealthSystem")});
		}
	}
	// Post-simulation step
	{
		// ClearMapSystem
		{
			auto sb = w.system() //
										.name("ClearMapSystem")
										.all<GameWorldComponent&>()
										.on_each([](GameWorldComponent& value) {
#if PRINT_SYSTEM_NAME
											GAIA_LOG_D("10 - ClearMapSystem");
#endif
											clear_screen();
											value.pGameWorld->InitWorldMap();
										});
			w.child(sb.entity(), groupPostSimulation);
		}
		// WriteSpritesToMapSystem
		{
			auto sb = w.system() //
										.name("WriteSpritesToMapSystem")
										.all<Position>()
										.all<Sprite>()
										.on_each([](const Position& p, const Sprite& s) {
											g_pGameWorld->map[p.y][p.x] = s.value;
										});
			w.child(sb.entity(), groupPostSimulation);
		}
		// RenderSystem
		{
			auto sb = w.system() //
										.name("RenderSystem")
										.all<GameWorldComponent>()
										.on_each([](const GameWorldComponent& value) {
#if PRINT_SYSTEM_NAME
											GAIA_LOG_D("12 - RenderSystem");
#endif
											GAIA_FOR_(ScreenY, y) {
												GAIA_FOR_(ScreenX, x) {
													putchar(value.pGameWorld->map[y][x]);
												}
												printf("\n");
											}
										});
			w.child(sb.entity(), groupPostSimulation);
		}
		// UISystem Player
		{
			auto sb = w.system() //
										.name("UISystemP")
										.all<Health>()
										.all<Player>()
										.on_each([](const Health& h) {
											printf("Player health: %d/%d\n", h.value, h.valueMax);
										});
			w.child(sb.entity(), groupPostSimulation);
		}
		// UISystem Non-player
		{
			auto sb = w.system() //
										.name("UISystemNP")
										.all<Health>()
										.no<Player>()
										.no<Item>()
										.on_each([](ecs::Entity e, const Health& h) {
											printf("Enemy %u:%u health: %d/%d\n", e.id(), e.gen(), h.value, h.valueMax);
										});
			w.child(sb.entity(), groupPostSimulation);
		}
		// GameStateSystem Player
		{
			struct GameStateSystemPData {
				bool hadPlayer = false;
			};
			// auto sysDataEntity = w.add<GameStateSystemPData>().entity;

			auto sb = w.system() //
										.name("GameStateSystemP")
										.all<Health>()
										.all<Player>()
										.on_each([](ecs::Iter& it) {
											auto sysEnt = it.world()->get("GameStateSystemP");
											auto& sysData = it.world()->set<GameStateSystemPData>(sysEnt);

											const bool hasNoPlayer = it.size() == 0;
											if (sysData.hadPlayer && hasNoPlayer) {
												printf("You are dead. Good job.\n");
												g_pGameWorld->terminate = true;
											}
											sysData.hadPlayer = !hasNoPlayer;
										});
			w.child(sb.entity(), groupPostSimulation);
			w.add<GameStateSystemPData>(sb.entity());
			// sb.add({ecs::QueryOpKind::All, ecs::QueryAccess::Write, sysDataEntity, sb.entity()});
		}
		// GameStateSystem Non-player
		{
			struct GameStateSystemNPData {
				bool hadEnemies = false;
			};
			// auto sysDataEntity = w.add<GameStateSystemPData>().entity;

			auto sb = w.system() //
										.name("GameStateSystemNP")
										.all<Health>()
										.no<Player>()
										.no<Item>()
										.on_each([](ecs::Iter& it) {
											auto sysEnt = it.world()->get("GameStateSystemNP");
											auto& sysData = it.world()->set<GameStateSystemNPData>(sysEnt);

											const bool hasNoEnemies = it.size() == 0;
											if (sysData.hadEnemies && hasNoEnemies) {
												printf("All enemies are gone. They must have died of old age waiting for you to kill them.\n");
												g_pGameWorld->terminate = true;
											}
											sysData.hadEnemies = !hasNoEnemies;
										});
			w.child(sb.entity(), groupPostSimulation);
			w.add<GameStateSystemNPData>(sb.entity());
			// sb.add({ecs::QueryOpKind::All, ecs::QueryAccess::Write, sysDataEntity, sb.entity()});
		}
	}

	g_pGameWorld->init();

	while (!g_pGameWorld->terminate) {
		w.update();
	}

	return 0;
}

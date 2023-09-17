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
char get_char() {
	char buf = 0;
	termios old{};
	if (tcgetattr(0, &old) < 0)
		perror("tcsetattr()");
	old.c_lflag &= ~ICANON;
	old.c_lflag &= ~ECHO;
	old.c_cc[VMIN] = 1;
	old.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSANOW, &old) < 0)
		perror("tcsetattr ICANON");
	if (read(0, &buf, 1) < 0)
		perror("read()");
	old.c_lflag |= ICANON;
	old.c_lflag |= ECHO;
	if (tcsetattr(0, TCSADRAIN, &old) < 0)
		perror("tcsetattr ~ICANON");
	return (buf);
}

void ClearScreen() {
	auto res = system("clear");
	(void)res;
}
#else
char get_char() {
	return (char)_getch();
}
void ClearScreen() {
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
			const uint64_t h1 = utils::calculate_hash64(p.x);
			const uint64_t h2 = utils::calculate_hash64(p.y);
			return utils::hash_combine(h1, h2);
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
//! Tag represention physical object that can interact with one-another
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

		uint32_t GetId() const {
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
		const uint32_t cid = current.GetId();
		const uint32_t gid = goal.GetId();
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
		containers::map<uint32_t, uint32_t> parents; // key: ID, data: parentID
		containers::map<uint32_t, std::pair<float, float>> scores; // key: ID, data: <g, f>
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
			for (uint32_t i = 0; i < MAX_NEIGHBORS; ++i) {
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
// Global definitions.
// Normally these would not be global but this is just a simple demo
// in one source file.

ecs::World g_ecs;
ecs::SystemManager g_smPreSimulation(g_ecs);
ecs::SystemManager g_smSimulation(g_ecs);
ecs::SystemManager g_smPostSimulation(g_ecs);

//----------------------------------------------------------------------
// Game world
//----------------------------------------------------------------------

struct World {
	ecs::World& w;
	//! map tiles
	char map[ScreenY][ScreenX]{};
	//! tile content
	containers::map<Position, containers::darray<ecs::Entity>> content;
	//! quit the game when true
	bool terminate = false;

	explicit World(ecs::World& world): w(world) {}

	void Init() {
		InitWorldMap();
		CreatePlayer();
		CreateEnemies();
		CreateItems();
	}

	void InitWorldMap() {
		// map
		for (uint32_t y = 0; y < ScreenY; ++y) {
			for (uint32_t x = 0; x < ScreenX; ++x) {
				map[y][x] = TILE_FREE;
			}
		}

		// edges
		for (uint32_t y = 1; y < ScreenY - 1; ++y) {
			map[y][0] = TILE_WALL;
			map[y][ScreenX - 1] = TILE_WALL;
		}
		for (uint32_t x = 0; x < ScreenX; ++x) {
			map[0][x] = TILE_WALL;
			map[ScreenY - 1][x] = TILE_WALL;
		}
		// random obstacles in the upper right part
		srand(0);
		for (uint32_t y = 3; y < ScreenY / 2; ++y) {
			for (uint32_t x = ScreenX / 2; x < ScreenX - 2; ++x) {
				bool placeWall = (rand() % 4) == 0;
				if (placeWall)
					map[y][x] = TILE_WALL;
			}
		}

#if USE_PATHFINDING
		std::vector<AStar::Node> graph;
		graph.reserve(ScreenX * ScreenY);
		uint32_t index = 0;
		for (uint32_t y = 0; y < ScreenY; ++y) {
			for (uint32_t x = 0; x < ScreenX; ++x, ++index) {
				AStar::Node node{index};

				if (y > 0)
					node.InitIndex(0, map[y - 1][x] != TILE_WALL);
				if (x < ScreenX - 1)
					node.InitIndex(1, map[y][x + 1] != TILE_WALL);
				if (y < ScreenY - 1)
					node.InitIndex(2, map[y + 1][x] != TILE_WALL);
				if (x > 0)
					node.InitIndex(3, map[y][x - 1] != TILE_WALL);

				graph.push_back(std::move(node));
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
		auto player = w.CreateEntity();
		w.AddComponent<Position>(player, {5, 10});
		w.AddComponent<Velocity>(player, {0, 0});
		w.AddComponent<RigidBody>(player);
		w.AddComponent<Orientation>(player, {1, 0});
		w.AddComponent<Sprite>(player, {TILE_PLAYER});
		w.AddComponent<Health>(player, {100, 100});
		w.AddComponent<BattleStats>(player, {9, 5});
		w.AddComponent<Player>(player);
	}

	void CreateEnemies() {
		containers::sarray<ecs::Entity, 3> enemies;
		for (size_t i = 0; i < enemies.size(); ++i) {
			auto& e = enemies[i];
			e = w.CreateEntity();
			w.AddComponent<Position>(e, {});
			w.AddComponent<Velocity>(e, {0, 0});
			w.AddComponent<RigidBody>(e);
			const bool isOrc = i % 2;
			if (isOrc) {
				w.AddComponent<Sprite>(e, {TILE_ENEMY_ORC});
				w.AddComponent<Health>(e, {60, 60});
				w.AddComponent<BattleStats>(e, {12, 7});
			} else {
				w.AddComponent<Sprite>(e, {TILE_ENEMY_GOBLIN});
				w.AddComponent<Health>(e, {40, 40});
				w.AddComponent<BattleStats>(e, {10, 5});
			}
		}
		w.SetComponent<Position>(enemies[0], {8, 8});
		w.SetComponent<Position>(enemies[1], {10, 10});
		w.SetComponent<Position>(enemies[2], {12, 12});
	}

	void CreateItems() {
		auto potion = w.CreateEntity();
		w.AddComponent<Position>(potion, {5, 5});
		w.AddComponent<Sprite>(potion, {TILE_POTION});
		w.AddComponent<RigidBody>(potion);
		w.AddComponent<Item>(potion, {ItemType::Potion});
		w.AddComponent<BattleStats>(potion, {10, 0});

		auto poison = w.CreateEntity();
		w.AddComponent<Position>(poison, {15, 10});
		w.AddComponent<Sprite>(poison, {TILE_POISON});
		w.AddComponent<RigidBody>(poison);
		w.AddComponent<Item>(poison, {ItemType::Poison});
		w.AddComponent<BattleStats>(poison, {-10, 0});
	}

	void CreateArrow(Position p, Velocity v) {
		auto e = w.CreateEntity();
		w.AddComponent<Position>(e, std::move(p));
		w.AddComponent<Velocity>(e, std::move(v));
		w.AddComponent<RigidBody>(e);
		w.AddComponent<Sprite>(e, {TILE_ARROW});
		w.AddComponent<Item>(e, {ItemType::Arrow});
		w.AddComponent<BattleStats>(e, {10, 0});
		w.AddComponent<Health>(e, {1, 1});
	}
};
World g_world(g_ecs);

//----------------------------------------------------------------------
// System
//----------------------------------------------------------------------

class UpdateMapSystem final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<const Position, RigidBody>();
	}
	void OnUpdate() override {
		g_world.content.clear();
		m_q.ForEach([&](ecs::Entity e, const Position& p) {
			g_world.content[p].push_back(e);
		});
	}
};

struct CollisionData {
	//! Entity colliding
	ecs::Entity e1;
	//! Entity being collided with
	ecs::Entity e2;
	//! Position of collision
	Position p;
	//! Velocity at the time of collision
	Velocity v;
};

class CollisionSystem final: public ecs::System {
	ecs::Query m_q;
	containers::darray<CollisionData> m_colliding;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<const Position, Velocity, const RigidBody>();
	}

	void OnUpdate() override {
		// Drop all previous collision records
		m_colliding.clear();

		auto getSign = [](int val) {
			return val < 0 ? -1 : 1;
		};

		m_q.ForEach([&](ecs::Iterator iter) {
			auto ent = iter.View<ecs::Entity>();
			auto vel = iter.ViewRW<Velocity>();
			auto pos = iter.View<Position>();

			for (uint32_t i: iter) {
				// Skip stationary objects
				const auto& v =
						vel[i]; // This is <= 8 bytes so it would be okay even if we did a copy rather than const reference
				if (v.x == 0 && v.y == 0)
					continue;

				auto e = ent[i];
				const auto& p =
						pos[i]; // This is <= 8 bytes so it would be okay even if we did a copy rather than const reference

				// Traverse the path along the velocity vector.
				// Normally this would be something like DDA algoritm or anything you like.
				// However, for the sake of simplicity, because we only move horizontally or vertically,
				// we are fine with traversing one axis exclusively.
				const int ii = v.y != 0;
				const int jj = (ii + 1) & 1;
				const int dd[2] = {ii ? 0 : getSign(v.x), ii ? getSign(v.y) : 0};
				int pp[2] = {p.x + dd[0], p.y + dd[1]};
				const int vv[2] = {v.x, v.y};
				const int pp_end = pp[ii] + vv[ii];

				const auto collisionsBefore = m_colliding.size();

				int naa = 0;
				for (; pp[ii] != pp_end; pp[ii] += dd[ii], naa += dd[ii]) {
					// Stop on wall collisions
					if (g_world.map[pp[1]][pp[0]] == TILE_WALL) {
						m_colliding.push_back({e, ecs::EntityNull, Position{pp[0], pp[1]}, v});
						goto onCollision;
					}

					// Skip when there's no content
					auto it = g_world.content.find({pp[0], pp[1]});
					if (it == g_world.content.end())
						continue;

					// Generate content collision data
					bool hadCollision = false;
					for (auto e2: it->second) {
						// If the content has non-zero velocity we need to determine if we'd hit it
						if (GetWorld().HasComponent<Velocity>(e2)) {
							auto v2 = GetWorld().GetComponent<Velocity>(e2);
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

						m_colliding.push_back({e, e2, Position{pp[0], pp[1]}, v});
						hadCollision = true;
					}
					if (hadCollision)
						break;
				}

				// Skip if no collisions were detected
				if (collisionsBefore == m_colliding.size())
					continue;

			onCollision:
				// Alter the velocity according to the first contact we made along the way.
				// We make every collsion stop the moving object. This is only a question of design.
				// We might as well keep the velocity and handle the collision aftermath in a different system.
				// Or we could interoduce collision layers or many other things.
				if (v.x != 0)
					vel[i] = {naa, 0};
				else
					vel[i] = {0, naa};
			}
		});
	}

	const containers::darray<CollisionData>& GetCollisions() const {
		return m_colliding;
	}
};

class OrientationSystem final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<Orientation, const Velocity>().WithChanged<Velocity>();
	}

	void OnUpdate() override {
		// Update orientation based on the current velocity
		m_q.ForEach([&](Orientation& o, const Velocity& v) {
			if (v.x != 0) {
				o.x = v.x > 0 ? 1 : -1;
				o.y = 0;
			}
			if (v.y != 0) {
				o.x = 0;
				o.y = v.y > 0 ? 1 : -1;
			}
		});
	}
};

class MoveSystem final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<Position, const Velocity>();
	}

	void OnUpdate() override {
		// Update position based on current velocity
		m_q.ForEach([&](Position& p, const Velocity& v) {
			p.x += v.x;
			p.y += v.y;
		});
	}
};

class HandleDamageSystem final: public ecs::System {
	CollisionSystem* m_collisionSystem{};

public:
	void OnCreated() override {
		m_collisionSystem = g_smSimulation.FindSystem<CollisionSystem>();
	}

	void OnUpdate() override {
		const auto& colls = m_collisionSystem->GetCollisions();
		for (const auto& coll: colls) {
			// Skip world collisions
			if (coll.e2 == ecs::EntityNull)
				continue;

			uint32_t idx1{}, idx2{};
			auto* pChunk1 = GetWorld().GetChunk(coll.e1, idx1);
			auto* pChunk2 = GetWorld().GetChunk(coll.e2, idx2);

			// Skip non-damagable things
			if (!pChunk2->HasComponent<Health>())
				continue;
			if (!pChunk1->HasComponent<BattleStats>() || !pChunk2->HasComponent<BattleStats>())
				continue;

			// Verify if damage can be applied (e.g. power > armor)
			const auto stats1 = pChunk1->View<BattleStats>();
			const auto stats2 = pChunk2->View<BattleStats>();

			const int damage = stats1[idx1].power - stats2[idx2].armor;
			if (damage < 0)
				continue;

			// Apply damage
			auto health2 = pChunk2->ViewRW<Health>();
			health2[idx2].value -= damage;
		}
	}

	bool DependsOn([[maybe_unused]] const BaseSystem* system) const override {
		return system == g_smSimulation.FindSystem<CollisionSystem>();
	}
};

class HandleItemHitSystem final: public ecs::System {
	CollisionSystem* m_collisionSystem{};

public:
	void OnCreated() override {
		m_collisionSystem = g_smSimulation.FindSystem<CollisionSystem>();
	}

	void OnUpdate() override {
		const auto& colls = m_collisionSystem->GetCollisions();
		for (const auto& coll: colls) {
			// Entity -> world content collision
			if (coll.e2 == ecs::EntityNull) {
				uint32_t idx1{};
				auto* pChunk1 = GetWorld().GetChunk(coll.e1, idx1);
				GAIA_ASSERT(pChunk1 != nullptr);

				// An arrow colliding with something. Bring its health to 0 (destroyed).
				// We could have simpy called GetWorld().DeleteEntity(coll.e1) but doing it
				// this way allows our more control. Who knows what kinds of effect and
				// post-processing we might have in mind for the arrow later in the frame.
				if (pChunk1->HasComponent<Item>() && pChunk1->HasComponent<Health>()) {
					auto item1 = pChunk1->View<Item>();
					if (item1[idx1].type == ItemType::Arrow) {
						auto health1 = pChunk1->ViewRW<Health>();
						health1[idx1].value = 0;
					}
				}
			}
			// Entity -> entity collision
			else {
				uint32_t idx1{}, idx2{};
				auto* pChunk1 = GetWorld().GetChunk(coll.e1, idx1);
				auto* pChunk2 = GetWorld().GetChunk(coll.e2, idx2);
				GAIA_ASSERT(pChunk1 != nullptr);
				GAIA_ASSERT(pChunk2 != nullptr);

				// TODO: Add ability to get a list of components based on query

				// E.g. a player colliding with an item
				if (pChunk1->HasComponent<Health>() && pChunk2->HasComponent<Item>() && pChunk2->HasComponent<BattleStats>()) {
					auto health1 = pChunk1->ViewRW<Health>();
					auto stats2 = pChunk2->View<BattleStats>();

					// Apply the item's effect
					health1[idx1].value += stats2[idx2].power;
				}

				// An arrow colliding with something. Bring its health to 0 (destroyed).
				if (pChunk1->HasComponent<Item>() && pChunk1->HasComponent<Health>()) {
					auto item1 = pChunk1->View<Item>();
					if (item1[idx1].type == ItemType::Arrow) {
						auto health1 = pChunk1->ViewRW<Health>();
						health1[idx1].value = 0;
					}
				}
			}
		}
	}

	bool DependsOn([[maybe_unused]] const BaseSystem* system) const override {
		return system == g_smSimulation.FindSystem<CollisionSystem>();
	}
};

class HandleHealthSystem final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<Health>().WithChanged<Health>();
	}

	void OnUpdate() override {
		m_q.ForEach([&](Health& h) {
			if (h.value > h.valueMax)
				h.value = h.valueMax;
		});
	}
};

class HandleDeathSystem final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<const Health, const Position>().WithChanged<Health>();
	}

	void OnUpdate() override {
		m_q.ForEach([&](ecs::Entity e, const Health& h, const Position& p) {
			if (h.value > 0)
				return;

			g_world.map[p.y][p.x] = TILE_FREE;
			g_smSimulation.AfterUpdateCmdBufer().DeleteEntity(e);
		});
	}
};

class WriteSpritesToMapSystem final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<const Position, const Sprite>();
	}

	void OnUpdate() override {
		ClearScreen();
		g_world.InitWorldMap();
		m_q.ForEach([&](const Position& p, const Sprite& s) {
			g_world.map[p.y][p.x] = s.value;
		});
	}
};

class RenderSystem final: public ecs::System {
public:
	void OnUpdate() override {
		for (uint32_t y = 0; y < ScreenY; ++y) {
			for (uint32_t x = 0; x < ScreenX; ++x) {
				putchar(g_world.map[y][x]);
			}
			printf("\n");
		}
	}
};

class UISystem final: public ecs::System {
	ecs::Query m_qp;
	ecs::Query m_qe;

public:
	void OnCreated() override {
		m_qp = GetWorld().CreateQuery().All<const Health, Player>();
		m_qe = GetWorld().CreateQuery().All<const Health>().None<Player, Item>();
	}

	void OnUpdate() override {
		m_qp.ForEach([](const Health& h) {
			printf("Player health: %d/%d\n", h.value, h.valueMax);
		});

		m_qe.ForEach([&](ecs::Entity e, const Health& h) {
			printf("Enemy %u:%u health: %d/%d\n", e.id(), e.gen(), h.value, h.valueMax);
		});
	}
};

class GameStateSystem: public ecs::System {
	ecs::Query m_qp;
	ecs::Query m_qe;
	bool m_hadEnemies = true;

public:
	void OnCreated() override {
		m_qp = GetWorld().CreateQuery().All<const Health, Player>();
		m_qe = GetWorld().CreateQuery().All<const Health>().None<Player, Item>();
	}

	void OnUpdate() override {
		const bool hasPlayer = m_qp.HasEntities();
		if (!hasPlayer) {
			printf("You are dead. Good job.\n");
			g_world.terminate = true;
		}

		const bool hasEnemies = m_qe.HasEntities();
		if (m_hadEnemies && !hasEnemies) {
			printf("All enemies are gone. They must have died of old age waiting for you to kill them.\n");
			g_world.terminate = true;
		}
		m_hadEnemies = hasEnemies;
	}
};

class InputSystem final: public ecs::System {
	ecs::Query m_q;

public:
	void OnCreated() override {
		m_q = GetWorld().CreateQuery().All<const Player, Velocity, const Position, const Orientation>();
	}

	void OnUpdate() override {
		const char key = get_char();

		g_world.terminate = key == KEY_QUIT;

		// Player movement
		m_q.ForEach([&](Velocity& v, const Position& p, const Orientation& o) {
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
				g_world.CreateArrow({p.x, p.y}, {o.x, o.y});
			}
		});
	}
};

int main() {
	ClearScreen();
	printf(
			"Welcome to the rudimenatiest of roguelikes driven by Gaia-ECS.\n"
			"\nControls:\n"
			"  %c, %c, %c, %c - movement\n"
			"  %c - shoot an arrow\n"
			"     - for melee, apply movement in the direction of the enemy standing next to you\n"
			"  %c - quit the game\n"
			"\nLegend:\n"
			"  %c - player\n"
			"  %c - goblin, a small but terrifying monster\n"
			"  %c - orc, a big and run on sight monster\n"
			"  %c - potion, replenishes health\n"
			"  %c - poison, damages health\n"
			"  %c - wall\n"
			"\nPress any key to continue...\n",
			KEY_UP, KEY_LEFT, KEY_DOWN, KEY_RIGHT, KEY_SHOOT, KEY_QUIT, TILE_PLAYER, TILE_ENEMY_GOBLIN, TILE_ENEMY_ORC,
			TILE_POTION, TILE_POISON, TILE_WALL);

	// Pre-simulation step
	g_smSimulation.CreateSystem<InputSystem>();
	g_smPreSimulation.CreateSystem<UpdateMapSystem>();
	// Simulation
	g_smSimulation.CreateSystem<OrientationSystem>();
	g_smSimulation.CreateSystem<CollisionSystem>();
	g_smSimulation.CreateSystem<MoveSystem>();
	g_smSimulation.CreateSystem<HandleDamageSystem>();
	g_smSimulation.CreateSystem<HandleItemHitSystem>();
	g_smSimulation.CreateSystem<HandleHealthSystem>();
	g_smSimulation.CreateSystem<HandleDeathSystem>();
	// Post-simulation step
	g_smPostSimulation.CreateSystem<WriteSpritesToMapSystem>();
	g_smPostSimulation.CreateSystem<RenderSystem>();
	g_smPostSimulation.CreateSystem<UISystem>();
	g_smPostSimulation.CreateSystem<GameStateSystem>();

	g_world.Init();
	while (!g_world.terminate) {
		g_smPreSimulation.Update();
		g_smSimulation.Update();
		g_smPostSimulation.Update();
		g_ecs.Update();
	}

	return 0;
}

//! \file
//! \brief Grid A* implementation used by enemy chase.

#include "roguelike_path.h"

#include <cmath>

void OpenSet::sift_up(uint32_t i) {
	while (i > 0) {
		const uint32_t p = (i - 1) / 2;
		if (m_nodes[i].f >= m_nodes[p].f)
			break;
		gaia::core::swap(m_nodes[i], m_nodes[p]);
		i = p;
	}
}

void OpenSet::sift_down(uint32_t i) {
	for (;;) {
		const uint32_t l = i * 2 + 1;
		const uint32_t r = l + 1;
		uint32_t best = i;
		if (l < m_nodes.size() && m_nodes[l].f < m_nodes[best].f)
			best = l;
		if (r < m_nodes.size() && m_nodes[r].f < m_nodes[best].f)
			best = r;
		if (best == i)
			break;
		gaia::core::swap(m_nodes[i], m_nodes[best]);
		i = best;
	}
}

void OpenSet::push(OpenNode n) {
	m_nodes.push_back(n);
	sift_up((uint32_t)m_nodes.size() - 1);
}

void OpenSet::pop() {
	m_nodes[0] = m_nodes.back();
	m_nodes.pop_back();
	if (!m_nodes.empty())
		sift_down(0);
}

const OpenNode& OpenSet::top() const {
	return m_nodes[0];
}

bool OpenSet::empty() const {
	return m_nodes.empty();
}

bool OpenSet::has_id(uint32_t id) const {
	GAIA_EACH(m_nodes) {
		if (m_nodes[i].id == id)
			return true;
	}
	return false;
}

AStar::Node::Node(): id(0), neighbors(0), edge_costs(0) {}

AStar::Node::Node(uint32_t value): id(value), neighbors(0), edge_costs(0) {}

void AStar::Node::InitIndex(uint32_t index, uint32_t cost) {
	SetNeighbor(index, cost != 0);
	SetEdgeCost(index, cost);
}

uint32_t AStar::Node::comp_id() const {
	return id;
}

bool AStar::Node::HasNeighbor(uint32_t index) const {
	return (((uint32_t)neighbors >> index) & 1U) != 0U;
}

uint32_t AStar::Node::GetEdgeCost(uint32_t index) const {
	return ((uint32_t)edge_costs >> (index * 2)) & 3U;
}

void AStar::Node::SetNeighbor(uint32_t index, bool value) {
	const uint32_t mask = 1U << index;
	neighbors = (neighbors & ~mask) | ((uint32_t)value << index);
}

void AStar::Node::SetEdgeCost(uint32_t index, uint32_t cost) {
	const uint32_t mask = (3U << (index * 2));
	edge_costs = (edge_costs & ~mask) | ((cost & 3U) << (index * 2));
}

float AStar::HeuristicCostEstimate(const Node& current, const Node& goal) const {
	const uint32_t cid = current.comp_id();
	const uint32_t gid = goal.comp_id();
	const int dx = (int)NodeIdToX(cid) - (int)NodeIdToX(gid);
	const int dy = (int)NodeIdToY(cid) - (int)NodeIdToY(gid);
	return sqrtf((float)(dx * dx + dy * dy));
}

gaia::cnt::darray<uint32_t> AStar::FindPath(const gaia::cnt::darray<Node>& graph, uint32_t start_id, uint32_t goal_id) {
	if (start_id >= graph.size() || goal_id >= graph.size())
		return {};

	gaia::cnt::map<uint32_t, uint32_t> parents;
	gaia::cnt::map<uint32_t, Score> scores;
	gaia::cnt::set<uint32_t> closed_set;
	OpenSet open_set;

	const Node& start_node = graph[start_id];
	const Node& goal_node = graph[goal_id];

	open_set.push({0.f, start_id});
	scores[start_id] = {0.f, HeuristicCostEstimate(start_node, goal_node)};

	while (!open_set.empty()) {
		const uint32_t current_id = open_set.top().id;
		open_set.pop();

		if (current_id == goal_id) {
			gaia::cnt::darray<uint32_t> path;
			uint32_t node_id = goal_id;
			while (node_id != start_id) {
				path.push_back(node_id);
				node_id = parents[node_id];
			}
			path.push_back(start_id);

			uint32_t a = 0;
			uint32_t b = (uint32_t)path.size();
			while (a + 1 < b) {
				--b;
				gaia::core::swap(path[a], path[b]);
				++a;
			}
			return path;
		}

		closed_set.emplace(current_id);

		constexpr int neighborOffsets[MAX_NEIGHBORS] = {-(int)ScreenX, 1, ScreenX, -1};

		const Node& current_node = graph[current_id];
		GAIA_FOR(MAX_NEIGHBORS) {
			if (!current_node.HasNeighbor(i))
				continue;

			const auto neighborCost = (float)current_node.GetEdgeCost(i);
			if (neighborCost <= 0.F)
				continue;

			const uint32_t neighbor_id = current_id + neighborOffsets[i];
			if (neighbor_id >= graph.size())
				continue;
			if (closed_set.find(neighbor_id) != closed_set.end())
				continue;

			const float tentative_g_score = scores[current_id].g + neighborCost;
			if (tentative_g_score < scores[neighbor_id].g) {
				const float f = tentative_g_score + HeuristicCostEstimate(graph[neighbor_id], goal_node);
				parents[neighbor_id] = current_id;
				scores[neighbor_id] = {tentative_g_score, f};
				if (!open_set.has_id(neighbor_id))
					open_set.push({f, neighbor_id});
			} else if (!open_set.has_id(neighbor_id)) {
				const float f = tentative_g_score + HeuristicCostEstimate(graph[neighbor_id], goal_node);
				parents[neighbor_id] = current_id;
				scores[neighbor_id] = {tentative_g_score, f};
				open_set.push({f, neighbor_id});
			}
		}
	}

	return {};
}

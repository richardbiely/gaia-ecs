#pragma once

#include "roguelike_types.h"

//! \file
//! \brief Grid A* used by enemy chase. This is application code, not a Gaia API.

//! Binary heap node stored in the A* open set.
struct OpenNode {
	//! Estimated total cost.
	float f;
	//! Cell id `y * ScreenX + x`.
	uint32_t id;
};

//! Min-heap of `OpenNode` ordered by `f`.
class OpenSet {
	gaia::cnt::darray<OpenNode> m_nodes;

	//! Moves a newly inserted node toward the heap root.
	//! \param i Initial node index.
	void sift_up(uint32_t i);
	//! Moves the heap root toward its ordered position.
	//! \param i Initial node index.
	void sift_down(uint32_t i);

public:
	//! Pushes a node and restores heap order.
	//! \param n Node to insert.
	void push(OpenNode n);

	//! Removes the cheapest node.
	void pop();

	//! Cheapest node. The set must not be empty.
	//! \return Reference to the heap root.
	const OpenNode& top() const;

	//! True when no nodes remain.
	//! \return True when the heap is empty.
	bool empty() const;

	//! Linear membership test used to avoid duplicate open-set entries.
	//! \param id Cell id to look up.
	//! \return True when `id` is already queued.
	bool has_id(uint32_t id) const;
};

//! Four-neighbor A* over the dungeon grid.
class AStar {
	static constexpr uint32_t MAX_NEIGHBORS = 4;

	struct Score {
		float g;
		float f;
	};

public:
	//! Packed walkability for one cell.
	struct Node {
	private:
		uint32_t id;
		uint32_t neighbors : 4;
		uint32_t edge_costs : 8;

	public:
		//! Creates a blocked node with id zero.
		Node();
		//! Creates a blocked node with the supplied cell id.
		//! \param value Cell id.
		explicit Node(uint32_t value);

		//! Records whether neighbor `index` is walkable and what it costs.
		//! \param index 0 north, 1 east, 2 south, 3 west.
		//! \param cost Zero means blocked. Any other value is the edge cost.
		void InitIndex(uint32_t index, uint32_t cost);

		//! Cell id stored in this node.
		//! \return `y * ScreenX + x`.
		uint32_t comp_id() const;

		//! True when neighbor `index` exists.
		//! \param index Direction index.
		//! \return True when that edge is present.
		bool HasNeighbor(uint32_t index) const;

		//! Cost of neighbor `index`.
		//! \param index Direction index.
		//! \return Packed two-bit cost.
		uint32_t GetEdgeCost(uint32_t index) const;

	private:
		//! Sets one neighbor-presence bit.
		//! \param index Direction index.
		//! \param value Whether the neighbor exists.
		void SetNeighbor(uint32_t index, bool value);
		//! Stores one packed edge cost.
		//! \param index Direction index.
		//! \param cost Two-bit edge cost.
		void SetEdgeCost(uint32_t index, uint32_t cost);
	};

	//! Column of a packed cell id.
	//! \param id Cell id.
	//! \return X coordinate.
	static constexpr uint32_t NodeIdToX(uint32_t id) {
		return id % ScreenX;
	}

	//! Row of a packed cell id.
	//! \param id Cell id.
	//! \return Y coordinate.
	static constexpr uint32_t NodeIdToY(uint32_t id) {
		return id / ScreenX;
	}

	//! Packs a cell coordinate.
	//! \param x Column.
	//! \param y Row.
	//! \return Cell id.
	static constexpr uint32_t NodeIdFromXY(uint32_t x, uint32_t y) {
		return y * ScreenX + x;
	}

	//! Euclidean heuristic.
	//! \param current Node being expanded.
	//! \param goal Destination node.
	//! \return Estimated remaining cost.
	float HeuristicCostEstimate(const Node& current, const Node& goal) const;

	//! Returns the cheapest path from `start_id` to `goal_id`, start first.
	//! \param graph One node per map cell.
	//! \param start_id Start cell id.
	//! \param goal_id Goal cell id.
	//! \return Ordered cell ids, or empty when no path exists.
	gaia::cnt::darray<uint32_t> FindPath(const gaia::cnt::darray<Node>& graph, uint32_t start_id, uint32_t goal_id);
};

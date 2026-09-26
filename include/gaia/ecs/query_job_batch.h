#pragma once

#include "gaia/ecs/world.h"

namespace gaia {
	namespace ecs {
		//! Reusable coordinator-owned execution graph. Registration copies callbacks, but retains non-owning query
		//! references. Queries and World must outlive the batch and must not move during its lifetime. All graph access is
		//! coordinator-only. run() prepares each visibility phase only after the preceding phase has joined and applied its
		//! effects. Explicit dependencies take precedence over registration order. Undeclared accesses remain the caller's
		//! responsibility. Registration/scratch capacity is retained across runs. Query::job still owns per-execution
		//! scheduler resources.
		class QueryJobBatch final {
		public:
			//! Outcome of a synchronous graph run. Empty means no registered nodes, not rejected query work.
			enum class Result : uint8_t {
				Completed,
				Empty,
				Busy,
				Cycle,
				WrongWorld,
				UnsupportedScheduler,

				PreparationFailed
			};

			//! Non-owning registration identifier, valid only for its originating batch lifetime.
			struct Handle {
				const QueryJobBatch* owner = nullptr;
				uint32_t index = (uint32_t)-1;
			};

			//! Query preparation rejection from the latest non-busy run.
			//! Status is meaningful only when handle.owner is non-null. Graph and scheduler errors have no query status.
			struct Failure {
				Handle handle{};
				QueryJobStatus status = QueryJobStatus::Ready;
			};

		private:
			//! Type-erased retained callback and non-owning query registration.
			struct Node {
				Query* query;
				void* context;
				SchedJob (*prepare)(void*, Query&, QueryExecType, QueryJobStatus&);
				void (*execute)(void*, Query&);
				void (*destroy)(void*);
				QueryExecType mode;
				uint32_t group;
			};

			//! Directed dependency: visibility edges advance preparation to a later completion boundary.
			struct Edge {
				uint32_t first, second;
				bool visibility;
			};

			World& m_world;
			cnt::darray<Node> m_nodes;
			cnt::darray<Edge> m_edges;
			cnt::darray<Edge> m_runEdges;
			//! Reused adjacency, min-heap and phase buckets. No transitive closure is materialized.
			cnt::darray<uint32_t> m_order, m_indegree, m_phase, m_head, m_next, m_ready;
			cnt::darray<uint32_t> m_phaseHead, m_phaseTail, m_phaseNext;
			cnt::darray<SchedJob> m_jobs;
			uint32_t m_group = 0;
			bool m_running = false;
			Failure m_failure;

			//! Validates a batch-local handle without touching query state.
			//! \param h Handle to inspect.
			//! \return True for a live registration in this batch.
			bool valid(Handle h) const {
				return h.owner == this && h.index < m_nodes.size();
			}

			//! Stores a dependency, rejecting mutation during execution.
			//! \param a Prerequisite registration.
			//! \param b Dependent registration.
			//! \param visibility Whether effects must be applied before dependent preparation.
			//! \return False for invalid handles or an active run. Cycles are diagnosed by run().
			bool edge(Handle a, Handle b, bool visibility) {
				if (m_running || !valid(a) || !valid(b))
					return false;
				for (auto& e: m_edges)
					if (e.first == a.index && e.second == b.index) {
						e.visibility |= visibility;
						return true;
					}
				m_edges.push_back({a.index, b.index, visibility});
				return true;
			}

			//! Builds outgoing edge adjacency in linear time, retaining allocation capacity.
			void adjacency() {
				m_head.resize(m_nodes.size());
				m_next.resize(m_runEdges.size());
				for (auto& head: m_head)
					head = (uint32_t)-1;
				for (uint32_t i = 0; i < m_runEdges.size(); ++i) {
					auto a = m_runEdges[i].first;
					m_next[i] = m_head[a];
					m_head[a] = i;
				}
			}

			//! Produces group-first, registration-stable topological order with a ready min-heap.
			//! \return False for cycles, including backwards crossings of registration barriers.
			bool order() {
				const auto n = (uint32_t)m_nodes.size();
				m_order.clear();
				m_ready.clear();
				m_indegree.resize(n);
				for (auto& degree: m_indegree)
					degree = 0;
				for (const auto& e: m_runEdges) {
					if (m_nodes[e.first].group > m_nodes[e.second].group)
						return false;
					++m_indegree[e.second];
				}

				adjacency();
				auto later = [&](uint32_t a, uint32_t b) {
					if (m_nodes[a].group != m_nodes[b].group)
						return m_nodes[a].group > m_nodes[b].group;
					return a > b;
				};

				for (uint32_t i = 0; i < n; ++i) {
					if (m_indegree[i] == 0) {
						m_ready.push_back(i);
						std::push_heap(m_ready.begin(), m_ready.end(), later);
					}
				}

				while (!m_ready.empty()) {
					std::pop_heap(m_ready.begin(), m_ready.end(), later);
					auto a = m_ready.back();
					m_ready.pop_back();
					m_order.push_back(a);
					for (auto e = m_head[a]; e != (uint32_t)-1; e = m_next[e]) {
						auto b = m_runEdges[e].second;
						if (--m_indegree[b] == 0) {
							m_ready.push_back(b);
							std::push_heap(m_ready.begin(), m_ready.end(), later);
						}
					}
				}

				return m_order.size() == n;
			}

		public:
			//! Creates an unlocked, empty registration graph.
			//! \param world World used by every registered query and its scheduler.
			explicit QueryJobBatch(World& world): m_world(world) {}
			~QueryJobBatch() {
				for (auto& n: m_nodes)
					n.destroy(n.context);
			}

			QueryJobBatch(const QueryJobBatch&) = delete;
			QueryJobBatch(QueryJobBatch&&) = delete;
			QueryJobBatch& operator=(const QueryJobBatch&) = delete;
			QueryJobBatch& operator=(QueryJobBatch&&) = delete;

			//! Copies a callable without preparing/matching its query or locking World.
			//! \tparam Func Copyable query callback type.
			//! \param query Non-owning, stable query reference. Its current configuration is read by each run.
			//! \param func Callback copied into retained registration storage and then into each prepared query job.
			//! \param mode Query job execution mode.
			//! \return Batch-local handle, or an invalid handle if called during run().
			template <typename Func>
			Handle add(Query& query, Func func, QueryExecType mode = QueryExecType::Default) {
				if (m_running)
					return {};

				auto* retained = new Func(GAIA_MOV(func));
				const auto idx = (uint32_t)m_nodes.size();
				m_nodes.push_back(
						{&query, retained,
						 [](void* p, Query& q, QueryExecType m, QueryJobStatus& status) {
							 return q.job(*static_cast<Func*>(p), m, &status);
						 },
						 [](void* p, Query& q) {
							 auto callback = *static_cast<Func*>(p);
							 q.each(callback, QueryExecType::Default);
						 },
						 [](void* p) {
							 delete static_cast<Func*>(p);
						 },
						 mode, m_group});
				return {this, idx};
			}

			//! Requires prerequisite effects (including structural commands and observers) before dependent matching.
			//! \param first Prerequisite handle.
			//! \param second Dependent handle.
			//! \return Whether the edge was recorded.
			bool dep(Handle first, Handle second) {
				return edge(first, second, true);
			}

			//! Orders payload execution only. Both queries may be prepared against the same pre-execution World.
			//! \param first Prerequisite handle.
			//! \param second Dependent handle.
			//! \return Whether the edge was recorded.
			//! \warning Does not make structural commands, observer reactions, or changed-query filters visible to the
			//! successor.
			bool dep_payload(Handle first, Handle second) {
				return edge(first, second, false);
			}

			//! Starts a new registration group whose preparation observes effects of all preceding groups.
			//! \return False during execution. True when the boundary was recorded.
			bool barrier() {
				if (m_running)
					return false;

				++m_group;
				return true;
			}

			//! Returns the latest query preparation rejection without changing the run result.
			//! \return Failure with an invalid handle unless a query registration rejected preparation.
			//! \note Cleared before each non-busy run. Busy attempts preserve the previous or active run diagnostic.
			Failure failure() const {
				return m_failure;
			}

			//! Validates the graph, then owns all preparation, dependency wiring, submission, joins and deletion.
			//! \return Actionable run status. PreparationFailed can occur after earlier phases already completed.
			//! \note Requires add for Default worker nodes and add_par for parallel worker nodes, plus submit/wait/del.
			//! dep is needed only for same-phase edges. Requirements conservatively include empty matching nodes.
			//! Main-thread-only graphs need no scheduler callbacks. Main-thread queries execute on the calling coordinator,
			//! with completion boundaries before and after, regardless of their requested execution mode.
			//! \note Empty matching queries are successful registrations. Callback-local mutable copies reset each run.
			//! \warning Do not call from a scheduler worker or overlap unrelated prepared jobs on this World.
			Result run() {
				if (m_running || m_world.locked() || m_world.m_queryJobBatch != nullptr || m_world.m_deferredQueryCount != 0)
					return Result::Busy;

				m_failure = {};
				if (m_nodes.empty())
					return Result::Empty;

				const auto& s = m_world.sched();

				m_running = true;
				struct RunGuard {
					bool& flag;
					~RunGuard() {
						flag = false;
					}
				} guard{m_running};

				m_runEdges = m_edges;
				const auto n = (uint32_t)m_nodes.size();
				if (!order())
					return Result::Cycle;

				for (uint32_t a = 0; a < n; ++a) {
					auto& node = m_nodes[a];
					if (node.mode != QueryExecType::Default && node.mode != QueryExecType::Parallel &&
							node.mode != QueryExecType::ParallelPerf && node.mode != QueryExecType::ParallelEff) {
						m_failure = {{this, a}, QueryJobStatus::InvalidMode};
						return Result::PreparationFailed;
					}
					if (node.query->fetch().world() != &m_world)
						return Result::WrongWorld;
				}

				// Explicit order determines conflict direction. Different groups already have completion boundaries.
				for (uint32_t i = 0; i < n; ++i) {
					for (uint32_t j = i + 1; j < n; ++j) {
						auto a = m_order[i], b = m_order[j];
						if (m_nodes[a].group != m_nodes[b].group)
							break;
						if (m_nodes[a].query->conflicts_with(*m_nodes[b].query))
							m_runEdges.push_back({a, b, false});
					}
				}

				adjacency();
				m_phase.resize(n);
				m_jobs.resize(n);
				for (auto& phase: m_phase)
					phase = 0;
				uint32_t maxPhase = 0, floor = 0, group = m_nodes[m_order[0]].group;

				// Group bounds replace dense barrier edges. Main-thread nodes occupy an exclusive phase.
				for (auto a: m_order) {
					if (group != m_nodes[a].group) {
						floor = maxPhase + 1;
						group = m_nodes[a].group;
					}

					const bool main = m_nodes[a].query->main_thread_required();
					if (main)
						floor = maxPhase + 1;
					m_phase[a] = core::get_max(m_phase[a], floor);
					maxPhase = core::get_max(maxPhase, m_phase[a]);
					if (main)
						floor = maxPhase + 1;

					for (auto e = m_head[a]; e != (uint32_t)-1; e = m_next[e]) {
						const auto& edge = m_runEdges[e];
						m_phase[edge.second] = core::get_max(m_phase[edge.second], m_phase[a] + (uint32_t)edge.visibility);
					}
				}

				// Check the complete plan before preparing any jobs or consuming changed filters.
				for (const auto& node: m_nodes) {
					if (node.query->main_thread_required())
						continue;
					if (!s.submit || !s.wait || !s.del || (node.mode == QueryExecType::Default ? !s.add : !s.add_par))
						return Result::UnsupportedScheduler;
				}
				if (!s.dep) {
					for (const auto& edge: m_runEdges)
						if (m_phase[edge.first] == m_phase[edge.second])
							return Result::UnsupportedScheduler;
				}

				m_phaseHead.resize(maxPhase + 1);
				m_phaseTail.resize(maxPhase + 1);
				m_phaseNext.resize(n);

				for (auto& head: m_phaseHead)
					head = (uint32_t)-1;

				for (auto a: m_order) {
					auto phase = m_phase[a];
					m_phaseNext[a] = (uint32_t)-1;
					if (m_phaseHead[phase] == (uint32_t)-1)
						m_phaseHead[phase] = a;
					else
						m_phaseNext[m_phaseTail[phase]] = a;
					m_phaseTail[phase] = a;
				}

				for (auto& stamp: m_indegree)
					stamp = (uint32_t)-1;

				for (uint32_t phase = 0; phase <= maxPhase; ++phase) {
					auto first = m_phaseHead[phase];
					if (first == (uint32_t)-1)
						continue;

					if (m_nodes[first].query->main_thread_required()) {
						auto& node = m_nodes[first];
						node.execute(node.context, *node.query);
						continue;
					}

					QueryJobScope scope(m_world);
					bool failed = false;
					for (auto a = first; a != (uint32_t)-1; a = m_phaseNext[a]) {
						auto& node = m_nodes[a];
						QueryJobStatus status;
						m_jobs[a] = node.prepare(node.context, *node.query, node.mode, status);
						if (!m_jobs[a].valid()) {
							if (status != QueryJobStatus::Empty) {
								m_failure = {{this, a}, status};
								failed = true;
								break;
							}

							// Empty matches retain a lightweight scheduler junction only when they have successors.
							// This preserves arbitrary sparse dependency chains without dense transitive closure.
							bool junction = false;
							for (auto e = m_head[a]; e != (uint32_t)-1; e = m_next[e])
								junction |= m_phase[m_runEdges[e].second] == phase;

							if (junction) {
								if (s.add != nullptr) {
									SchedTaskDesc desc{};
									desc.invoke = [](void*) {};
									m_jobs[a] = SchedJob(s, s.add(s.pCtx, &desc), false, nullptr, nullptr);
								} else {
									SchedParDesc desc{};
									desc.invoke = [](void*, uint32_t, uint32_t) {};
									desc.itemCount = 1;
									desc.groupSize = 1;
									desc.execType = node.mode;
									m_jobs[a] = SchedJob(s, s.add_par(s.pCtx, &desc), false, nullptr, nullptr);
								}
							}
						}
					}

					if (!failed) {
						// Visit each adjacency edge once and stamp prerequisites to deduplicate explicit/conflict links.
						for (auto a = first; a != (uint32_t)-1; a = m_phaseNext[a]) {
							for (auto e = m_head[a]; e != (uint32_t)-1; e = m_next[e]) {
								auto b = m_runEdges[e].second;
								if (m_phase[b] == phase && m_jobs[a].valid() && m_jobs[b].valid() && m_indegree[b] != a) {
									m_jobs[b].dep(m_jobs[a]);
									m_indegree[b] = a;
								}
							}
						}

						for (auto a = first; a != (uint32_t)-1; a = m_phaseNext[a])
							m_jobs[a].submit();
						for (auto a = first; a != (uint32_t)-1; a = m_phaseNext[a])
							m_jobs[a].wait();
					}

					for (auto a = first; a != (uint32_t)-1; a = m_phaseNext[a])
						m_jobs[a].del();

					scope.finish();
					if (failed)
						return Result::PreparationFailed;
				}

				return Result::Completed;
			}
		};
	} // namespace ecs
} // namespace gaia

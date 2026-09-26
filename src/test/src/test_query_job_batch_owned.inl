//! Reusable owning query execution graph regressions.
namespace {
	struct OwnedBatchValue {
		int value;
	};
	struct OwnedBatchTag {};

	//! Delegates real work to Gaia while checking coordinator preparation and owned-token cleanup.
	struct OwnedBatchSchedProbe {
		ecs::Sched base = ecs::sched_def();
		unsigned added = 0, submitted = 0, waited = 0, deleted = 0, deps = 0, active = 0;
		unsigned firstSubmitAdds = 0;
		ecs::Sched descriptor() {
			auto s = base;
			s.pCtx = this;
			s.add = [](void* p, const ecs::SchedTaskDesc* d) {
				auto& x = *(OwnedBatchSchedProbe*)p;
				CHECK(x.active == 0);
				++x.added;
				return x.base.add(x.base.pCtx, d);
			};
			s.add_par = [](void* p, const ecs::SchedParDesc* d) {
				auto& x = *(OwnedBatchSchedProbe*)p;
				CHECK(x.active == 0);
				++x.added;
				return x.base.add_par(x.base.pCtx, d);
			};
			s.submit = [](void* p, ecs::SchedToken t) {
				auto& x = *(OwnedBatchSchedProbe*)p;
				if (!x.submitted)
					x.firstSubmitAdds = x.added;
				++x.submitted;
				++x.active;
				x.base.submit(x.base.pCtx, t);
			};
			s.wait = [](void* p, ecs::SchedToken t) {
				auto& x = *(OwnedBatchSchedProbe*)p;
				x.base.wait(x.base.pCtx, t);
				++x.waited;
				--x.active;
			};
			s.del = [](void* p, ecs::SchedToken t) {
				auto& x = *(OwnedBatchSchedProbe*)p;
				++x.deleted;
				x.base.del(x.base.pCtx, t);
			};
			s.dep = [](void* p, ecs::SchedToken a, ecs::SchedToken b) {
				auto& x = *(OwnedBatchSchedProbe*)p;
				CHECK(x.active == 0);
				++x.deps;
				x.base.dep(x.base.pCtx, a, b);
			};
			return s;
		}
	};
} // namespace

TEST_CASE("Query owned batch - scheduler phase lifecycle") {
	ecs::World w;
	OwnedBatchSchedProbe probe;
	w.set_sched(probe.descriptor());
	auto e = w.add();
	w.add<OwnedBatchValue>(e, {0});
	auto q = w.query().all<OwnedBatchValue&>();
	ecs::QueryJobBatch batch(w);
	auto a = batch.add(
			q,
			[](OwnedBatchValue& v) {
				v.value = 2;
			},
			ecs::QueryExecType::Parallel);
	auto b = batch.add(
			q,
			[](OwnedBatchValue& v) {
				v.value *= 3;
			},
			ecs::QueryExecType::Parallel);
	auto c = batch.add(
			q,
			[](OwnedBatchValue& v) {
				++v.value;
			},
			ecs::QueryExecType::Parallel);
	CHECK(batch.dep_payload(a, b));
	CHECK(batch.dep(b, c));
	CHECK(probe.added == 0);
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	CHECK(w.get<OwnedBatchValue>(e).value == 7);
	CHECK(probe.firstSubmitAdds == 2);
	CHECK(probe.added == 3);
	CHECK(probe.submitted == 3);
	CHECK(probe.waited == 3);
	CHECK(probe.deleted == 3);
	CHECK(probe.deps == 1);
	CHECK(probe.active == 0);
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	CHECK(probe.added == 6);
	CHECK(probe.deleted == 6);
	w.reset_sched();
}

TEST_CASE("Query owned batch - late matching and reuse") {
	ecs::World w;
	auto q = w.query().all<OwnedBatchValue&>();
	ecs::QueryJobBatch batch(w);
	batch.add(q, [](OwnedBatchValue& v) {
		++v.value;
	});
	auto e = w.add();
	w.add<OwnedBatchValue>(e, {1});
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	CHECK(w.get<OwnedBatchValue>(e).value == 2);
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	CHECK(w.get<OwnedBatchValue>(e).value == 3);
}

TEST_CASE("Query owned batch - effects dependency late matching") {
	ecs::World w;
	w.add<OwnedBatchTag>();
	auto e = w.add();
	w.add<OwnedBatchValue>(e, {1});
	auto first = w.query().all<const OwnedBatchValue>();
	auto second = w.query().all<OwnedBatchTag>();
	ecs::QueryJobBatch batch(w);
	unsigned seen = 0;
	auto a = batch.add(first, [&](ecs::Iter& it) {
		it.cmd_buffer_st().add<OwnedBatchTag>(e);
	});
	auto b = batch.add(second, [&](ecs::Iter& it) {
		seen += it.size();
	});
	CHECK(batch.dep(a, b));
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	CHECK(seen == 1);
}

TEST_CASE("Query owned batch - cycles reject before callbacks") {
	ecs::World w;
	auto e = w.add();
	w.add<OwnedBatchValue>(e, {0});
	auto q = w.query().all<const OwnedBatchValue>();
	ecs::QueryJobBatch batch(w);
	int calls = 0;
	auto a = batch.add(q, [&](const OwnedBatchValue&) {
		++calls;
	});
	auto b = batch.add(q, [&](const OwnedBatchValue&) {
		++calls;
	});
	CHECK(batch.dep(a, b));
	CHECK(batch.dep(b, a));
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Cycle);
	CHECK(calls == 0);
}

TEST_CASE("Query owned batch - payload conflicts and barriers") {
	ecs::World w;
	auto e = w.add();
	w.add<OwnedBatchValue>(e, {0});
	auto q = w.query().all<OwnedBatchValue&>();
	ecs::QueryJobBatch batch(w);
	batch.add(q, [](OwnedBatchValue& v) {
		v.value = 3;
	});
	batch.add(q, [](OwnedBatchValue& v) {
		v.value *= 2;
	});
	CHECK(batch.barrier());
	batch.add(q, [](OwnedBatchValue& v) {
		++v.value;
	});
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	CHECK(w.get<OwnedBatchValue>(e).value == 7);
}

TEST_CASE("Query owned batch - payload edge does not imply structural visibility") {
	ecs::World w;
	w.add<OwnedBatchTag>();
	auto e = w.add();
	w.add<OwnedBatchValue>(e, {0});
	auto q = w.query().all<const OwnedBatchValue>();
	auto tagged = w.query().all<OwnedBatchTag>();
	ecs::QueryJobBatch batch(w);
	unsigned seen = 0;
	auto a = batch.add(q, [&](ecs::Iter& it) {
		it.cmd_buffer_st().add<OwnedBatchTag>(e);
	});
	auto b = batch.add(tagged, [&](ecs::Iter& it) {
		seen += it.size();
	});
	CHECK(batch.dep_payload(a, b));
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	CHECK(seen == 0);
	CHECK(w.has<OwnedBatchTag>(e));
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	CHECK(seen == 1);
}

TEST_CASE("Query owned batch - reverse payload chain through empty match") {
	ecs::World w;
	auto e = w.add();
	w.add<OwnedBatchValue>(e, {0});
	auto q = w.query().all<const OwnedBatchValue>();
	auto empty = w.query().all<OwnedBatchTag>();
	ecs::QueryJobBatch batch(w);
	std::atomic_int value{0};
	auto last = batch.add(
			q,
			[&](const OwnedBatchValue&) {
				value.store(value.load() + 1);
			},
			ecs::QueryExecType::Parallel);
	auto middle = batch.add(empty, [](ecs::Iter&) {});
	auto first = batch.add(
			q,
			[&](const OwnedBatchValue&) {
				value.store(7);
			},
			ecs::QueryExecType::Parallel);
	CHECK(batch.dep_payload(first, middle));
	CHECK(batch.dep_payload(middle, last));
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	CHECK(value.load() == 8);
}

TEST_CASE("Query owned batch - statuses and immutable active registrations") {
	ecs::World w;
	ecs::QueryJobBatch batch(w), other(w);
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Empty);
	auto e = w.add();
	w.add<OwnedBatchValue>(e, {0});
	auto q = w.query().all<const OwnedBatchValue>();
	auto a = batch.add(q, [&](const OwnedBatchValue&) {
		CHECK(batch.run() == ecs::QueryJobBatch::Result::Busy);
		CHECK_FALSE(batch.barrier());
		CHECK(batch.add(q, [](ecs::Iter&) {}).owner == nullptr);
	});
	CHECK_FALSE(other.dep(a, a));
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	ecs::QueryJobBatch wrong(w);
	ecs::World w2;
	auto foreign = w2.query().all<OwnedBatchValue>();
	wrong.add(foreign, [](ecs::Iter&) {});
	CHECK(wrong.run() == ecs::QueryJobBatch::Result::WrongWorld);
	q.main_thread();
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	q.main_thread(false);
	auto sched = w.sched();
	ecs::Sched incomplete = sched;
	incomplete.submit = nullptr;
	w.set_sched(incomplete);
	CHECK(batch.run() == ecs::QueryJobBatch::Result::UnsupportedScheduler);
	w.set_sched(sched);
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
}

TEST_CASE("Query owned batch - main thread completion boundaries") {
	ecs::World w;
	OwnedBatchSchedProbe probe;
	w.set_sched(probe.descriptor());
	w.add<OwnedBatchTag>();
	auto e = w.add();
	w.add<OwnedBatchValue>(e, {0});
	auto q = w.query().all<const OwnedBatchValue>();
	auto main = w.query().all<const OwnedBatchValue>().main_thread();
	auto tagged = w.query().all<OwnedBatchTag>();
	ecs::QueryJobBatch batch(w);
	const auto coordinator = std::this_thread::get_id();
	std::atomic_int value{0};
	auto a = batch.add(
			q,
			[&](const OwnedBatchValue&) {
				value.store(7);
			},
			ecs::QueryExecType::Parallel);
	auto b = batch.add(
			main,
			[&](ecs::Iter& it) {
				CHECK(std::this_thread::get_id() == coordinator);
				CHECK(probe.active == 0);
				CHECK(value.load() == 7);
				CHECK(w.locked());
				it.cmd_buffer_st().add<OwnedBatchTag>(e);
			},
			ecs::QueryExecType::Parallel);
	auto c = batch.add(tagged, [&](ecs::Iter& it) {
		value.fetch_add((int)it.size());
	});
	CHECK(batch.dep_payload(a, b));
	CHECK(batch.dep_payload(b, c));
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	CHECK(value.load() == 8);
	CHECK(probe.added == 2);
	CHECK(probe.deleted == 2);
	w.reset_sched();
}

TEST_CASE("Query owned batch - large sparse reverse chains and empty junctions") {
	ecs::World w;
	auto e = w.add();
	w.add<OwnedBatchValue>(e, {0});
	auto q = w.query().all<const OwnedBatchValue>();
	auto empty = w.query().all<OwnedBatchTag>();
	ecs::QueryJobBatch batch(w);
	cnt::darray<ecs::QueryJobBatch::Handle> handles;
	std::atomic_int calls{0};
	for (uint32_t i = 0; i < 1024; ++i) {
		auto& query = i % 3 == 1 ? empty : q;
		handles.push_back(batch.add(query, [&, i](ecs::Iter&) {
			const int expected = (int)((1023 - i) - (1023 - i + 1) / 3);
			CHECK(calls.fetch_add(1) == expected);
		}));
	}
	for (uint32_t i = 1; i < handles.size(); ++i)
		CHECK(batch.dep_payload(handles[i], handles[i - 1]));
	for (int run = 0; run < 2; ++run) {
		calls.store(0);
		CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
		CHECK(calls.load() == 683);
	}
}

TEST_CASE("Query owned batch - sparse barriers and backwards crossing") {
	ecs::World w;
	auto q = w.query().all<OwnedBatchTag>();
	ecs::QueryJobBatch batch(w);
	auto first = batch.add(q, [](ecs::Iter&) {});
	for (uint32_t i = 1; i < 1024; ++i) {
		CHECK(batch.barrier());
		batch.add(q, [](ecs::Iter&) {});
	}
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	auto last = batch.add(q, [](ecs::Iter&) {});
	CHECK(batch.dep_payload(last, first));
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Cycle);
}

TEST_CASE("Query owned batch - minimal schedulers and phase edges") {
	for (auto mode: {ecs::QueryExecType::Default, ecs::QueryExecType::Parallel}) {
		for (int edge = 0; edge < 3; ++edge) {
			ecs::World w;
			OwnedBatchSchedProbe probe;
			auto s = probe.descriptor();
			s.sched = nullptr;
			s.sched_par = nullptr;
			s.dep = nullptr;
			if (mode == ecs::QueryExecType::Default)
				s.add_par = nullptr;
			else
				s.add = nullptr;
			w.set_sched(s);
			auto e = w.add();
			w.add<OwnedBatchValue>(e, {0});
			auto q = w.query().all<const OwnedBatchValue>();
			ecs::QueryJobBatch batch(w);
			std::atomic_int calls{0};
			auto a = batch.add(
					q,
					[&](const OwnedBatchValue&) {
						++calls;
					},
					mode);
			auto b = batch.add(
					q,
					[&](const OwnedBatchValue&) {
						++calls;
					},
					mode);
			if (edge == 1)
				CHECK(batch.dep(a, b));
			if (edge == 2)
				CHECK(batch.dep_payload(a, b));
			if (edge == 2) {
				CHECK(batch.run() == ecs::QueryJobBatch::Result::UnsupportedScheduler);
				CHECK(calls.load() == 0);
				CHECK(probe.added == 0);
				CHECK(probe.submitted == 0);
			} else {
				CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
				CHECK(calls.load() == 2);
				CHECK(probe.added == 2);
				CHECK(probe.deleted == 2);
			}
			w.reset_sched();
		}
	}
}

TEST_CASE("Query owned batch - main thread needs no deferred callbacks") {
	ecs::World w;
	OwnedBatchSchedProbe probe;
	auto s = probe.descriptor();
	s.add = nullptr;
	s.add_par = nullptr;
	s.submit = nullptr;
	s.dep = nullptr;
	s.wait = nullptr;
	s.del = nullptr;
	w.set_sched(s);
	auto e = w.add();
	w.add<OwnedBatchValue>(e, {0});
	auto q = w.query().all<OwnedBatchValue&>().main_thread();
	ecs::QueryJobBatch batch(w);
	auto a = batch.add(
			q,
			[](OwnedBatchValue& v) {
				++v.value;
			},
			ecs::QueryExecType::Parallel);
	auto b = batch.add(q, [](OwnedBatchValue& v) {
		++v.value;
	});
	CHECK(batch.dep_payload(a, b));
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	CHECK(w.get<OwnedBatchValue>(e).value == 2);
	CHECK(probe.added == 0);
	w.reset_sched();
}

TEST_CASE("Query owned batch - parallel only empty junction") {
	ecs::World w;
	OwnedBatchSchedProbe probe;
	auto s = probe.descriptor();
	s.add = nullptr;
	s.sched = nullptr;
	s.sched_par = nullptr;
	w.set_sched(s);
	auto e = w.add();
	w.add<OwnedBatchValue>(e, {0});
	auto q = w.query().all<const OwnedBatchValue>();
	auto empty = w.query().all<OwnedBatchTag>();
	ecs::QueryJobBatch batch(w);
	std::atomic_int value{0};
	auto c = batch.add(
			q,
			[&](const OwnedBatchValue&) {
				CHECK(value.load() == 7);
				++value;
			},
			ecs::QueryExecType::Parallel);
	auto b = batch.add(empty, [](ecs::Iter&) {}, ecs::QueryExecType::Parallel);
	auto a = batch.add(
			q,
			[&](const OwnedBatchValue&) {
				value.store(7);
			},
			ecs::QueryExecType::Parallel);
	CHECK(batch.dep_payload(a, b));
	CHECK(batch.dep_payload(b, c));
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	CHECK(value.load() == 8);
	CHECK(probe.added == 3);
	CHECK(probe.deleted == 3);
	CHECK(probe.deps == 2);
	w.reset_sched();
}

TEST_CASE("Query owned batch - failure diagnostics and partial completion retry") {
	ecs::World w;
	OwnedBatchSchedProbe probe;
	w.set_sched(probe.descriptor());
	auto component = w.add<OwnedBatchValue>().entity;
	w.add(component, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	auto base = w.prefab();
	w.add<OwnedBatchValue>(base, {4});
	auto child = w.instantiate(base);
	auto read = w.query().all<const OwnedBatchValue>();
	auto write = w.query().all<OwnedBatchValue&>();
	ecs::QueryJobBatch batch(w);
	std::atomic_int completed{0};
	batch.add(read, [&](const OwnedBatchValue&) {
		++completed;
	});
	CHECK(batch.barrier());
	batch.add(read, [](const OwnedBatchValue&) {});
	auto failed = batch.add(write, [](OwnedBatchValue& v) {
		++v.value;
	});
	CHECK(batch.failure().handle.owner == nullptr);
	CHECK(batch.run() == ecs::QueryJobBatch::Result::PreparationFailed);
	CHECK(batch.failure().handle.owner == failed.owner);
	CHECK(batch.failure().handle.index == failed.index);
	CHECK(batch.failure().status == ecs::QueryJobStatus::MissingOverride);
	CHECK(completed.load() == 1);
	CHECK(probe.added == 2);
	CHECK(probe.deleted == 2);
	CHECK(probe.submitted == 1);
	CHECK_FALSE(w.locked());
	// A busy attempt must not erase the previous failure.
	auto pending = read.job([](const OwnedBatchValue&) {});
	CHECK(pending.valid());
	if (pending.valid()) {
		CHECK(batch.run() == ecs::QueryJobBatch::Result::Busy);
		CHECK(batch.failure().handle.index == failed.index);
		CHECK(batch.failure().status == ecs::QueryJobStatus::MissingOverride);
		pending.del();
	}
	w.override<OwnedBatchValue>(child);
	CHECK(batch.run() == ecs::QueryJobBatch::Result::Completed);
	CHECK(batch.failure().handle.owner == nullptr);
	CHECK(batch.failure().handle.index == (uint32_t)-1);
	CHECK(completed.load() == 2);
	CHECK(w.get<OwnedBatchValue>(child).value == 5);
	w.reset_sched();
}

TEST_CASE("Query owned batch - invalid modes diagnosed before any phase") {
	for (bool main: {false, true}) {
		ecs::World w;
		OwnedBatchSchedProbe probe;
		w.set_sched(probe.descriptor());
		auto e = w.add();
		w.add<OwnedBatchValue>(e, {0});
		auto q = w.query().all<OwnedBatchValue&>();
		auto bad = w.query().all<OwnedBatchTag>().main_thread(main);
		ecs::QueryJobBatch batch(w);
		batch.add(q, [](OwnedBatchValue& v) {
			++v.value;
		});
		CHECK(batch.barrier());
		auto failed = batch.add(bad, [](ecs::Iter&) {}, (ecs::QueryExecType)255);
		CHECK(batch.run() == ecs::QueryJobBatch::Result::PreparationFailed);
		CHECK(batch.failure().handle.owner == failed.owner);
		CHECK(batch.failure().handle.index == failed.index);
		CHECK(batch.failure().status == ecs::QueryJobStatus::InvalidMode);
		CHECK(w.get<OwnedBatchValue>(e).value == 0);
		CHECK(probe.added == 0);
		w.reset_sched();
	}
}

TEST_CASE("Query owned batch - capability rejection preserves all phases and changed filters") {
	for (int missing = 0; missing < 6; ++missing) {
		ecs::World w;
		OwnedBatchSchedProbe probe;
		auto s = probe.descriptor();
		if (missing == 0)
			s.add = nullptr;
		if (missing == 1)
			s.add_par = nullptr;
		if (missing == 2)
			s.submit = nullptr;
		if (missing == 3)
			s.wait = nullptr;
		if (missing == 4)
			s.del = nullptr;
		if (missing == 5)
			s.dep = nullptr;
		w.set_sched(s);
		auto e = w.add();
		w.add<OwnedBatchValue>(e, {0});
		auto main = w.query().all<const OwnedBatchValue>().main_thread();
		auto changed = w.query().all<const OwnedBatchValue>().changed<OwnedBatchValue>();
		auto writer = w.query().all<OwnedBatchValue&>();
		ecs::QueryJobBatch batch(w);
		int mainCalls = 0;
		batch.add(main, [&](const OwnedBatchValue&) {
			++mainCalls;
		});
		batch.add(changed, [](const OwnedBatchValue&) {});
		auto mode = missing == 1 ? ecs::QueryExecType::Parallel : ecs::QueryExecType::Default;
		batch.add(
				writer,
				[](OwnedBatchValue& v) {
					++v.value;
				},
				mode);
		// The inferred read/write conflict requires dep within the worker phase.
		CHECK(batch.run() == ecs::QueryJobBatch::Result::UnsupportedScheduler);
		CHECK(batch.failure().handle.owner == nullptr);
		CHECK(mainCalls == 0);
		CHECK(probe.added == 0);
		CHECK(probe.submitted == 0);
		CHECK(probe.waited == 0);
		CHECK(probe.deleted == 0);
		CHECK(probe.deps == 0);
		int seen = 0;
		changed.each([&](const OwnedBatchValue&) {
			++seen;
		});
		CHECK(seen == 1);
		w.reset_sched();
	}
}

TEST_CASE("Query owned batch - invalid mode is not an empty match") {
	ecs::World w;
	auto empty = w.query().all<OwnedBatchTag>();
	ecs::QueryJobBatch batch(w);
	batch.add(empty, [](ecs::Iter&) {}, (ecs::QueryExecType)255);
	CHECK(batch.run() == ecs::QueryJobBatch::Result::PreparationFailed);
	CHECK_FALSE(w.locked());
}

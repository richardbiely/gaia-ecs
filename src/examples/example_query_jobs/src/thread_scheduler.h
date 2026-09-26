#pragma once

#include <gaia/ecs/sched.h>
#include <thread>

//! Minimal external scheduler: one allocation and one OS thread per submitted task.
//! Demonstration only, not a pool or performance recommendation. Callbacks must not throw.
//! Only Default tasks separated by batch visibility boundaries are supported. There is no
//! add_par or same-phase dep support. Lifecycle callbacks run on the coordinator only.
struct ThreadScheduler {
	//! Copied task description and scheduler-owned thread, retained until del().
	struct Task {
		gaia::ecs::SchedTaskDesc desc;
		std::thread worker;
	};

	unsigned submitted = 0;
	unsigned live = 0;

	//! Decodes this adapter's opaque token.
	//! \param token Token previously returned by add.
	//! \return Scheduler-owned task.
	static Task& task(gaia::ecs::SchedToken token) {
		return *reinterpret_cast<Task*>(token.value[0]);
	}

	//! Creates the non-owning scheduler descriptor. This object must outlive its World.
	//! \return Descriptor with only add, submit, wait and del installed.
	gaia::ecs::Sched descriptor() {
		gaia::ecs::Sched sched{};
		sched.pCtx = this;
		sched.add = [](void* ctx, const gaia::ecs::SchedTaskDesc* desc) {
			auto* t = new Task{*desc, {}};
			++static_cast<ThreadScheduler*>(ctx)->live;
			return gaia::ecs::SchedToken{{reinterpret_cast<uintptr_t>(t), 0}};
		};
		sched.submit = [](void* ctx, gaia::ecs::SchedToken token) {
			auto* t = &task(token);
			t->worker = std::thread([t] {
				t->desc.invoke(t->desc.pCtx);
			});
			++static_cast<ThreadScheduler*>(ctx)->submitted;
		};
		sched.wait = [](void*, gaia::ecs::SchedToken token) {
			auto& worker = task(token).worker;
			if (worker.joinable())
				worker.join();
		};
		sched.del = [](void* ctx, gaia::ecs::SchedToken token) {
			// Also accepts an unsubmitted task, whose thread is not joinable.
			auto* t = &task(token);
			if (t->worker.joinable())
				t->worker.join();
			delete t;
			--static_cast<ThreadScheduler*>(ctx)->live;
		};
		return sched;
	}
};

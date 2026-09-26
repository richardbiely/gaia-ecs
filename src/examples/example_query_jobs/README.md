# Query execution without manual job management

The example uses three application operations:

- `each(..., QueryExecType::Parallel)` for one query. It waits before returning.
- `batch.add(query, callback)` to register work.
- `batch.dep(first, second)` and `batch.run()` to execute dependent queries.

There are no `SchedJob` handles or `QueryJobScope` objects in the application flow.

## What runs

1. Gaia's built-in scheduler increments every entity through parallel `each()`.
2. A batch's first query records `Ready` additions for selected entities.
3. Its second query sees those additions and updates only the selected entities.
4. The same registrations run again after removing `Ready`, verifying fresh matching.
5. The entire workload repeats with an external scheduler installed for the batch.

The executable checks every entity's membership and value, compares both scheduler outcomes, verifies cycle rejection and checks that the external scheduler releases all task tokens. Validation uses ordinary conditions, so it still runs in Release builds.

Read [src/main.cpp](src/main.cpp) for the application flow. Queries are declared before the batch so they remain alive and at stable addresses until the batch is destroyed. Component types used by worker commands are registered during setup. Use `dep()` when a later query must see the earlier query's changes.

## External scheduler

[src/thread_scheduler.h](src/thread_scheduler.h) contains the adapter. Application registration and execution are unchanged after `world.set_sched(...)`.

This intentionally small adapter starts one OS thread per task. It is a working integration example, not a production thread pool or a performance recommendation. It supports the Default tasks used here through `add`, `submit`, `wait` and `del`. The batch handles the dependency's completion boundary on the coordinator, so this example does not require the scheduler's `dep` callback.

An adapter supporting parallel-for registrations also needs `add_par`. Dependencies within one execution phase need `dep`. The initial parallel `each()` demonstration always uses Gaia's built-in scheduler, before installing this minimal adapter.

## Build and run

From the repository root:

```sh
cmake -S . -B build/query-jobs -DCMAKE_BUILD_TYPE=Debug -DGAIA_BUILD_EXAMPLES=ON
cmake --build build/query-jobs --target gaia_example_query_jobs
ctest --test-dir build/query-jobs -R '^gaia_example_query_jobs$' --output-on-failure
```

Use a separate build directory with `-DCMAKE_BUILD_TYPE=Release` for an optimized build. For multi-configuration generators, select the configuration with `--config Debug` when building and `-C Debug` when running CTest.

Success ends with `PASS: identical outcomes`, a count of four external tasks and zero live tokens. The adapter stays separate from the query code so adopting a different scheduler does not require rewriting the workload.

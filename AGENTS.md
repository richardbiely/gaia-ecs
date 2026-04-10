# AGENTS.md

## Purpose

This repository is Gaia-ECS. All changes, examples, benchmarks, tests, documentation, and explanations must stay grounded in Gaia-ECS itself. Do not drift into generic ECS patterns or assumptions.

## Core rules

- Use Gaia-ECS-native terminology, APIs, and constraints.
- Do not substitute patterns from other ECS libraries.
- Keep performance-critical paths simple, explicit, and measurable.
- Avoid speculative claims. Everything performance-related must be verified.
- Compilation time and runtime must both stay fast.

## Container policy

- Do not use C++ standard library containers (e.g. `std::vector`, `std::unordered_map`, `std::array`) or `std::function`.
- Use Gaia-ECS-specific containers or custom data structures aligned with existing patterns and performance constraints.

## Determinism

- Systems must produce deterministic results given the same inputs unless explicitly documented otherwise.
- Do not introduce hidden nondeterminism such as unordered iteration, race conditions, or time-dependent logic.

## Memory

- Avoid dynamic allocations in hot paths.
- Prefer preallocation, pooling, or arena-style allocation patterns used in Gaia-ECS.
- Any new allocation pattern must be justified and measured.

## Structural changes

- Minimize structural changes during iteration such as entity/component add/remove.
- If unavoidable, document the cost and ensure iteration safety is preserved.

## Threading

- No parallel writes to the same component.
- Read-only access may run in parallel.
- Do not introduce synchronization or contention without measurement.

## Build and runtime performance

- Compilation time must stay fast. Do not introduce template-heavy, macro-heavy, or abstraction-heavy code without strong justification.
- Runtime must stay fast. Prefer predictable, low-overhead code in hot paths.
- Changes must consider both compile-time cost and runtime cost, not just one of them.

## Testing requirements

- Unit tests must run in both optimized and unoptimized builds unless explicitly stated otherwise.
- New behavior should be covered by tests where practical.
- Bug fixes should include regression tests when feasible.

## Performance requirements

- Performance tests must be run in optimized builds.
- Do not use debug or otherwise unoptimized results as performance evidence.
- Any claimed performance improvement must include an A/B comparison against a relevant baseline.

## A/B comparison requirements

- State what changed.
- State how it was measured.
- State the test configuration.
- Provide before/after results.

## Benchmarking

- Benchmarks must isolate the change being measured.
- Do not combine multiple changes in one comparison.
- Include warm-up and stabilization runs.

## API discipline

- Do not introduce abstractions that hide cost.
- Public APIs must make performance characteristics explicit.

## Comments and documentation

- Code comments must use Doxygen format.
- Public APIs, important internals, and non-obvious behavior must be documented clearly.
- The README must be updated whenever changes affect usage, behavior, architecture, build steps, performance characteristics, or developer workflow.

## Debug vs Release

- Debug-only checks must not affect optimized builds.
- Assertions should catch misuse without impacting release performance.

## Reporting expectations

- Separate correctness claims from performance claims.
- If results are noisy or incomplete, state that clearly.
- Prefer reproducible benchmark setups over anecdotal observations.

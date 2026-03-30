# AGENTS.md

## Purpose

This repository is Gaia-ECS. All changes, examples, benchmarks, tests, and explanations must stay grounded in Gaia-ECS itself and must not drift into generic ECS advice.

## Core rules

- Prefer Gaia-ECS-native terminology, APIs, data flow, and constraints.
- Do not substitute patterns or assumptions from other ECS libraries.
- Keep performance-sensitive code paths simple, measurable, and explicit.
- When discussing internals, be precise and avoid hand-wavy claims.

## Implementation guidance

- Match the existing repository style and architecture.
- Favor changes that preserve cache efficiency, archetype/chunk locality, and low-overhead iteration.
- Be careful with synchronization, scheduling, invalidation, and structural changes that can affect determinism or throughput.
- Do not present speculative optimizations as facts without measurement.

## Testing requirements

- Unit tests must run in both optimized and unoptimized builds unless a task explicitly states otherwise.
- New behavior should be covered by tests where practical.
- Bug fixes should include or update a regression test when feasible.

## Performance requirements

- Performance tests must be run in optimized builds.
- Do not use debug or otherwise unoptimized benchmark results as evidence for performance claims.
- When performance improvements are claimed, provide an A/B comparison against a relevant baseline.
- The A/B comparison should state:
  - what changed,
  - how it was measured,
  - the test configuration,
  - and the before/after results.

## Reporting expectations

- Separate correctness claims from performance claims.
- If measurement is incomplete or noisy, say so clearly.
- Prefer reproducible benchmark setups over anecdotal observations.

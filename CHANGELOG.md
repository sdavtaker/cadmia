# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.4.3] - 2026-06-19

### Added
- `interval<T>::operator==` — structural equality (same lower, upper, closure flags)
- `IADEVSEngine<T>::engine_equals()` — pure-virtual structural equality check;
  implemented by `simulator<M>` (compares `state_`, `t_last_`, `t_next_`) and
  `coordinator<T>` (recurses over child engines by name)
- `LogEntry<T>::merged_into` — optional field set only for `kind="dedup"` entries,
  carrying the branch id that subsumes the deduplicated branch's future
- `to_ndjson`: emits `merged_into` when present

### Changed
- `RootCoordinator::simulate()`: at each BFS step, scans the queue and erases any
  entry whose coordinator state is identical to the head, emitting a `"dedup"`
  `LogEntry` recording the merge. Prevents exponential branch blowup when multiple
  simulation paths converge to the same state (e.g. two uncoupled generators with
  the same interval period).

## [0.4.2] - 2026-06-07

### Fixed
- `simulator::advance_t_next_past_limit()`: for a punctual limit `[L, L]`, the
  function did nothing when `t_next.lower < L`, leaving the deferred engine's
  `t_next` unchanged and still including `L`.  Now unconditionally sets
  `t_next.lower = L` (open) for any punctual limit, matching Algorithm 3 line 15
  of VWD21 (`E_next.t_next = E_next.t_next − limit`).

## [0.4.1] - 2026-06-07

### Fixed
- `simulator::x()`: elapsed time upper bound used `t_next_.lower` instead of
  `t_last_.lower`, producing a negative upper when a model received a second external
  event at a time interval overlapping its current `t_last`.  Now correctly computes
  `t.upper - t_last_.lower` with closedness `t.upper_closed && t_last_.lower_closed`,
  matching Algorithm 1 of the IA-DEVS specification.

## [0.4.0] - 2026-06-03

### Added
- `LogEntry<T>::raw_output` (`std::optional<std::any>`) — typed output value preserved
  alongside the existing string field so callers can `any_cast` the component output.

### Changed
- `interval<T>::operator+` and `operator-` apply directed rounding for
  `std::floating_point<T>`: lower bound computed with `FE_DOWNWARD`, upper bound with
  `FE_UPWARD`, restoring `FE_TONEAREST` after each operation.  Non-floating-point types
  use the unchanged constexpr path.  Translation units that use `interval<float>` or
  `interval<double>` arithmetic must be compiled with
  `-frounding-math -fno-unsafe-math-optimizations` for the rounding to be respected.

### Removed
- `cadmia::modeling::decimal<N>` — moved to the `cdcommons` library as
  `cdcommons::time::decimal<Scale>`, where it gains infinity sentinels and integrates
  with the full cdcommons time-type family.  Update includes from
  `<cadmia/modeling/decimal.hpp>` to `<cdcommons/time/decimal.hpp>` and the namespace
  from `cadmia::modeling::decimal` to `cdcommons::time::decimal`.

## [0.3.0] - 2026-05-02

First release with full coupled model simulation support: coordinator, root
coordinator, BFS branching, and the accumulator basic model.

### Added
- `CoupledModel<T>` with component map, influencer map, typed translation map, and SELECT function
- `coordinator<T>` implementing the DEVS abstract simulator protocol for coupled models
- `RootCoordinator<T>` driving simulation with BFS branching over non-punctual event intervals
- `make_translation<SrcOutput, DstInput>` and `identity_translation()` connection helpers
- `accumulator` basic IA-DEVS model (running sum with passive/active phases)
- `generator` basic IA-DEVS model (periodic output source)
- `coupled_gp` example: generator → accumulator validating the full simulation stack
- Unit tests: coordinator, root coordinator, accumulator model, coupled model

### Fixed
- `interval<state_t>` concept constraint failure when `state_t` is a nested struct with
  default member initializers — GCC requires an explicit `constexpr` default constructor
  for `std::default_initializable<T>` to hold during concept instantiation

## [0.2.0] - 2025-11-17

Initial atomic simulation layer: `interval<T>`, `decimal<N>`, `simulator<Atomic>`,
`counter` basic model, and the `interval_scalar` concept.

[Unreleased]: https://github.com/sdavtaker/cadmia/compare/v0.4.0...HEAD
[0.4.0]: https://github.com/sdavtaker/cadmia/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/sdavtaker/cadmia/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/sdavtaker/cadmia/releases/tag/v0.2.0

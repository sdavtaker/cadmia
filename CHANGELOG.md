# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

[Unreleased]: https://github.com/sdavtaker/cadmia/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/sdavtaker/cadmia/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/sdavtaker/cadmia/releases/tag/v0.2.0

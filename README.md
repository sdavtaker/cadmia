# CadmIA [![License](https://img.shields.io/badge/License-BSD%202--Clause-orange.svg)](https://opensource.org/licenses/BSD-2-Clause) [![codecov](https://codecov.io/gh/sdavtaker/cadmia/branch/master/graph/badge.svg)](https://codecov.io/gh/sdavtaker/cadmia) [![DOI](https://zenodo.org/badge/68416727.svg)](https://zenodo.org/badge/latestdoi/68416727)
[![OpenSSF Scorecard](https://api.scorecard.dev/projects/github.com/sdavtaker/cadmia/badge)](https://scorecard.dev/viewer/?uri=github.com/sdavtaker/cadmia)


## Introduction
CadmIA is the next evolution of the Cadmium and CDBoost simulators, rewritten to implement
the IA‑DEVS formalism using modern C++23. The goal is to keep the proven ideas and
architectures of previous projects while delivering a cleaner, safer, and faster
implementation aligned with the IA‑DEVS specification from the [Uncertainty Aware DEVS paper](https://cell-devs-02.sce.carleton.ca/publications/2021/VWD21/).

Status: work in progress. Interfaces and behaviors may change while the IA‑DEVS core is
implemented and validated.

## Top features (planned)
* IA‑DEVS semantics with strong compile‑time validation of models.
* Value-based coupling: components communicate via typed message values; no port system (UA-DEVS/IA-DEVS do not define ports).
* Time representation decoupled from model logic.
* Header‑only delivery for easy consumption.
* C++23 standard library features and guidelines (no extra deps unless necessary).

## Quick start
### Requirements
* A C++23 compliant compiler (clang, GCC, or MSVC with C++23 enabled).

### Install
* The library is header‑only. Add the `include` directory to your compiler's include paths. Install by CMAKE is available.

### Building tests
Testing requires:
* Catch2 (brought via vcpkg in the provided CMake setup).

## Notes on concurrency (roadmap)
CadmIA will prioritize clear, deterministic IA‑DEVS semantics. Concurrency support will be
re‑evaluated with standard C++ facilities (e.g., jthread) rather than Boost. Details will be
documented once the single‑threaded IA‑DEVS engine is complete.

## AI coding agents
To keep contributor guidance consistent across IDEs and AI assistants, this repository defines
editor-agnostic rules for automated coding agents. See `agents.md` for the conventions AI tools
should follow (response format for change requests, C++23 coding standards, project constraints,
and testing expectations). This centralized file replaces any IDE-specific configuration to avoid
drift over time.

## References
* IA‑DEVS specification (VWD21): https://cell-devs-02.sce.carleton.ca/publications/2021/VWD21/
	(local: `uadevs-paper.tex`).
* CD++ website: http://cell-devs.sce.carleton.ca/mediawiki/index.php/Main_Page
* CD++ paper: http://www.sce.carleton.ca/faculty/wainer/papers/spe482.pdf
* CDBoost overview: http://blincubator.com/bi_library/simulation/?gform_post_id=1390
* Sequential PDEVS architecture (legacy background):
	http://cell-devs.sce.carleton.ca/publications/2015/VNWD15/

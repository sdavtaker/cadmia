# Agent instructions for this repository

These guidelines apply to any AI coding agent (VS Code, JetBrains IDEs, Neovim, web UIs, etc.) working on this repo. Keep responses concise and focused, and follow the standards below when proposing or making changes.

## Response format

- Keep answers short and impersonal.
- For change requests, format your reply as follows:
  1) Describe the solution step-by-step (briefly).
  2) Group changes by file; use the file path as the header.
  3) For each file, add a short summary, then a single code block with only the changes.
  4) Start the code block with four backticks and a language tag.
  5) First line inside the code block: a comment with the filepath.
  6) Use a single code block per file; if parts are unchanged, replace with `// ...existing code...`.
  7) Avoid heavy formatting; bullet lists are fine.
  8) Do not repeat large unchanged regions.

## C++ coding guidance (project-wide)

- Prefer concept definitions over SFINAE.
- Use C++23 features where appropriate.
- Use standard library features where possible.
- Avoid unnecessary dependencies; prefer the standard library and STL.
- Avoid adding new dependencies unless explicitly requested.
- The code was originally developed for C++11; refactor to C++23 when it simplifies reading and safety.
- Prefer `std::` over `using namespace std;`.
- Avoid `#define` unless absolutely necessary.
- Use `#pragma once` in headers.
- Prefer compile-time checks (`static_assert`, `constexpr`) over runtime checks.
- Avoid global variables; prefer passing parameters or using class members.
- For code correctness, refer to the C++ Core Guidelines: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
- We are building a C++ library for IA-DEVS simulation. For functionality correctness, refer to `uadevs-paper.tex` if it is available in this repo. Otherwise, refer to the online version in: https://cell-devs-02.sce.carleton.ca/publications/2021/VWD21/Uncertainty%2069.pdf
- Prefer STL algorithms and data structures over custom implementations.
- Exceptions: prefer standard exceptions; document the exception-safety guarantee (strong, basic, no-throw in every function).
- Prefer descriptive variable and function names.
- Detect input errors early and fail loudly.
- Prefer `using` over `typedef`

## Historical context

- This project started as a fork of the Cadmium PDEVS simulator to provide a solid foundation and reference implementation while transitioning to IA-DEVS.
- PDEVS-specific code has been removed from this repository as the IA-DEVS implementation matured.
- For reference to the original Cadmium PDEVS implementation, see: https://github.com/SimulationEverywhere/cadmium

## Project constraints

- Respect existing license headers and SPDX tags.
- Only add new files when explicitly requested.
- Prefer standard C++23. Use `#pragma once` in headers.
- Use RAII, `constexpr`, and `static_assert` where appropriate; avoid macros when possible.
- No `using namespace` in headers. Keep includes ordered: standard, third-party, project; group-separated.
- Keep lines ≤ 100 columns. Trim trailing whitespace. End files with a newline.
- Prefer `noexcept` on functions that cannot throw; mark methods `const` when appropriate.
- The project is a headers only library.

## Security and compliance

- Do not include copyrighted code you don’t have rights to; provide minimal stubs if needed.
- Do not suggest unsafe operations or hidden network access.

## Testing and quality

- Target high code coverage with unit tests.
- Explain assumptions briefly; provide small, focused diffs.
- Align to `.editorconfig` and `.clang-format` in this repo.

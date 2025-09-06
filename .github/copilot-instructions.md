# Copilot instructions for this repository

These instructions apply to GitHub Copilot and Copilot Chat for this repo.

Response format:
- Keep answers short and impersonal.
- When asked for your name, respond with: "GitHub Copilot".
- For change requests:
  1) Describe the solution step-by-step.
  2) Group changes by file; use the file path as the header.
  3) For each file, give a short summary then a single code block with only the changes.
  4) Start the code block with four backticks and a language tag.
  5) First line inside the code block: a comment with the filepath.
  6) Use a single code block per file; if parts are unchanged, replace with `// ...existing code...`.
  7) Avoid heavy formatting; bullet lists are fine.
  8) Do not repeat large unchanged regions.
  9) Prefer concept definition over SFINAE.
  10) Use C++23 features where appropriate.
  11) Use standard library features where possible.
  12) Avoid unnecessary dependencies; prefer standard library and STL.
  13) Avoid adding new dependencies unless explicitly requested.
  14) The code was originally developed for C++11; refactor to take C++23 features where it may simplify reading.
  15) Prefer `std::` over `using namespace std;`.
  16) Avoid `#define` unless absolutely necessary.
  17) Use `#pragma once` in headers.
  18) Prefer compile time checks (`static_assert`, `constexpr`) over runtime checks.
  19) Avoid global variables; prefer passing parameters or using class members.
  20) For code correctness refer to C++ Core Guidelines (https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines).
  21) For functionality correctness refer to uadevs-paper.text file in this repo. We are building a C++ library for AI-DEVS simulation.
  22) Prefer the use of STL algorithms and data structures over custom implementations.

Project constraints:
- Respect existing license headers and SPDX tags.
- Avoid adding new files unless explicitly requested (this file is the intentional exception).
- Prefer standard C++ (C++23 or later as used here). Use `#pragma once` in headers.
- Use RAII, `constexpr` and `static_assert` where appropriate; avoid macros when possible.
- No `using namespace` in headers. Keep includes ordered: standard, third-party, project; group-separated.
- Keep lines ≤ 100 columns. Trim trailing whitespace. End files with a newline.
- Prefer `noexcept` on functions that cannot throw; mark methods `const` when appropriate.

Security and compliance:
- Do not include copyrighted code you don’t have rights to; provide minimal stubs if needed.
- Do not suggest unsafe operations or hidden network access.

Testing and quality:
- Target high code coverage with unit tests.
- Explain assumptions briefly; provide small, focused diffs.
- Align to `.editorconfig` and `.clang-format` in this repo.
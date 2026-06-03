# cadmia Log Format

cadmia does **not** emit NDJSON to stdout. Instead, `RootCoordinator::simulate()` returns
an in-memory `std::vector<LogEntry<T>>` — one entry per simulation step across all branches.
Callers format and display this data however they need.

This design reflects cadmia's non-deterministic BFS execution model (Algorithm 4 from VWD21):
when multiple components are simultaneously imminent, the simulator explores each possible
execution order as a separate branch, so there is no single linear event stream to write.

## LogEntry Structure

```cpp
template <typename T> struct LogEntry {
    int step{};
    std::string branch;
    std::string kind;                       // "atomic" | "coupled" | "skip"
    std::optional<std::string> parent_branch;
    std::string time;                       // interval string, e.g. "[100, 200]"
    std::string component;                  // engine name; empty for "skip" entries
    std::optional<std::string> output;      // "<output>" if produced, nullopt otherwise
    std::optional<std::any> raw_output;     // typed output value for any_cast by callers
};
```

## Field Semantics

| Field           | Type                      | Description |
|-----------------|---------------------------|-------------|
| `step`          | `int`                     | Monotonically increasing step counter within this branch, starting at 0. |
| `branch`        | `string`                  | BFS branch identifier. Root branch is `"0"`. When a branch forks into N children, the children are `"0.0"`, `"0.1"`, …, `"0.N-1"`. Further nesting uses the same dot notation: `"0.0.2"`. |
| `kind`          | `string`                  | `"atomic"` — an atomic model fired; `"coupled"` — a coupled sub-model fired (delegates to inner engine); `"skip"` — this step advanced time without a component firing (time progressed, no imminent component). |
| `parent_branch` | `optional<string>`        | Branch ID of the parent when this branch was forked. `nullopt` for the root branch `"0"`. |
| `time`          | `string`                  | The limit interval used for this step, formatted as an interval string (see below). |
| `component`     | `string`                  | Name of the engine that fired. Empty string for `"skip"` entries. |
| `output`        | `optional<string>`        | `"<output>"` (literal marker) if the component produced output this step; `nullopt` if passive or no output. Use `raw_output` to retrieve the actual value. |
| `raw_output`    | `optional<any>`           | Typed output value (`std::any` from the model's `output()` function). Use `std::any_cast<YourType>` to recover the value. `nullopt` when `output` is `nullopt`. |

## Time Representation

cadmia uses interval arithmetic for time. Every time value is an `interval<T>` with:
- A lower bound and an upper bound (each a value of type `T`)
- Closed/open endpoint flags (`lower_closed`, `upper_closed`)
- Infinity is carried in-band: `std::numeric_limits<T>::infinity()` (and its negation) are
  used as bound values to represent ±∞. For types where `has_infinity == false` (e.g. `int`),
  intervals are always finite.

The `time` field is formatted by `RootCoordinator` as:

```
[lower, upper]   — both endpoints closed
(lower, upper]   — lower open
[lower, upper)   — upper open
(lower, upper)   — both open
[-inf, upper]    — lower is −∞
[lower, +inf)    — upper is +∞
```

Bound values are formatted via `operator<<` (through `std::ostringstream`); `-inf` and
`+inf` are emitted literally when `is_lower_infinite()` / `is_upper_infinite()` is true.

**Example intervals:**
- `[100, 200]` — both bounds finite and closed
- `[0, +inf)` — open-ended at infinity (passivated model)
- `(50, 50]` — degenerate interval representing a point in time

## Branch Structure

The root coordinator maintains a BFS queue. At each BFS step:

1. If only one component is imminent, it executes in the current branch (step counter advances).
2. If multiple components are simultaneously imminent, the branch **forks**: one new child branch per possible execution order is created, each cloning the pre-execution coordinator state.

This means:
- A deterministic simulation produces a single branch `"0"` with entries `step=0, 1, 2, …`
- A simulation with one non-deterministic moment at step 3 produces `"0.0"` and `"0.1"` forking from `"0"` at step 3, each with `parent_branch = "0"`

## Example Log

A two-component simulation that forks once:

```
step  branch  kind    parent  time        component  output
0     0       atomic          [0, 1]      clock      <output>
1     0       atomic          [1, 2]      clock      <output>
2     0       coupled         [2, 3]      top        <output>
3     0.0     atomic  0       [3, 4]      a          <output>
3     0.1     atomic  0       [3, 4]      b          nullopt
4     0.0     skip    0       [4, +inf)              
```

Reading the example:
- Steps 0–2 are deterministic; branch `"0"` advances.
- At step 3, two components (`a` and `b`) are simultaneously imminent, so the branch forks into `"0.0"` and `"0.1"`, both with `parent_branch = "0"`.
- Branch `"0.0"` continues to step 4 where the system passivates (kind `"skip"`, `time = "[4, +inf)"`, no component).

## Iterating the Log in C++

```cpp
#include <cadmia/engine/root_coordinator.hpp>

cadmia::engine::RootCoordinator<int> rc;
auto log = rc.simulate(my_model, initial_time);

for (const auto &entry : log) {
    std::cout << "step=" << entry.step
              << " branch=" << entry.branch
              << " kind=" << entry.kind
              << " time=" << entry.time
              << " component=" << entry.component;
    if (entry.output)
        std::cout << " output=" << *entry.output;
    std::cout << "\n";
}
```

## Simulation Limits

`simulate()` accepts two optional limit parameters:

| Parameter     | Default | Meaning |
|---------------|---------|---------|
| `max_steps`   | 10000   | Total steps across all branches. When reached, the simulation stops gracefully and returns the log accumulated so far. |
| `max_branches`| 1000    | Active BFS queue size limit. Exceeded when branching would push the queue past this size; throws `SimulationLimitError`. |

`SimulationLimitError` (derived from `std::runtime_error`) is thrown when `max_branches` is
exceeded, indicating likely exponential blowup in the non-determinism of the model.

## Comparison with cdboost / cadmium

| Feature            | cadmia                        | cadmium / cdboost           |
|--------------------|-------------------------------|-----------------------------|
| Output format      | `vector<LogEntry<T>>`         | NDJSON to stdout via spdlog |
| Time type          | `interval<T>` (bounds + flags)| Single `TIME` value         |
| Multi-branch       | Yes — BFS explores all orders | No — single execution path  |
| Filtering          | Filter the returned vector    | Filter NDJSON with `jq`     |
| Activation         | Always on (returned by simulate) | Requires `log::init()`   |

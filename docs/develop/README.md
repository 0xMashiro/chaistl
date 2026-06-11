# chaistl Development Documentation

This directory contains all developer-facing documentation for the chaistl
project. It is organized into five categories, from "must follow" to "for
reference only".

---

## Quick Navigation

| If you want to... | Go to |
|-------------------|-------|
| Understand the project scope and stability promises | [`constitution/`](constitution/) |
| Learn how to implement a container, iterator, or metaprogramming utility | [`patterns/`](patterns/) |
| Understand why a specific design choice was made | [`decisions/`](decisions/) |
| Read background analysis on iterator patterns or reference implementations | [`background/`](background/) |
| Follow how-to instructions (build, test, read stdlib source) | [`guides/`](guides/) |

---

## Directory Structure

```
docs/develop/
├── README.md                       # this file
│
├── constitution/                   # Project constitution: scope, architecture, API tiers
│   ├── architecture.md             # Namespaces, dependencies, public include layout, taxonomy
│   └── scope.md                    # Standard-compatible, stable extension, experimental boundaries
│
├── patterns/                       # Implementation patterns: how to write code
│   ├── containers.md               # Container construction, assignment, reallocation, swap
│   ├── iterators.md                # Iterator patterns (iterator_impl<Const> → deducing this)
│   ├── memory.md                   # Allocator interaction, RAII guards, exception safety
│   ├── metaprogramming.md          # if constexpr, void_t, noexcept propagation, deduction guides
│   ├── performance.md              # Trivially-copyable fast paths, [[likely]], move-if-noexcept
│   ├── include-philosophy.md       # Public entry headers, precise detail includes, API tiers
│   └── toolbox.md                  # std facilities reference (type traits, utility, memory, etc.)
│
├── decisions/                      # Design decision records (working documents, not final law)
│   ├── README.md                   # Index, format, and when-to-write guidance
│   ├── heap-policy-design.md       # priority_queue policy model and exception contracts
│   ├── iterator-interface.md       # current iterator facade pattern
│   └── tree-policy-design.md       # rb/avl/treap policy split for ordered containers
│
├── background/                     # Background analysis: not prescriptive
│   ├── gcc-ext-pbds-census.md      # pb_ds feature census and prioritization
│   ├── iterator-patterns.md        # Four iterator implementation techniques compared
│   ├── rotation-primitives-analysis.md
│   ├── reference-analysis.md       # Reference library design comparison
│   └── external-resources.md       # WG21 papers, implementation links
│
└── guides/                         # How-to guides
    ├── build.md                    # Build, test, benchmark commands
    ├── benchmarking.md             # Benchmark design, naming, policy matrix
    ├── testing.md                  # Tracker types, exception injection, compile-fail tests
    └── reading-stdlib.md           # How to read libc++/libstdc++ as reference material
```

---

## Document Categories Explained

### Constitution

Documents that define what the project **is** and **is not**. Changes require
discussion and consensus.

- **architecture.md**: Namespace layout, dependency rules, public include
  layout, container taxonomy
- **scope.md**: What belongs in standard-compatible `chaistl::*`, stable
  chaistl extensions, and `chaistl::experimental::*`; also which external
  specifications the project follows

### Patterns

Documents that describe **how to write code** in chaistl. They are canonical
examples, not strict rules.

- **containers.md**: Constructor categories, assignment patterns, reallocation
- **iterators.md**: Iterator patterns — `iterator_impl<Const>` (current) and deducing this (likely target)
- **memory.md**: `allocator_traits` routing, exception guards, storage builders
- **metaprogramming.md**: `if constexpr`, `void_t`, `noexcept` propagation, deduction guides
- **performance.md**: Trivially-copyable fast paths, branch hints, move-if-noexcept
- **include-philosophy.md**: Component-name public headers, precise internal
  includes, API tiers, directory layout
- **toolbox.md**: Complete reference of `std` facilities to use when implementing

### Decisions

Design decision records. Each file records **one** choice: the context, the
options considered, the chosen path, and the consequences.

These are **working documents**, not irrevocable law. As the project evolves,
choices may be revisited. Old records are kept (marked superseded) so the
history of reasoning is preserved.

### Background

Documents that provide **context and analysis** but do not prescribe behavior.
They help contributors understand the design space.

- **gcc-ext-pbds-census.md**: GCC pb_ds extension inventory and what chaistl
  should or should not learn from it
- **iterator-patterns.md**: Historical evolution of four iterator implementation techniques
- **reference-analysis.md**: How EASTL, Boost, and other references approach
  similar problems
- **rotation-primitives-analysis.md**: Rotation helper tradeoffs for tree
  policies
- **external-resources.md**: Where to find specifications and implementations

### Guides

Step-by-step instructions for **doing things**.

- **build.md**: How to configure, build, test, and benchmark
- **benchmarking.md**: How to design benchmark suites, name workloads, and
  compare policy variants
- **testing.md**: Tracker types, exception injection, compile-fail tests
- **reading-stdlib.md**: How to navigate libc++ and libstdc++ source for learning

---

## Adding New Documents

### When to add a decision record

Add a decision record when:
- A design choice affects multiple containers or modules
- The choice is not obvious from reading the code
- There were meaningful alternatives worth recording

Do **not** add a decision record for:
- Bug fixes
- Refactoring that preserves semantics
- Adding a new container that follows existing patterns

### When to add to Background

Add to `background/` when:
- You researched multiple implementations and want to share findings
- You analyzed a pattern but the project has not (yet) adopted it
- You want to document external resources for future reference

---

## Stability

| Category | Stability |
|----------|-----------|
| Constitution | Changes require PR review and discussion |
| Patterns | Updated as new patterns emerge; old patterns deprecated, not deleted |
| Decisions | Working documents; may be revisited, old records kept as history |
| Background | Living documents; updated as new information becomes available |
| Guides | Updated when build system or tooling changes |

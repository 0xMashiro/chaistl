# ChaiSTL

[![CI](https://github.com/0xMashiro/chaistl/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/0xMashiro/chaistl/actions/workflows/ci.yml)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

> What I cannot create, I do not understand.

ChaiSTL is an educational C++23 STL-style container library for learning data
structures by building them.

The project sits between classroom examples and production standard-library
implementations. SGI STL is approachable but simplified by age, while
libstdc++, libc++, and MSVC STL are excellent but necessarily shaped by ABI,
compatibility, platform, and decades of implementation history. ChaiSTL aims
for the middle ground: small enough to read, serious enough to test, and close
enough to the standard library to make the comparison useful.

ChaiSTL is built for C++ data structure courses, STL source reading, and
modern C++ implementation practice. It focuses on how data structures are
expressed with generic programming, allocator-aware storage, iterator
protocols, exception safety, policy-based design, tests, benchmarks, and build
tooling.

It is not a production standard library and does not try to replace
libstdc++, libc++, or MSVC STL. Production C++ code should use the standard
library shipped with its target platform and toolchain.

ChaiSTL also does not claim that its current implementation style is best
practice. The project is expected to keep changing as it explores newer C++
standard features, better testing strategies, cleaner build workflows, and more
useful ways to explain data structure implementation.

The author does not claim to be a C++ expert. Some implementations may miss
important historical design context, contain overly local tradeoffs, or have
strict technical issues that deserve correction. Community review, bug reports,
design notes, and patches are welcome; learning from that feedback is one of
the reasons this project is being opened.

## Scope

ChaiSTL focuses on data structures and the C++ mechanisms needed to implement
them well:

- STL-style containers, container adapters, iterators, algorithms, allocators,
  concepts, and supporting type traits.
- Interoperability with the real standard library where it helps validate
  interface shape and behavior.
- Tests that exercise standard-compatible behavior, exception safety,
  allocator interaction, and compile-time constraints. The tests are also
  intended to be executable examples for learning and debugging.
- Benchmarks that make implementation tradeoffs visible without claiming to
  outperform vendor libraries.
- Project infrastructure that is useful to study in its own right: CMake
  presets, layered CI, formatting, coverage, and benchmark setup.
- Policy-based data structure experiments inspired by GCC PBDS, including
  tree and heap policies that make implementation choices explicit.

Contributions that explore independent STL-like components are welcome when
they fit the educational scope of the project. Examples include smart
pointers, allocators, memory utilities, iterator adapters, or other components
that can be studied in isolation and tested for interoperability with the
official standard library. These areas are welcome research directions, but
they are not a personal maintenance commitment from the project author.

The project intentionally does not pursue:

- ABI stability.
- Full standard-library replacement status.
- Vendor-level backwards compatibility.
- Production support guarantees.
- Complete implementation of every standard library facility.

Some historical notes may mention `chaigo`, an earlier private toy project
used as personal practice material. It is not part of ChaiSTL, is not required
to build or understand this repository, and is not planned for publication.

## Design Principles

- Use **C++23** as the supported language baseline.
- Use C++26 builds as forward-compatibility canaries, not as the primary user
  contract.
- Prefer readable, modern generic code over historical compatibility layers.
- Use [cppreference](https://en.cppreference.com/) and the
  [C++ working draft](https://eel.is/c++draft/) as specification references.
- Read libstdc++, libc++, Boost, EASTL, and other libraries for implementation
  strategy, not as source material to copy.
- Treat policy-based design as an experiment and teaching tool, not as a
  universal abstraction style.
- Keep standard-compatible components, documented ChaiSTL extensions, and
  experimental components in separate namespaces and headers.

## Library Areas

- `chaistl::*` - standard-compatible components plus documented stable
  extensions.
- `chaistl::concepts::*` - constraints used to express standard-library-style
  requirements.
- `chaistl::meta::*` - compile-time traits and type transforms.
- `chaistl::experimental::*` - unstable data structures and utilities. These
  follow STL conventions but may change without notice.

## What You Can Study

- How contiguous, linked, tree-backed, heap-backed, and flat containers are
  organized.
- How PBDS-style policy layers can expose tree and heap implementation
  strategies.
- How allocator-aware construction and raw storage management interact.
- How iterator protocols, reverse iterators, ranges, and deduction guides fit
  together.
- How exception safety is encoded with RAII guards and construction
  transactions.
- How standard-like behavior can be tested against `std` where interfaces
  overlap.
- How to learn an implementation by reading and debugging focused tests. For
  beginners, the tests are often the best entry point: each one names a small
  behavior, constructs a concrete scenario, and shows the expected observable
  result.
- How a medium-sized C++ project can structure formatting, CI, tests,
  coverage, and benchmarks without becoming an industrial-scale codebase.
- How CI can be organized as a learning-friendly pipeline: fast style checks
  first, C++23 contract checks next, and slower compatibility, sanitizer,
  coverage, and benchmark jobs after the core build is known to work.
- How evolving C++ standards can change the shape of generic code and project
  practice over time.

## Quick Start

Requirements: C++23, CMake 3.28+, Ninja, Clang 22 or GCC 14.

```sh
git submodule update --init --recursive

cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug
```

Other presets (GCC, C++26, benchmarks) are documented in
[docs/develop/guides/build.md](docs/develop/guides/build.md).

The test suite is written to be read as much as it is written to be run. When
studying a container, start with its focused tests under `test/chaistl/`, step
through them in a debugger, then read the corresponding header in
`include/chaistl/`.

## For Beginners

You do not need to understand every C++ language feature before reading this
project. ChaiSTL uses concepts, ranges, allocator-aware construction,
exception-safety guards, deduction guides, and other modern C++ techniques, but
many of them can be treated as supporting infrastructure at first.

Pick one thing to study, then read the corresponding tests first, run or debug
a small scenario, and follow the public member functions in the header. If a
`requires` clause, allocator helper, raw-storage utility, or exception guard is
not the thing you are studying, it is fine to skip it temporarily and come back
later.

At first, templates, concepts, traits, and deduction guides may make class
signatures look more complicated than the underlying data structure. They are
there to express type requirements, preserve interoperability with standard
library components, improve diagnostics, and keep invalid combinations from
becoming runtime bugs. Their value usually becomes clearer after trying to
implement a similar container without them.

The project tries to keep low-level memory management and exception-safety
machinery wrapped in reusable helpers. That means beginners can focus on the
shape of the data structure first: where elements live, how iterators move,
when storage changes, and what observable behavior the container promises.

## Usage

```cpp
#include <chaistl/chaistl.hpp>

int main() {
  chaistl::vector<int> values;
  values.push_back(1);
  values.push_back(2);
}
```

Focused public entry headers are also available:

```cpp
#include <chaistl/containers.hpp>
#include <chaistl/containers/vector.hpp>
#include <chaistl/containers/pairing_heap.hpp>
#include <chaistl/experimental/containers/ring_buffer.hpp>
#include <chaistl/algorithm.hpp>
#include <chaistl/meta.hpp>
```

## Project Shape

```text
include/chaistl/
  chaistl.hpp                  # full public entry point
  concepts.hpp                 # concepts and named-requirement constraints
  containers.hpp               # containers library entry point
  iterators.hpp                # iterators library entry point
  algorithm.hpp                # algorithms library entry point
  memory.hpp                   # memory management library entry point
  meta.hpp                     # metaprogramming utilities

  algorithm/                   # algorithm implementations
  concepts/                    # constraint definitions
  containers/                  # user-facing stable container entry points
    vector.hpp                 #   thin public entry: includes sequence/vector.hpp
    set.hpp                    #   thin public entry: includes associative/set.hpp
    pairing_heap.hpp           #   stable extension alias
    avl_set.hpp                #   stable extension alias
    sequence/                  #   vector, deque, list, forward_list, array, stack, queue
    associative/               #   set, map (tree-based)
    heap/                      #   priority_queue and heap policies
    tree/                      #   internal tree infrastructure
    views/                     #   span, mdspan
  iterators/                   # concrete iterators and adaptors
  memory/                      # allocators and memory utilities
    detail/                    #   internal helpers (guards, builders, algorithms)
  meta/                        # compile-time traits and type transforms
  experimental/                # non-standard extensions (no stability promise)
```

## Documentation

- [docs/develop/guides/build.md](docs/develop/guides/build.md) - build, test, benchmark commands
- [docs/develop/constitution/architecture.md](docs/develop/constitution/architecture.md) - namespace layout, include philosophy, file naming
- [docs/develop/constitution/scope.md](docs/develop/constitution/scope.md) - standard vs experimental boundary, source references
- [docs/develop/patterns/toolbox.md](docs/develop/patterns/toolbox.md) - `std` facilities to reach for when implementing
- [docs/develop/patterns/containers.md](docs/develop/patterns/containers.md) - container construction, assignment, reallocation patterns
- [docs/develop/patterns/iterators.md](docs/develop/patterns/iterators.md) - iterator implementation patterns
- [docs/develop/patterns/memory.md](docs/develop/patterns/memory.md) - allocator interaction, RAII guards, exception safety
- [docs/develop/patterns/metaprogramming.md](docs/develop/patterns/metaprogramming.md) - `if constexpr`, `void_t`, `noexcept` propagation
- [docs/develop/patterns/performance.md](docs/develop/patterns/performance.md) - fast paths, branch hints, move-if-noexcept
- [docs/develop/patterns/include-philosophy.md](docs/develop/patterns/include-philosophy.md) - file naming, aggregation headers, directory layout
- [docs/develop/guides/testing.md](docs/develop/guides/testing.md) - tracker types, exception injection, compile-fail tests, coverage
- [docs/develop/guides/reading-stdlib.md](docs/develop/guides/reading-stdlib.md) - how to read libc++/libstdc++ as reference material
- [docs/develop/decisions/](docs/develop/decisions/) - design decision records (why the code looks like it does)
- [docs/develop/background/](docs/develop/background/) - background analysis (iterator techniques, reference library comparisons)

## License

ChaiSTL is distributed under the [Apache License 2.0](LICENSE).

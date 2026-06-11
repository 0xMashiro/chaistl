# chaistl Visualizer Lab

Experimental visualizer prototype for `chaistl` internals.

This directory is intentionally **not** part of the first-version core work.
The main project should stay focused on readable STL implementations, tests,
benchmarks, and source-level diagrams. The visualizer is kept as a lab so the
useful ideas are not lost.

## Current conclusion

A polished STL visualizer quickly becomes a separate product:

- Real execution traces need intrusive hooks, generated overlays, or compiler
  instrumentation.
- Hand-written traces are maintainable only for small teaching cases.
- Source mapping is useful, but fragile if it depends on raw string matching.
- The best short-term value is not animation; it is better comments, diagrams,
  tests, and benchmark cases in the core library.

## Ideas worth keeping

- **Test-backed cases**: visual examples should be linked to real gtest cases,
  not invented separately.
- **Semantic steps**: useful teaching steps are allocator/storage/lifetime
  boundaries: allocate raw storage, construct object, move/copy construct,
  destroy, deallocate, and commit pointer state.
- **Sidecar source maps**: source highlighting should live in tooling metadata,
  not in `vector.hpp` comments.
- **Equal-width memory cells**: a memory view must preserve the invariant that
  each `T` slot occupies the same visual width.
- **Implementation-guided simulator**: TypeScript simulation gives a better
  teaching experience than pretending to be a debugger.

## Prototype shape

The current prototype contains two paths:

- `web/src/visualCases.ts`: TypeScript teaching cases derived from vector tests.
- `cpp/`: an older WebAssembly experiment that returns trace JSON.

The TypeScript path is the more promising one for future teaching material. The
WASM path is kept only as a reference for possible later validation against real
`chaistl` execution.

## Run the prototype

```sh
cd tools/visualizer/web
pnpm install
pnpm dev
```

The top-level CMake option `CHAISTL_BUILD_VISUALIZER` remains off by default.

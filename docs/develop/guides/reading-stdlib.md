# Reading Reference Implementations

Production standard libraries are an essential resource, but they must be read
with the right expectations. Here is what to look for and what to ignore.

## What to Study

| Aspect | Where to look | What to learn |
|--------|--------------|---------------|
| Reallocation strategy | `vector::_M_realloc_insert` (libstdc++) | How to sequence allocate→construct→destroy→deallocate |
| Deque block management | `deque::_M_push_back_aux` (libstdc++) | Map growth, block allocation, index arithmetic |
| List sort algorithm | `list::sort` (libstdc++/libc++) | Bottom-up merge sort with counter array |
| Exception safety | `vector::_M_realloc_insert` | Strong guarantee via "construct new before destroying old" |
| Iterator design | `__deque_iterator` (libstdc++) | Segmented iterator arithmetic, `operator+=` |
| Allocator dispatch | `allocator_traits` (all three) | How traits normalize different allocator interfaces |

## What to Ignore

| Pattern | Why to skip it |
|---------|---------------|
| `std::__1` / `std::__detail` namespaces | Internal ABI isolation, not relevant to chaistl |
| `_M_` / `_S_` member prefixes | Hungarian notation for data members, use readable names instead |
| Pre-C++11 compatibility layers | `_M_move_assign`, `_M_copy_assign` overload sets for old standards |
| ABI tags and versioning | `[[__abi_tag__]]`, inline namespace versioning for binary compat |
| Platform `#ifdef` guards | `_GLIBCXX_USE_CXX11_ABI`, `_LIBCPP_NO_EXCEPTIONS` — chaistl targets C++23 only |
| Debug mode instrumentation | `_GLIBCXX_DEBUG`, `_LIBCPP_DEBUG` — separate concern |
| Optimized specializations | `vector<bool>`, `basic_string` small-object optimization |

The rule: **study the algorithm, ignore the scaffolding.**

## How to Navigate

```sh
# Clone reference implementations for local study
git clone https://github.com/gcc-mirror/gcc.git reference/libstdcxx
git clone https://github.com/llvm/llvm-project.git reference/libcxx

# Find a specific implementation
cd reference/libstdcxx
grep -rn "push_back" libstdc++-v3/include/bits/ | head -20
```

Reference repositories live under `reference/`, which is git-ignored.

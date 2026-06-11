// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdlib>

// ============================================================================
// Standard Library Hardening — P3471-style hardened preconditions.
//
// Hardened preconditions check for undefined behavior that leads to memory
// safety issues (out-of-bounds access, access to uninitialized memory, etc.).
// When a hardened precondition is violated, the program terminates.
//
// This is inspired by P3471 "Standard Library Hardening" (C++26).
// Reference: https://wg21.link/p3471
//
// Usage:
//   CHAI_HARDENED(pos < size(), "vector::operator[]: out of bounds");
//
// Control:
//   - Define CHAI_HARDENING_ENABLED to enable checks (default in debug builds).
//   - Define CHAI_HARDENING_DISABLED to disable checks (default in release builds).
//   - If neither is defined, checks are enabled when NDEBUG is NOT defined.
// ============================================================================

#ifndef CHAI_HARDENING_ENABLED
#ifndef CHAI_HARDENING_DISABLED
#if defined(NDEBUG)
#define CHAI_HARDENING_DISABLED
#else
#define CHAI_HARDENING_ENABLED
#endif
#endif
#endif

#ifdef CHAI_HARDENING_ENABLED

// Hardened precondition: check the condition, terminate if violated.
// The message is ignored in the minimal implementation (std::abort).
// Future: could forward to a user-configurable violation handler.
//
// Migration path when C++ Contracts (P2900) or compiler-native hardened
// mode becomes available:
//
//   1. Compiler flag (e.g. GCC/Clang -fhardened, MSVC /hardened):
//      Replace CHAI_HARDENED with __builtin_trap() or equivalent.
//      Zero source changes in container code.
//
//   2. C++ Contracts [[pre: condition]]:
//      CHAI_HARDENED(cond, msg)  →  [[pre: cond]]
//      The macro position (first statement in function body) maps
//      directly to a contract predicate.  Mechanical search/replace.
//
//   3. std::hardened precondition (if P3471 is adopted into the language):
//      CHAI_HARDENED becomes a no-op wrapper around the native facility.
//
// Because CHAI_HARDENED is placed exactly where a [[pre]] predicate would
// appear, container code requires no restructuring to migrate.
#define CHAI_HARDENED(condition, ...) \
  do {                                \
    if (!(condition)) {               \
      std::abort();                   \
    }                                 \
  } while (false)

#else

// Hardened preconditions are disabled: expand to nothing.
#define CHAI_HARDENED(condition, ...) ((void)0)

#endif

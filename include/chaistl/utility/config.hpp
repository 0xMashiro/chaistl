// SPDX-License-Identifier: Apache-2.0

#pragma once

#if defined(__GNUC__) || defined(__clang__)
#define CHAI_ALWAYS_INLINE [[gnu::always_inline]]
#define CHAI_COLD [[gnu::cold]]
#else
#define CHAI_ALWAYS_INLINE
#define CHAI_COLD
#endif

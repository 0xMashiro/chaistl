// SPDX-License-Identifier: Apache-2.0

// Benchmark registration helpers.

#pragma once

#include <map>
#include <string>

#define CHAISTL_REGISTER_BENCHMARK_FILE(fn)                  \
  namespace {                                                \
  const int chaistl_benchmark_registered_##fn = ((fn)(), 0); \
  }

#define CHAISTL_BENCHMARK_SMOKE_DOMAIN(domain, regex)                                                                  \
  namespace {                                                                                                          \
  const int chaistl_benchmark_smoke_domain_##domain = (::chaistl_benchmark::register_smoke_domain(#domain, regex), 0); \
  }

namespace chaistl_benchmark {

void register_smoke_domain(const char* domain, const char* filter_regex);
const std::map<std::string, std::string>& smoke_domains();

}  // namespace chaistl_benchmark

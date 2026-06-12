// SPDX-License-Identifier: Apache-2.0

#include "chaistl/registry.hpp"

namespace chaistl_benchmark {

namespace {

std::map<std::string, std::string>& mutable_smoke_domains() {
  static auto* domains = new std::map<std::string, std::string>();
  return *domains;
}

}  // namespace

void register_smoke_domain(const char* domain, const char* filter_regex) {
  auto& domains = mutable_smoke_domains();
  auto& regex = domains[domain];
  if (!regex.empty()) {
    regex += "|";
  }
  regex += filter_regex;
}

const std::map<std::string, std::string>& smoke_domains() {
  return mutable_smoke_domains();
}

}  // namespace chaistl_benchmark

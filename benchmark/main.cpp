// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "chaistl/comparison_reporter.hpp"
#include "chaistl/registry.hpp"

namespace {

class CompositeReporter final : public benchmark::BenchmarkReporter {
 public:
  CompositeReporter(benchmark::BenchmarkReporter& first, benchmark::BenchmarkReporter& second)
      : first_(first), second_(second) {}

  bool ReportContext(const Context& context) override {
    return first_.ReportContext(context) && second_.ReportContext(context);
  }

  void ReportRunsConfig(double min_time, bool has_explicit_iters, benchmark::IterationCount iters) override {
    first_.ReportRunsConfig(min_time, has_explicit_iters, iters);
    second_.ReportRunsConfig(min_time, has_explicit_iters, iters);
  }

  void ReportRuns(const std::vector<Run>& reports) override {
    first_.ReportRuns(reports);
    second_.ReportRuns(reports);
  }

  void Finalize() override {
    first_.Finalize();
    second_.Finalize();
  }

 private:
  benchmark::BenchmarkReporter& first_;
  benchmark::BenchmarkReporter& second_;
};

std::map<std::string, std::string> smoke_domains_with_aggregates() {
  auto domains = chaistl_benchmark::smoke_domains();

  const auto append_domain = [&domains](std::string& filter, const char* domain) {
    const auto iter = domains.find(domain);
    if (iter == domains.end()) {
      return;
    }
    if (!filter.empty()) {
      filter += "|";
    }
    filter += iter->second;
  };

  std::string policy_filter;
  append_domain(policy_filter, "tree_policy");
  append_domain(policy_filter, "order_statistic");
  append_domain(policy_filter, "heap_policy");
  domains["policy"] = policy_filter;

  std::string all_filter;
  for (const auto& [domain, filter] : chaistl_benchmark::smoke_domains()) {
    if (!all_filter.empty()) {
      all_filter += "|";
    }
    all_filter += filter;
  }
  domains["all"] = all_filter;

  return domains;
}

void print_smoke_domains(std::ostream& out) {
  for (const auto& [domain, filter] : smoke_domains_with_aggregates()) {
    (void)filter;
    out << domain << '\n';
  }
}

int preprocess_smoke_args(int& argc, char**& argv, std::vector<std::string>& args, std::vector<char*>& arg_ptrs) {
  constexpr std::string_view smoke_prefix = "--smoke=";
  const auto domains = smoke_domains_with_aggregates();

  args.clear();
  args.reserve(static_cast<std::size_t>(argc) + 1);

  bool smoke_requested = false;
  std::string smoke_domain;

  for (int index = 0; index < argc; ++index) {
    std::string arg = argv[index];
    if (arg == "--smoke_list") {
      print_smoke_domains(std::cout);
      return 1;
    }
    if (arg == "--smoke") {
      smoke_requested = true;
      smoke_domain = "all";
      continue;
    }
    if (arg.starts_with(smoke_prefix)) {
      smoke_requested = true;
      smoke_domain = arg.substr(smoke_prefix.size());
      if (smoke_domain.empty()) {
        smoke_domain = "all";
      }
      continue;
    }
    args.push_back(std::move(arg));
  }

  if (smoke_requested) {
    const auto iter = domains.find(smoke_domain);
    if (iter == domains.end()) {
      std::cerr << "Unknown smoke domain: " << smoke_domain << '\n';
      std::cerr << "Available smoke domains:\n";
      print_smoke_domains(std::cerr);
      return 2;
    }
    args.push_back("--benchmark_filter=" + iter->second);
    args.push_back("--benchmark_dry_run");
  }

  arg_ptrs.clear();
  arg_ptrs.reserve(args.size() + 1);
  for (auto& arg : args) {
    arg_ptrs.push_back(arg.data());
  }
  arg_ptrs.push_back(nullptr);

  argc = static_cast<int>(arg_ptrs.size());
  --argc;
  argv = arg_ptrs.data();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args;
  std::vector<char*> arg_ptrs;
  const int smoke_result = preprocess_smoke_args(argc, argv, args, arg_ptrs);
  if (smoke_result != 0) {
    return smoke_result == 1 ? 0 : smoke_result;
  }

  benchmark::Initialize(&argc, argv);

  // Use the comparison reporter alongside the console reporter.
  chaistl_benchmark::ComparisonReporter comparison_reporter;
  benchmark::ConsoleReporter console_reporter;
  CompositeReporter reporter(console_reporter, comparison_reporter);

  benchmark::RunSpecifiedBenchmarks(&reporter);

  benchmark::Shutdown();
  return 0;
}

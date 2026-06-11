// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>
#include <vector>

#include "chaistl/comparison_reporter.hpp"

namespace chaistl_benchmark {

void register_array_benchmarks();
void register_deque_benchmarks();
void register_forward_list_benchmarks();
void register_heap_policy_benchmarks();
void register_list_benchmarks();
void register_tree_policy_benchmarks();
void register_vector_benchmarks();
void register_storage_builder_benchmarks();
void register_flat_container_benchmarks();
void register_bit_structure_benchmarks();

}  // namespace chaistl_benchmark

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

}  // namespace

int main(int argc, char** argv) {
  chaistl_benchmark::register_array_benchmarks();
  chaistl_benchmark::register_deque_benchmarks();
  chaistl_benchmark::register_forward_list_benchmarks();
  chaistl_benchmark::register_heap_policy_benchmarks();
  chaistl_benchmark::register_list_benchmarks();
  chaistl_benchmark::register_tree_policy_benchmarks();
  chaistl_benchmark::register_vector_benchmarks();
  chaistl_benchmark::register_storage_builder_benchmarks();
  chaistl_benchmark::register_flat_container_benchmarks();
  chaistl_benchmark::register_bit_structure_benchmarks();

  benchmark::Initialize(&argc, argv);

  // Use the comparison reporter alongside the console reporter.
  chaistl_benchmark::ComparisonReporter comparison_reporter;
  benchmark::ConsoleReporter console_reporter;
  CompositeReporter reporter(console_reporter, comparison_reporter);

  benchmark::RunSpecifiedBenchmarks(&reporter);

  benchmark::Shutdown();
  return 0;
}

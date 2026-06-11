// SPDX-License-Identifier: Apache-2.0

// References:
// - Google Benchmark supports custom reporters via benchmark::BenchmarkReporter.
// - This reporter produces an EASTL-style comparison table showing std vs chaistl
//   performance ratios side by side.

#pragma once

#include <benchmark/benchmark.h>
#include <map>
#include <string>
#include <vector>

namespace chaistl_benchmark {

// ComparisonReporter collects benchmark results and prints a formatted
// comparison table grouping std::* vs chaistl::* benchmarks by operation.
class ComparisonReporter : public benchmark::BenchmarkReporter {
 public:
  struct RunData {
    std::string benchmark_name;
    std::string container_name;  // e.g. "std::vector<int>" or "chaistl::vector<int>"
    std::string operation_name;  // e.g. "push_back_growth"
    std::string arg_str;         // e.g. "65536"
    double real_time_ns{};
    double cpu_time_ns{};
  };

  struct ComparisonRow {
    std::string operation;  // e.g. "vector<int>/push_back_growth/65536"
    double std_time_ns{};
    double chaistl_time_ns{};
    bool has_std = false;
    bool has_chaistl = false;
  };

  bool ReportContext(const Context& context) override;
  void ReportRuns(const std::vector<Run>& reports) override;
  void Finalize() override;

 private:
  std::vector<RunData> runs_;

  static void ParseBenchmarkName(const std::string& full_name,
                                 std::string& container_name,
                                 std::string& operation_name,
                                 std::string& arg_str);
  static void PrintComparisonTable(const std::map<std::string, ComparisonRow>& rows);
};

}  // namespace chaistl_benchmark

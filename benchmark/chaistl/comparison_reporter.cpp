// SPDX-License-Identifier: Apache-2.0

// References:
// - Google Benchmark supports custom reporters via benchmark::BenchmarkReporter.
// - This reporter produces an EASTL-style comparison table showing std vs chaistl
//   performance ratios side by side.

#include "comparison_reporter.hpp"

#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace chaistl_benchmark {

bool ComparisonReporter::ReportContext(const Context& /*context*/) {
  // Print the comparison reporter header.
  std::cout << "================================================================================\n";
  std::cout << "ChaiSTL Benchmark Comparison Reporter\n";
  std::cout << "================================================================================\n\n";
  return true;
}

void ComparisonReporter::ReportRuns(const std::vector<Run>& reports) {
  for (const auto& report : reports) {
    RunData data;
    data.benchmark_name = std::string(report.benchmark_name());
    data.real_time_ns = report.GetAdjustedRealTime();
    data.cpu_time_ns = report.GetAdjustedCPUTime();

    ParseBenchmarkName(data.benchmark_name, data.container_name, data.operation_name, data.arg_str);

    runs_.push_back(std::move(data));
  }
}

void ComparisonReporter::ParseBenchmarkName(const std::string& full_name,
                                            std::string& container_name,
                                            std::string& operation_name,
                                            std::string& arg_str) {
  // Format: "container_name/operation_name/arg" or "container_name/operation_name"
  // Container name may contain '/' (e.g. "std::array<int,32>"), so we parse from the end.

  container_name.clear();
  operation_name.clear();
  arg_str.clear();

  // Find the last slash first (this separates arg from the rest).
  std::size_t last_slash = full_name.rfind('/');
  if (last_slash == std::string::npos) {
    container_name = full_name;
    return;
  }

  // Check if the part after the last slash is a number (arg).
  std::string after_last = full_name.substr(last_slash + 1);
  bool is_arg = !after_last.empty() && std::all_of(after_last.begin(), after_last.end(), [](char c) {
    return std::isdigit(c) || c == '-';
  });

  if (is_arg) {
    arg_str = after_last;
    std::string prefix = full_name.substr(0, last_slash);

    // Find the second-to-last slash.
    std::size_t op_slash = prefix.rfind('/');
    if (op_slash == std::string::npos) {
      container_name = prefix;
      return;
    }

    operation_name = prefix.substr(op_slash + 1);
    container_name = prefix.substr(0, op_slash);
  } else {
    // No arg; last slash separates container from operation.
    operation_name = after_last;
    container_name = full_name.substr(0, last_slash);
  }
}

void ComparisonReporter::PrintComparisonTable(const std::map<std::string, ComparisonRow>& rows) {
  // Header.
  std::cout << "\n";
  std::cout << "================================================================================\n";
  std::cout << "Container Benchmark Comparison: std vs chaistl\n";
  std::cout << "================================================================================\n";
  std::cout << std::left << std::setw(50) << "Test" << std::right << std::setw(18) << "std (ns)" << std::setw(18)
            << "chaistl (ns)" << std::setw(10) << "Ratio" << std::setw(12) << "Faster?" << "\n";
  std::cout << "--------------------------------------------------------------------------------\n";

  double total_std_ns = 0.0;
  double total_chaistl_ns = 0.0;

  for (const auto& [key, row] : rows) {
    if (!row.has_std || !row.has_chaistl) {
      continue;  // Skip incomplete comparisons.
    }

    double ratio = row.std_time_ns / row.chaistl_time_ns;
    const char* faster = (ratio > 1.05) ? "chaistl" : (ratio < 0.95) ? "std" : "~tie";

    std::ostringstream ratio_stream;
    ratio_stream << std::fixed << std::setprecision(2) << ratio;

    std::cout << std::left << std::setw(50) << key << std::right << std::setw(18) << std::fixed << std::setprecision(1)
              << row.std_time_ns << std::setw(18) << std::fixed << std::setprecision(1) << row.chaistl_time_ns
              << std::setw(10) << ratio_stream.str() << std::setw(12) << faster << "\n";

    total_std_ns += row.std_time_ns;
    total_chaistl_ns += row.chaistl_time_ns;
  }

  std::cout << "--------------------------------------------------------------------------------\n";

  if (total_std_ns > 0.0 && total_chaistl_ns > 0.0) {
    double total_ratio = total_std_ns / total_chaistl_ns;
    const char* total_faster = (total_ratio > 1.05) ? "chaistl" : (total_ratio < 0.95) ? "std" : "~tie";

    std::ostringstream total_ratio_stream;
    total_ratio_stream << std::fixed << std::setprecision(2) << total_ratio;

    std::cout << std::left << std::setw(50) << "TOTAL (sum of all comparisons)" << std::right << std::setw(18)
              << std::fixed << std::setprecision(1) << total_std_ns << std::setw(18) << std::fixed
              << std::setprecision(1) << total_chaistl_ns << std::setw(10) << total_ratio_stream.str() << std::setw(12)
              << total_faster << "\n";
  }

  std::cout << "================================================================================\n";
}

void ComparisonReporter::Finalize() {
  // Group runs by (operation + arg) -> compare std vs chaistl.
  // Key format: "container_base/operation/arg" where container_base strips the prefix.
  std::map<std::string, ComparisonRow> rows;

  for (const auto& run : runs_) {
    if (run.container_name.empty() || run.operation_name.empty()) {
      continue;
    }

    // Determine if this is std or chaistl.
    bool is_std = (run.container_name.find("std::") == 0);
    bool is_chaistl = (run.container_name.find("chaistl::") == 0);

    if (!is_std && !is_chaistl) {
      continue;
    }

    // Extract base container name (e.g. "vector<int>" from "std::vector<int>").
    std::string base_name;
    if (is_std) {
      base_name = run.container_name.substr(5);  // Skip "std::".
    } else {
      base_name = run.container_name.substr(9);  // Skip "chaistl::".
    }

    // Build comparison key.
    std::string key = base_name + "/" + run.operation_name;
    if (!run.arg_str.empty()) {
      key += "/" + run.arg_str;
    }

    auto& row = rows[key];
    row.operation = key;

    if (is_std) {
      row.std_time_ns = run.real_time_ns;
      row.has_std = true;
    } else {
      row.chaistl_time_ns = run.real_time_ns;
      row.has_chaistl = true;
    }
  }

  PrintComparisonTable(rows);
}

}  // namespace chaistl_benchmark

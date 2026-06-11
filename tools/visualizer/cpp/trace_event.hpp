#pragma once

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace chaistl::visual {

struct trace_event {
  std::string op;
  std::string label;
  std::string storage;
  std::ptrdiff_t from_slot{-1};
  std::ptrdiff_t to_slot{-1};
  std::string value;
  std::string source_file;
  std::string source_anchor;
  std::size_t source_line{};
};

class trace_builder {
 public:
  void add(trace_event event) { events_.push_back(std::move(event)); }

  [[nodiscard]] std::string to_json(std::string_view scenario_name) const {
    std::ostringstream output;
    output << "{";
    output << "\"scenario\":\"" << escape(scenario_name) << "\",";
    output << "\"events\":[";

    for (std::size_t index = 0; index < events_.size(); ++index) {
      if (index != 0) {
        output << ",";
      }
      const auto& event = events_[index];
      output << "{";
      output << "\"id\":" << index << ",";
      output << "\"op\":\"" << escape(event.op) << "\",";
      output << "\"label\":\"" << escape(event.label) << "\",";
      output << "\"storage\":\"" << escape(event.storage) << "\",";
      output << "\"fromSlot\":" << event.from_slot << ",";
      output << "\"toSlot\":" << event.to_slot << ",";
      output << "\"value\":\"" << escape(event.value) << "\",";
      output << "\"sourceFile\":\"" << escape(event.source_file) << "\",";
      output << "\"sourceAnchor\":\"" << escape(event.source_anchor) << "\",";
      output << "\"sourceLine\":" << event.source_line;
      output << "}";
    }

    output << "]}";
    return output.str();
  }

 private:
  [[nodiscard]] static std::string escape(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (char character : value) {
      switch (character) {
        case '\\':
          result += "\\\\";
          break;
        case '"':
          result += "\\\"";
          break;
        case '\n':
          result += "\\n";
          break;
        case '\r':
          result += "\\r";
          break;
        case '\t':
          result += "\\t";
          break;
        default:
          result += character;
          break;
      }
    }
    return result;
  }

  std::vector<trace_event> events_;
};

[[nodiscard]] std::string run_vector_scenario(std::string_view name);

}  // namespace chaistl::visual

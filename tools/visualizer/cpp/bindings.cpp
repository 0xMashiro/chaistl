#include <emscripten/bind.h>
#include <string>

#include "trace_event.hpp"

namespace {

std::string run_scenario(const std::string& name) {
  return chaistl::visual::run_vector_scenario(name);
}

}  // namespace

EMSCRIPTEN_BINDINGS(chaistl_visualizer) {
  emscripten::function("run_scenario", &run_scenario);
}

#include <chaistl/containers/sequence/vector.hpp>

#include <cstddef>
#include <string>
#include <string_view>

#include "trace_event.hpp"

namespace chaistl::visual {
namespace {

constexpr std::string_view kVectorSourceFile = "include/chaistl/containers/sequence/vector.hpp";

namespace vector_source_anchor {
constexpr std::string_view reallocate_and_emplace_back_entry = "vector.reallocate_and_emplace_back.allocate";
constexpr std::string_view reallocate_and_emplace_back_allocate = "vector.reallocate_and_emplace_back.allocate";
constexpr std::string_view reallocate_and_emplace_back_construct_tail =
    "vector.reallocate_and_emplace_back.construct_tail";
constexpr std::string_view reallocate_and_emplace_back_move_old = "vector.reallocate_and_emplace_back.move_old";
constexpr std::string_view reallocate_and_emplace_back_commit = "vector.reallocate_and_emplace_back.commit";

constexpr std::string_view reallocate_and_emplace_at_entry = "vector.reallocate_and_emplace_at.allocate";
constexpr std::string_view reallocate_and_emplace_at_allocate = "vector.reallocate_and_emplace_at.allocate";
constexpr std::string_view reallocate_and_emplace_at_construct_inserted =
    "vector.reallocate_and_emplace_at.construct_inserted";
constexpr std::string_view reallocate_and_emplace_at_move_prefix = "vector.reallocate_and_emplace_at.move_prefix";
constexpr std::string_view reallocate_and_emplace_at_move_suffix = "vector.reallocate_and_emplace_at.move_suffix";
constexpr std::string_view reallocate_and_emplace_at_commit = "vector.reallocate_and_emplace_at.commit";
}  // namespace vector_source_anchor

void add_event(trace_builder& trace,
               std::string op,
               std::string label,
               std::string storage,
               std::ptrdiff_t from_slot = -1,
               std::ptrdiff_t to_slot = -1,
               std::string value = {},
               std::string_view source_anchor = {}) {
  trace.add(trace_event{.op = std::move(op),
                        .label = std::move(label),
                        .storage = std::move(storage),
                        .from_slot = from_slot,
                        .to_slot = to_slot,
                        .value = std::move(value),
                        .source_file = std::string{kVectorSourceFile},
                        .source_anchor = std::string{source_anchor}});
}

[[nodiscard]] std::string vector_insert_reallocate() {
  trace_builder trace;

  chaistl::vector<int> values;
  values.reserve(3);
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);

  add_event(trace,
            "snapshot",
            "current vector has size 3 and capacity 3",
            "old",
            -1,
            -1,
            "1,2,3",
            vector_source_anchor::reallocate_and_emplace_at_entry);
  add_event(trace,
            "allocate",
            "allocate candidate storage for reallocation insert",
            "new",
            -1,
            -1,
            "6",
            vector_source_anchor::reallocate_and_emplace_at_allocate);
  add_event(trace,
            "construct",
            "construct inserted value at its final slot first",
            "new",
            -1,
            1,
            "9",
            vector_source_anchor::reallocate_and_emplace_at_construct_inserted);
  add_event(trace,
            "move_construct",
            "relocate prefix element into candidate storage",
            "new",
            0,
            0,
            "1",
            vector_source_anchor::reallocate_and_emplace_at_move_prefix);
  add_event(trace,
            "move_construct",
            "relocate suffix element into candidate storage",
            "new",
            1,
            2,
            "2",
            vector_source_anchor::reallocate_and_emplace_at_move_suffix);
  add_event(trace,
            "move_construct",
            "relocate suffix element into candidate storage",
            "new",
            2,
            3,
            "3",
            vector_source_anchor::reallocate_and_emplace_at_move_suffix);

  values.insert(values.begin() + 1, 9);

  add_event(trace,
            "commit_storage",
            "candidate storage becomes the vector storage",
            "new",
            -1,
            -1,
            {},
            vector_source_anchor::reallocate_and_emplace_at_commit);
  add_event(trace,
            "deallocate",
            "release old storage after commit",
            "old",
            -1,
            -1,
            {},
            vector_source_anchor::reallocate_and_emplace_at_commit);
  add_event(trace,
            "snapshot",
            "final vector value order",
            "new",
            -1,
            -1,
            "1,9,2,3",
            vector_source_anchor::reallocate_and_emplace_at_commit);
  return trace.to_json("vector_insert_reallocate");
}

[[nodiscard]] std::string vector_push_back_reallocate() {
  trace_builder trace;

  chaistl::vector<int> values;
  values.reserve(2);
  values.push_back(1);
  values.push_back(2);

  add_event(trace,
            "snapshot",
            "current vector has no spare capacity",
            "old",
            -1,
            -1,
            "1,2",
            vector_source_anchor::reallocate_and_emplace_back_entry);
  add_event(trace,
            "allocate",
            "allocate a larger candidate storage",
            "new",
            -1,
            -1,
            "4",
            vector_source_anchor::reallocate_and_emplace_back_allocate);
  add_event(trace,
            "construct",
            "construct the new tail element first",
            "new",
            -1,
            2,
            "3",
            vector_source_anchor::reallocate_and_emplace_back_construct_tail);
  add_event(trace,
            "move_construct",
            "relocate old element 0",
            "new",
            0,
            0,
            "1",
            vector_source_anchor::reallocate_and_emplace_back_move_old);
  add_event(trace,
            "move_construct",
            "relocate old element 1",
            "new",
            1,
            1,
            "2",
            vector_source_anchor::reallocate_and_emplace_back_move_old);

  values.push_back(3);

  add_event(trace,
            "commit_storage",
            "candidate storage becomes the vector storage",
            "new",
            -1,
            -1,
            {},
            vector_source_anchor::reallocate_and_emplace_back_commit);
  add_event(trace,
            "deallocate",
            "release old storage after commit",
            "old",
            -1,
            -1,
            {},
            vector_source_anchor::reallocate_and_emplace_back_commit);
  add_event(trace,
            "snapshot",
            "final vector value order",
            "new",
            -1,
            -1,
            "1,2,3",
            vector_source_anchor::reallocate_and_emplace_back_commit);
  return trace.to_json("vector_push_back_reallocate");
}

}  // namespace

std::string run_vector_scenario(std::string_view name) {
  if (name == "vector_push_back_reallocate") {
    return vector_push_back_reallocate();
  }
  return vector_insert_reallocate();
}

}  // namespace chaistl::visual

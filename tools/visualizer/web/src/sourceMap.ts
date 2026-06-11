export type SourceMapEntry = {
  anchor: string;
  after: string;
  start: string;
  end?: string;
};

export const vectorSourceMap: SourceMapEntry[] = [
  {
    anchor: "vector.reallocate_and_emplace_back.allocate",
    after: "vector<T, Allocator>::reallocate_and_emplace_back(Args&&... args)",
    start: "pointer new_first = allocator_traits::allocate(allocator_, new_capacity);",
  },
  {
    anchor: "vector.reallocate_and_emplace_back.construct_tail",
    after: "vector<T, Allocator>::reallocate_and_emplace_back(Args&&... args)",
    start:
      "allocator_traits::construct(allocator_, std::to_address(new_tail), std::forward<Args>(args)...);",
  },
  {
    anchor: "vector.reallocate_and_emplace_back.move_old",
    after: "for (pointer current = first_; current != last_; ++current, ++new_last)",
    start: "allocator_traits::construct(allocator_,",
    end: "std::move_if_noexcept(*std::to_address(current)));",
  },
  {
    anchor: "vector.reallocate_and_emplace_back.commit",
    after: "vector<T, Allocator>::reallocate_and_emplace_back(Args&&... args)",
    start: "release_storage();",
    end: "capacity_last_ = first_ + new_capacity;",
  },
  {
    anchor: "vector.reallocate_and_emplace_at.allocate",
    after: "vector<T, Allocator>::reallocate_and_emplace_at(size_type offset, Args&&... args)",
    start: "pointer new_first = allocator_traits::allocate(allocator_, new_capacity);",
  },
  {
    anchor: "vector.reallocate_and_emplace_at.construct_inserted",
    after: "vector<T, Allocator>::reallocate_and_emplace_at(size_type offset, Args&&... args)",
    start:
      "allocator_traits::construct(allocator_, std::to_address(inserted), std::forward<Args>(args)...);",
  },
  {
    anchor: "vector.reallocate_and_emplace_at.move_prefix",
    after: "for (size_type index = 0; index < offset; ++index, ++prefix_last)",
    start: "allocator_traits::construct(allocator_,",
    end: "std::move_if_noexcept(first_[index]));",
  },
  {
    anchor: "vector.reallocate_and_emplace_at.move_suffix",
    after: "for (size_type index = offset; index < old_size; ++index, ++suffix_last)",
    start: "allocator_traits::construct(allocator_,",
    end: "std::move_if_noexcept(first_[index]));",
  },
  {
    anchor: "vector.reallocate_and_emplace_at.commit",
    after: "vector<T, Allocator>::reallocate_and_emplace_at(size_type offset, Args&&... args)",
    start: "release_storage();",
    end: "capacity_last_ = first_ + new_capacity;",
  },
];

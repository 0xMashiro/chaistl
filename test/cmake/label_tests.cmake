foreach(test_name IN LISTS chaistl_tests_TESTS)
  set(labels unit)

  if(test_name MATCHES "^chaistl\\.SmokeTest\\.")
    list(APPEND labels smoke)
  elseif(test_name MATCHES "^chaistl\\.AlgorithmComparisonTest\\.")
    list(APPEND labels algorithm)
  elseif(test_name MATCHES "^chaistl\\.AllocatorTest\\.")
    list(APPEND labels memory allocator)
  elseif(test_name MATCHES "^chaistl\\.StorageWorkflowTest\\." OR
         test_name MATCHES "^chaistl\\.UninitializedAllocatorAlgorithmsTest\\.")
    list(APPEND labels memory)
  elseif(test_name MATCHES "^chaistl\\..*Concept")
    list(APPEND labels concepts)
  elseif(test_name MATCHES "^chaistl\\.ContiguousIteratorTest\\." OR
         test_name MATCHES "^chaistl\\.ReverseIteratorTest\\.")
    list(APPEND labels iterators)
  elseif(test_name MATCHES "^chaistl\\.Array")
    list(APPEND labels containers sequence array)
  elseif(test_name MATCHES "^chaistl\\.Deque")
    list(APPEND labels containers sequence deque)
  elseif(test_name MATCHES "^chaistl\\.Vector")
    list(APPEND labels containers sequence vector)
  elseif(test_name MATCHES "^chaistl\\.List")
    list(APPEND labels containers sequence list)
  elseif(test_name MATCHES "^chaistl\\.ForwardList")
    list(APPEND labels containers sequence forward_list)
  elseif(test_name MATCHES "^chaistl\\.Queue" OR test_name MATCHES "^chaistl\\.Stack")
    list(APPEND labels containers sequence adapter)
  elseif(test_name MATCHES "^chaistl\\.PriorityQueue" OR
         test_name MATCHES "^chaistl\\.HeapPolicy" OR
         test_name MATCHES "^chaistl\\.HeapProperty" OR
         test_name MATCHES "^chaistl\\.MutableHeap" OR
         test_name MATCHES "^chaistl\\.HeapExceptionContract")
    list(APPEND labels containers heap policy heap_policy)
  elseif(test_name MATCHES "^chaistl\\.RingBufferPolicy" OR
         test_name MATCHES "^chaistl\\.RingBufferOverwrite" OR
         test_name MATCHES "^chaistl\\.RingBufferReject")
    list(APPEND labels containers sequence ring_buffer policy overflow_policy)
  elseif(test_name MATCHES "^chaistl\\.Span")
    list(APPEND labels containers views span)
  elseif(test_name MATCHES "^chaistl\\.Set" OR
         test_name MATCHES "^chaistl\\.Map" OR
         test_name MATCHES "^chaistl\\.Multiset" OR
         test_name MATCHES "^chaistl\\.Multimap")
    list(APPEND labels containers associative tree)
  elseif(test_name MATCHES "^chaistl\\.TreePolicyProperty" OR
         test_name MATCHES "^chaistl\\.RbTreePolicy" OR
         test_name MATCHES "^chaistl\\.AvlTreePolicy" OR
         test_name MATCHES "^chaistl\\.TreapTreePolicy")
    list(APPEND labels containers tree policy tree_policy)
  endif()

  set_tests_properties("${test_name}" PROPERTIES LABELS "${labels}")
endforeach()

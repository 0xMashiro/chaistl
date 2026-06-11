if(NOT EMSCRIPTEN)
  message(STATUS "Skipping chaistl visualizer: configure with emcmake to build WebAssembly")
  return()
endif()

add_executable(chaistl_visualizer_wasm)
target_sources(chaistl_visualizer_wasm PRIVATE cpp/bindings.cpp cpp/vector_scenarios.cpp)

target_link_libraries(chaistl_visualizer_wasm PRIVATE chaistl::chaistl)
target_compile_features(chaistl_visualizer_wasm PRIVATE cxx_std_23)

set_target_properties(chaistl_visualizer_wasm PROPERTIES
  OUTPUT_NAME "chaistl_visualizer"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web/public/wasm")

target_link_options(chaistl_visualizer_wasm PRIVATE
  --bind
  -sMODULARIZE=1
  -sEXPORT_NAME=createChaistlVisualizer
  -sENVIRONMENT=web
  -sALLOW_MEMORY_GROWTH=1)
